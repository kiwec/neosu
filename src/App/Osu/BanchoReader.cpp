// Copyright (c) 2023, kiwec, All rights reserved.

#include "BanchoReader.h"

#include <cstdlib>
#include <cstring>
#include <cassert>

#include "ModFlags.h"

namespace BANCHO::Proto {
void read_bytes(Packet &packet, u8 *bytes, size_t n) {
    if(packet.pos + n > packet.size) {
        packet.pos = packet.size + 1;
    } else {
        memcpy(bytes, packet.memory + packet.pos, n);
        packet.pos += n;
    }
}

u32 read_uleb128(Packet &packet) {
    u32 result = 0;
    u32 shift = 0;
    u8 byte = 0;

    do {
        byte = read<u8>(packet);
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while(byte & 0x80);

    return result;
}

UString read_string(Packet &packet) {
    u8 empty_check = read<u8>(packet);
    if(empty_check == 0) return {""};

    u32 len = read_uleb128(packet);
    u8 *str = new u8[len + 1];
    read_bytes(packet, str, len);
    str[len] = '\0';

    auto ustr = UString((const char *)str);
    delete[] str;

    return ustr;
}

std::string read_stdstring(Packet &packet) {
    u8 empty_check = read<u8>(packet);
    if(empty_check == 0) return {};

    u32 len = read_uleb128(packet);
    u8 *str = new u8[len + 1];
    read_bytes(packet, str, len);

    std::string str_out((const char *)str, len);
    delete[] str;

    return str_out;
}

MD5Hash read_hash(Packet &packet) {
    MD5Hash hash;

    u8 empty_check = read<u8>(packet);
    if(empty_check == 0) return hash;

    u32 len = read_uleb128(packet);
    if(len > 32) {
        len = 32;
    }

    read_bytes(packet, (u8 *)hash.string(), len);
    hash.hash[len] = '\0';
    return hash;
}

Replay::Mods read_mods(Packet &packet) {
    Replay::Mods mods;

    mods.flags = read<u64>(packet);
    mods.speed = read<f32>(packet);
    mods.notelock_type = read<i32>(packet);
    mods.ar_override = read<f32>(packet);
    mods.ar_overridenegative = read<f32>(packet);
    mods.cs_override = read<f32>(packet);
    mods.cs_overridenegative = read<f32>(packet);
    mods.hp_override = read<f32>(packet);
    mods.od_override = read<f32>(packet);
    using namespace ModMasks;
    using namespace Replay::ModFlags;
    if(eq(mods.flags, Autopilot)) {
        mods.autopilot_lenience = read<f32>(packet);
    }
    if(eq(mods.flags, Timewarp)) {
        mods.timewarp_multiplier = read<f32>(packet);
    }
    if(eq(mods.flags, Minimize)) {
        mods.minimize_multiplier = read<f32>(packet);
    }
    if(eq(mods.flags, ARTimewarp)) {
        mods.artimewarp_multiplier = read<f32>(packet);
    }
    if(eq(mods.flags, ARWobble)) {
        mods.arwobble_strength = read<f32>(packet);
        mods.arwobble_interval = read<f32>(packet);
    }
    if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
        mods.wobble_strength = read<f32>(packet);
        mods.wobble_frequency = read<f32>(packet);
        mods.wobble_rotation_speed = read<f32>(packet);
    }
    if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
        mods.jigsaw_followcircle_radius_factor = read<f32>(packet);
    }
    if(eq(mods.flags, Shirone)) {
        mods.shirone_combo = read<f32>(packet);
    }

    return mods;
}

void skip_string(Packet &packet) {
    u8 empty_check = read<u8>(packet);
    if(empty_check == 0) {
        return;
    }

    u32 len = read_uleb128(packet);
    packet.pos += len;
}

void write_bytes(Packet &packet, u8 *bytes, size_t n) {
    assert(bytes != nullptr);

    if(packet.pos + n > packet.size) {
        packet.memory = (unsigned char *)realloc(packet.memory, packet.size + n + 4096);
        assert(packet.memory && "realloc failed");
        packet.size += n + 4096;
        if(!packet.memory) return;
    }

    memcpy(packet.memory + packet.pos, bytes, n);
    packet.pos += n;
}

void write_uleb128(Packet &packet, u32 num) {
    if(num == 0) {
        u8 zero = 0;
        write<u8>(packet, zero);
        return;
    }

    while(num != 0) {
        u8 next = num & 0x7F;
        num >>= 7;
        if(num != 0) {
            next |= 0x80;
        }
        write<u8>(packet, next);
    }
}

void write_string(Packet &packet, const char *str) {
    if(!str || str[0] == '\0') {
        u8 zero = 0;
        write<u8>(packet, zero);
        return;
    }

    u8 empty_check = 11;
    write<u8>(packet, empty_check);

    u32 len = strlen(str);
    write_uleb128(packet, len);
    write_bytes(packet, (u8 *)str, len);
}

void write_hash(Packet &packet, const MD5Hash &hash) {
    write<u8>(packet, 0x0B);
    write<u8>(packet, 0x20);
    write_bytes(packet, (u8 *)hash.string(), 32);
}

void write_mods(Packet &packet, const Replay::Mods &mods) {
    write<u64>(packet, mods.flags);
    write<f32>(packet, mods.speed);
    write<i32>(packet, mods.notelock_type);
    write<f32>(packet, mods.ar_override);
    write<f32>(packet, mods.ar_overridenegative);
    write<f32>(packet, mods.cs_override);
    write<f32>(packet, mods.cs_overridenegative);
    write<f32>(packet, mods.hp_override);
    write<f32>(packet, mods.od_override);
    using namespace ModMasks;
    using namespace Replay::ModFlags;
    if(eq(mods.flags, Autopilot)) {
        write<f32>(packet, mods.autopilot_lenience);
    }
    if(eq(mods.flags, Timewarp)) {
        write<f32>(packet, mods.timewarp_multiplier);
    }
    if(eq(mods.flags, Minimize)) {
        write<f32>(packet, mods.minimize_multiplier);
    }
    if(eq(mods.flags, ARTimewarp)) {
        write<f32>(packet, mods.artimewarp_multiplier);
    }
    if(eq(mods.flags, ARWobble)) {
        write<f32>(packet, mods.arwobble_strength);
        write<f32>(packet, mods.arwobble_interval);
    }
    if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
        write<f32>(packet, mods.wobble_strength);
        write<f32>(packet, mods.wobble_frequency);
        write<f32>(packet, mods.wobble_rotation_speed);
    }
    if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
        write<f32>(packet, mods.jigsaw_followcircle_radius_factor);
    }
    if(eq(mods.flags, Shirone)) {
        write<f32>(packet, mods.shirone_combo);
    }
}
}  // namespace BANCHO::Proto
