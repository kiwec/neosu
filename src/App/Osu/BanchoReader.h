#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.
#include "MD5Hash.h"
#include "Replay.h"
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
};

namespace BANCHO::Proto {

void read_bytes(Packet &packet, u8 *bytes, size_t n);
u32 read_uleb128(Packet &packet);
UString read_string(Packet &packet);
std::string read_stdstring(Packet &packet);
void skip_string(Packet &packet);
MD5Hash read_hash(Packet &packet);
Replay::Mods read_mods(Packet &packet);

template <typename T>
T read(Packet &packet) {
    T result{};
    if(packet.pos + sizeof(T) > packet.size) {
        packet.pos = packet.size + 1;
        return result;
    } else {
        memcpy(&result, packet.memory + packet.pos, sizeof(T));
        packet.pos += sizeof(T);
        return result;
    }
}

void write_bytes(Packet &packet, u8 *bytes, size_t n);
void write_uleb128(Packet &packet, u32 num);
void write_string(Packet &packet, const char *str);
void write_hash(Packet &packet, const MD5Hash &hash);
void write_mods(Packet &packet, const Replay::Mods &mods);

template <typename T>
void write(Packet &packet, T t) {
    write_bytes(packet, (u8 *)&t, sizeof(T));
}
}  // namespace BANCHO::Proto
