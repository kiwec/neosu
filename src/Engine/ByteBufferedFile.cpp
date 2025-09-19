#include "ByteBufferedFile.h"

#include "Engine.h"
#include "ModFlags.h"
#include <system_error>
#include <cassert>

ByteBufferedFile::Reader::Reader(const UString &uPath) : buffer(READ_BUFFER_SIZE) {
    auto path = std::filesystem::path(uPath.plat_str());
    this->file.open(path, std::ios::binary);
    if(!this->file.is_open()) {
        this->set_error("Failed to open file for reading: " + std::generic_category().message(errno));
        debugLog("Failed to open '{:s}': {:s}\n", path.string().c_str(),
                 std::generic_category().message(errno).c_str());
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
    debugLog("Failed to initialize file reader '{:s}': {:s}\n", path.string().c_str(),
             std::generic_category().message(errno).c_str());
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
        debugLog("WARNING: Expected 32 bytes for hash, got {}!\n", len);
        extra = len - 32;
        len = 32;
    }

    assert(len <= 32);  // shut up gcc PLEASE
    if(this->read_bytes(reinterpret_cast<u8 *>(hash.string()), len) != len) {
        // just continue, don't set error flag
        debugLog("WARNING: failed to read {} bytes to obtain hash.\n", len);
        extra = len;
    }
    this->skip_bytes(extra);
    hash.hash[len] = '\0';
    return hash;
}

Replay::Mods ByteBufferedFile::Reader::read_mods() {
    Replay::Mods mods;

    mods.flags = this->read<u64>();
    mods.speed = this->read<f32>();
    mods.notelock_type = this->read<i32>();
    mods.ar_override = this->read<f32>();
    mods.ar_overridenegative = this->read<f32>();
    mods.cs_override = this->read<f32>();
    mods.cs_overridenegative = this->read<f32>();
    mods.hp_override = this->read<f32>();
    mods.od_override = this->read<f32>();
    using namespace ModMasks;
    using namespace Replay::ModFlags;
    if(eq(mods.flags, Autopilot)) {
        mods.autopilot_lenience = this->read<f32>();
    }
    if(eq(mods.flags, Timewarp)) {
        mods.timewarp_multiplier = this->read<f32>();
    }
    if(eq(mods.flags, Minimize)) {
        mods.minimize_multiplier = this->read<f32>();
    }
    if(eq(mods.flags, ARTimewarp)) {
        mods.artimewarp_multiplier = this->read<f32>();
    }
    if(eq(mods.flags, ARWobble)) {
        mods.arwobble_strength = this->read<f32>();
        mods.arwobble_interval = this->read<f32>();
    }
    if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
        mods.wobble_strength = this->read<f32>();
        mods.wobble_frequency = this->read<f32>();
        mods.wobble_rotation_speed = this->read<f32>();
    }
    if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
        mods.jigsaw_followcircle_radius_factor = this->read<f32>();
    }
    if(eq(mods.flags, Shirone)) {
        mods.shirone_combo = this->read<f32>();
    }

    return mods;
}

std::string ByteBufferedFile::Reader::read_string() {
    if(this->error_flag) {
        return {};
    }

    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return {};

    u32 len = this->read_uleb128();
    std::string str_out;
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

ByteBufferedFile::Writer::Writer(const UString &uPath) : buffer(WRITE_BUFFER_SIZE) {
    auto path = std::filesystem::path(uPath.plat_str());
    this->file_path = path;
    this->tmp_file_path = this->file_path;
    this->tmp_file_path += ".tmp";

    this->file.open(this->tmp_file_path, std::ios::binary);
    if(!this->file.is_open()) {
        this->set_error("Failed to open file for writing: " + std::generic_category().message(errno));
        debugLog("Failed to open '{:s}': {:s}\n", path.string().c_str(),
                 std::generic_category().message(errno).c_str());
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
                debugLog("Failed to rename temporary file: {:s}\n", ec.message().c_str());
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

void ByteBufferedFile::Writer::write_hash(MD5Hash hash) {
    if(this->error_flag) {
        return;
    }

    this->write<u8>(0x0B);
    this->write<u8>(0x20);
    this->write_bytes(reinterpret_cast<u8 *>(hash.string()), 32);
}

void ByteBufferedFile::Writer::write_mods(Replay::Mods mods) {
    if(this->error_flag) {
        return;
    }

    this->write<u64>(mods.flags);
    this->write<f32>(mods.speed);
    this->write<i32>(mods.notelock_type);
    this->write<f32>(mods.ar_override);
    this->write<f32>(mods.ar_overridenegative);
    this->write<f32>(mods.cs_override);
    this->write<f32>(mods.cs_overridenegative);
    this->write<f32>(mods.hp_override);
    this->write<f32>(mods.od_override);
    using namespace ModMasks;
    using namespace Replay::ModFlags;
    if(eq(mods.flags, Autopilot)) {
        this->write<f32>(mods.autopilot_lenience);
    }
    if(eq(mods.flags, Timewarp)) {
        this->write<f32>(mods.timewarp_multiplier);
    }
    if(eq(mods.flags, Minimize)) {
        this->write<f32>(mods.minimize_multiplier);
    }
    if(eq(mods.flags, ARTimewarp)) {
        this->write<f32>(mods.artimewarp_multiplier);
    }
    if(eq(mods.flags, ARWobble)) {
        this->write<f32>(mods.arwobble_strength);
        this->write<f32>(mods.arwobble_interval);
    }
    if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
        this->write<f32>(mods.wobble_strength);
        this->write<f32>(mods.wobble_frequency);
        this->write<f32>(mods.wobble_rotation_speed);
    }
    if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
        this->write<f32>(mods.jigsaw_followcircle_radius_factor);
    }
    if(eq(mods.flags, Shirone)) {
        this->write<f32>(mods.shirone_combo);
    }
}

void ByteBufferedFile::Writer::write_string(std::string str) {
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
    this->write_bytes(reinterpret_cast<u8 *>(const_cast<char *>(str.c_str())), len);
}

void ByteBufferedFile::Writer::flush() {
    if(this->error_flag || !this->file.is_open()) {
        return;
    }

    this->file.write(reinterpret_cast<const char *>(this->buffer.data()), this->pos);
    if(this->file.fail()) {
        this->set_error("Failed to write to file: " + std::generic_category().message(errno));
        return;
    }
    this->pos = 0;
}

void ByteBufferedFile::Writer::write_bytes(u8 *bytes, size_t n) {
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

    memcpy(this->buffer.data() + this->pos, bytes, n);
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

void ByteBufferedFile::copy(const UString &from_uPath, const UString &to_uPath) {
    Reader from(from_uPath);
    Writer to(to_uPath);

    if(!from.good()) {
        debugLog("Failed to open source file for copying: {:s}\n", from.error().data());
        return;
    }

    if(!to.good()) {
        debugLog("Failed to open destination file for copying: {:s}\n", to.error().data());
        return;
    }

    std::vector<u8> buf(READ_BUFFER_SIZE);

    u32 remaining = from.total_size;
    while(remaining > 0 && from.good() && to.good()) {
        u32 len = std::min(remaining, static_cast<u32>(READ_BUFFER_SIZE));
        if(from.read_bytes(buf.data(), len) != len) {
            debugLog("Copy failed: could not read {} bytes, {} remaining\n", len, remaining);
            break;
        }
        to.write_bytes(buf.data(), len);
        remaining -= len;
    }

    if(!from.good()) {
        debugLog("Copy failed during read: {:s}\n", from.error().data());
    }
    if(!to.good()) {
        debugLog("Copy failed during write: {:s}\n", to.error().data());
    }
}
