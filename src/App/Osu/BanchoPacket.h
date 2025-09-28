#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.
#include "MD5Hash.h"
#include "types.h"

struct Packet {
    u16 id{0};
    u8 *memory{nullptr};
    size_t size{0};
    size_t pos{0};
    u8 *extra{nullptr};
    i32 extra_int{0};  // lazy

    void reserve(u32 newsize) {
        if(newsize <= this->size) return;
        this->memory = (u8 *)realloc(this->memory, newsize);
        this->size = newsize;
    }

    void read_bytes(u8 *bytes, size_t n);
    u32 read_uleb128();
    std::string read_stdstring();
    inline UString read_string() {return {this->read_stdstring().c_str()};}
    void skip_string();
    MD5Hash read_hash();

    template <typename T>
    T read() {
        T result{};
        if(this->pos + sizeof(T) > this->size) {
            this->pos = this->size + 1;
            return result;
        } else {
            memcpy(&result, this->memory + this->pos, sizeof(T));
            this->pos += sizeof(T);
            return result;
        }
    }

    void write_bytes(u8 *bytes, size_t n);
    void write_uleb128(u32 num);
    void write_string(const char *str);
    void write_hash(const MD5Hash &hash);

    template <typename T>
    void write(T t) {
        this->write_bytes((u8 *)&t, sizeof(T));
    }
};
