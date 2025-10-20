// Copyright (c) 2024, kiwec & 2025, WH, All rights reserved.
#include "ByteBufferedFile.h"

#include "Logging.h"
#include "File.h"

#include <system_error>
#include <cassert>

ByteBufferedFile::Reader::Reader(std::string_view readPath)
    : buffer(std::make_unique_for_overwrite<u8[]>(READ_BUFFER_SIZE)) {
    auto path = File::getFsPath(readPath);
    this->file.open(path, std::ios::binary);
    if(!this->file.is_open()) {
        this->set_error("Failed to open file for reading: " + std::generic_category().message(errno));
        debugLog("Failed to open '{:s}': {:s}", readPath, std::generic_category().message(errno).c_str());
        return;
    }

    this->file.seekg(0, std::ios::end);
    if(this->file.fail()) {
        goto seek_error;
    }

    this->total_size = this->file.tellg();

    this->file.seekg(0, std::ios::beg);
    if(this->file.fail()) {
        goto seek_error;
    }

    return;  // success

seek_error:
    this->set_error("Failed to initialize file reader: " + std::generic_category().message(errno));
    debugLog("Failed to initialize file reader '{:s}': {:s}", readPath, std::generic_category().message(errno).c_str());
    this->file.close();
    return;
}

void ByteBufferedFile::Reader::set_error(const std::string &error_msg) {
    if(!this->error_flag) {  // only set first error
        this->error_flag = true;
        this->last_error = error_msg;
    }
}

MD5Hash ByteBufferedFile::Reader::read_hash() {
    MD5Hash hash;

    if(this->error_flag) {
        return hash;
    }

    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return hash;

    u32 len = this->read_uleb128();
    u32 extra = 0;
    if(len > 32) {
        // just continue, don't set error flag
        debugLog("WARNING: Expected 32 bytes for hash, got {}!", len);
        extra = len - 32;
        len = 32;
    }

    assert(len <= 32);  // shut up gcc PLEASE
    if(this->read_bytes(reinterpret_cast<u8 *>(hash.string()), len) != len) {
        // just continue, don't set error flag
        debugLog("WARNING: failed to read {} bytes to obtain hash.", len);
        extra = len;
    }
    this->skip_bytes(extra);
    hash.hash[len] = '\0';
    return hash;
}

std::string ByteBufferedFile::Reader::read_string() {
    if(this->error_flag) {
        return {};
    }

    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return {};

    u32 len = this->read_uleb128();
    static std::string str_out;
    str_out.resize(len);
    if(this->read_bytes(reinterpret_cast<u8 *>(str_out.data()), len) != len) {
        this->set_error("Failed to read " + std::to_string(len) + " bytes for string");
        return {};
    }

    return str_out;
}

u32 ByteBufferedFile::Reader::read_uleb128() {
    if(this->error_flag) {
        return 0;
    }

    u32 result = 0;
    u32 shift = 0;
    u8 byte = 0;

    do {
        byte = this->read<u8>();
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while(byte & 0x80);

    return result;
}

void ByteBufferedFile::Reader::skip_string() {
    if(this->error_flag) {
        return;
    }

    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return;

    u32 len = this->read_uleb128();
    this->skip_bytes(len);
}

ByteBufferedFile::Writer::Writer(std::string_view writePath)
    : buffer(std::make_unique_for_overwrite<u8[]>(WRITE_BUFFER_SIZE)) {
    auto path = File::getFsPath(writePath);
    this->file_path = path;
    this->tmp_file_path = this->file_path;
    this->tmp_file_path += ".tmp";

    this->file.open(this->tmp_file_path, std::ios::binary);
    if(!this->file.is_open()) {
        this->set_error("Failed to open file for writing: " + std::generic_category().message(errno));
        debugLog("Failed to open '{:s}': {:s}", writePath, std::generic_category().message(errno).c_str());
        return;
    }
}

ByteBufferedFile::Writer::~Writer() {
    if(this->file.is_open()) {
        this->flush();
        this->file.close();

        if(!this->error_flag) {
            std::error_code ec;
            std::filesystem::remove(this->file_path, ec);  // Windows (the Microsoft docs are LYING)
            std::filesystem::rename(this->tmp_file_path, this->file_path, ec);
            if(ec) {
                // can't set error in destructor, but log it
                debugLog("Failed to rename temporary file: {:s}", ec.message().c_str());
            }
        }
    }
}

void ByteBufferedFile::Writer::set_error(const std::string &error_msg) {
    if(!this->error_flag) {  // only set first error
        this->error_flag = true;
        this->last_error = error_msg;
    }
}

void ByteBufferedFile::Writer::write_hash(const MD5Hash &hash) {
    if(this->error_flag) {
        return;
    }

    this->write<u8>(0x0B);
    this->write<u8>(0x20);
    this->write_bytes(reinterpret_cast<const u8 *>(hash.string()), 32);
}

void ByteBufferedFile::Writer::write_string(const std::string &str) {
    if(this->error_flag) {
        return;
    }

    if(str[0] == '\0') {
        u8 zero = 0;
        this->write<u8>(zero);
        return;
    }

    u8 empty_check = 11;
    this->write<u8>(empty_check);

    u32 len = str.length();
    this->write_uleb128(len);
    this->write_bytes(reinterpret_cast<const u8 *>(str.c_str()), len);
}

void ByteBufferedFile::Writer::flush() {
    if(this->error_flag || !this->file.is_open()) {
        return;
    }

    this->file.write(reinterpret_cast<const char *>(&this->buffer[0]), this->pos);
    if(this->file.fail()) {
        this->set_error("Failed to write to file: " + std::generic_category().message(errno));
        return;
    }
    this->pos = 0;
}

void ByteBufferedFile::Writer::write_bytes(const u8 *bytes, uSz n) {
    if(this->error_flag || !this->file.is_open()) {
        return;
    }

    if(this->pos + n > WRITE_BUFFER_SIZE) {
        this->flush();
        if(this->error_flag) {
            return;
        }
    }

    if(this->pos + n > WRITE_BUFFER_SIZE) {
        this->set_error("Attempted to write " + std::to_string(n) + " bytes (exceeding buffer size " +
                        std::to_string(WRITE_BUFFER_SIZE) + ")");
        return;
    }

    memcpy(&this->buffer[this->pos], bytes, n);
    this->pos += n;
}

void ByteBufferedFile::Writer::write_uleb128(u32 num) {
    if(this->error_flag) {
        return;
    }

    if(num == 0) {
        u8 zero = 0;
        this->write<u8>(zero);
        return;
    }

    while(num != 0) {
        u8 next = num & 0x7F;
        num >>= 7;
        if(num != 0) {
            next |= 0x80;
        }
        this->write<u8>(next);
    }
}

void ByteBufferedFile::copy(std::string_view from_path, std::string_view to_path) {
    Reader from(from_path);
    if(!from.good()) {
        debugLog("Failed to open source file for copying: {:s}", from.error().data());
        return;
    }

    Writer to(to_path);
    if(!to.good()) {
        debugLog("Failed to open destination file for copying: {:s}", to.error().data());
        return;
    }

    std::vector<u8> buf(READ_BUFFER_SIZE);

    u32 remaining = from.total_size;
    while(remaining > 0 && from.good() && to.good()) {
        u32 len = std::min(remaining, static_cast<u32>(READ_BUFFER_SIZE));
        if(from.read_bytes(buf.data(), len) != len) {
            debugLog("Copy failed: could not read {} bytes, {} remaining", len, remaining);
            break;
        }
        to.write_bytes(buf.data(), len);
        remaining -= len;
    }

    if(!from.good()) {
        debugLog("Copy failed during read: {:s}", from.error().data());
    }
    if(!to.good()) {
        debugLog("Copy failed during write: {:s}", to.error().data());
    }
}
