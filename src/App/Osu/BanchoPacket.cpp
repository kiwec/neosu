// Copyright (c) 2023, kiwec, All rights reserved.

#include "BanchoPacket.h"

#include <cstdlib>
#include <cstring>
#include <cassert>

#include "ModFlags.h"

void Packet::read_bytes(u8 *bytes, size_t n) {
    if(this->pos + n > this->size) {
        this->pos = this->size + 1;
    } else {
        memcpy(bytes, this->memory + this->pos, n);
        this->pos += n;
    }
}

u32 Packet::read_uleb128() {
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

UString Packet::read_string() {
    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return {""};

    u32 len = this->read_uleb128();
    u8 *str = new u8[len + 1];
    read_bytes(str, len);
    str[len] = '\0';

    auto ustr = UString((const char *)str);
    delete[] str;

    return ustr;
}

std::string Packet::read_stdstring() {
    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return {};

    u32 len = this->read_uleb128();
    u8 *str = new u8[len + 1];
    read_bytes(str, len);

    std::string str_out((const char *)str, len);
    delete[] str;

    return str_out;
}

MD5Hash Packet::read_hash() {
    MD5Hash hash;

    u8 empty_check = this->read<u8>();
    if(empty_check == 0) return hash;

    u32 len = this->read_uleb128();
    if(len > 32) {
        len = 32;
    }

    read_bytes((u8 *)hash.string(), len);
    hash.hash[len] = '\0';
    return hash;
}

Replay::Mods Packet::read_mods() {
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

void Packet::skip_string() {
    u8 empty_check = this->read<u8>();
    if(empty_check == 0) {
        return;
    }

    u32 len = this->read_uleb128();
    this->pos += len;
}

void Packet::write_bytes(u8 *bytes, size_t n) {
    assert(bytes != nullptr);

    if(this->pos + n > this->size) {
        this->memory = (unsigned char *)realloc(this->memory, this->size + n + 4096);
        assert(this->memory && "realloc failed");
        this->size += n + 4096;
        if(!this->memory) return;
    }

    memcpy(this->memory + this->pos, bytes, n);
    this->pos += n;
}

void Packet::write_uleb128(u32 num) {
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

void Packet::write_string(const char *str) {
    if(!str || str[0] == '\0') {
        u8 zero = 0;
        this->write<u8>(zero);
        return;
    }

    u8 empty_check = 11;
    this->write<u8>(empty_check);

    u32 len = strlen(str);
    this->write_uleb128(len);
    this->write_bytes((u8 *)str, len);
}

void Packet::write_hash(const MD5Hash &hash) {
    this->write<u8>(0x0B);
    this->write<u8>(0x20);
    this->write_bytes((u8 *)hash.string(), 32);
}

void Packet::write_mods(const Replay::Mods &mods) {
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
