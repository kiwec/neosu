#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "ModFlags.h"

class DatabaseBeatmap;

namespace Replay {

template <typename T>
concept GenericReader = requires(T &t) {
    t.template read<u64>();
    t.template read<f32>();
    t.template read<i32>();
};

template <typename T>
concept GenericWriter = requires(T &t) {
    t.template write<u64>(u64{});
    t.template write<f32>(f32{});
    t.template write<i32>(i32{});
};

struct Mods {
    ModFlags flags{};

    f32 speed = 1.f;
    i32 notelock_type = 2;
    f32 autopilot_lenience = 0.75f;
    f32 ar_override = -1.f;
    f32 ar_overridenegative = 0.f;
    f32 cs_override = -1.f;
    f32 cs_overridenegative = 0.f;
    f32 hp_override = -1.f;
    f32 od_override = -1.f;
    f32 timewarp_multiplier = 1.5f;
    f32 minimize_multiplier = 0.5f;
    f32 artimewarp_multiplier = 0.5f;
    f32 arwobble_strength = 1.0f;
    f32 arwobble_interval = 7.0f;
    f32 wobble_strength = 25.f;
    f32 wobble_frequency = 1.f;
    f32 wobble_rotation_speed = 1.f;
    f32 jigsaw_followcircle_radius_factor = 0.f;
    f32 shirone_combo = 20.f;

    bool operator==(const Mods &) const = default;

    [[nodiscard]] inline bool has(ModFlags flag) const { return (this->flags & flag) == flag; }

    [[nodiscard]] LegacyFlags to_legacy() const;

    // Get AR/CS/OD, ignoring mods which change it over time
    // Used for ppv2 calculations.
    f32 get_naive_ar(const DatabaseBeatmap *map) const;
    f32 get_naive_cs(const DatabaseBeatmap *map) const;
    f32 get_naive_od(const DatabaseBeatmap *map) const;

    static Mods from_cvars();
    static Mods from_legacy(LegacyFlags legacy_flags);
    static void use(const Mods &mods);

    template <GenericReader R>
    static Mods unpack(R &reader) {
        Mods mods;

        mods.flags = static_cast<ModFlags>(reader.template read<u64>());
        mods.speed = reader.template read<f32>();
        mods.notelock_type = reader.template read<i32>();
        mods.ar_override = reader.template read<f32>();
        mods.ar_overridenegative = reader.template read<f32>();
        mods.cs_override = reader.template read<f32>();
        mods.cs_overridenegative = reader.template read<f32>();
        mods.hp_override = reader.template read<f32>();
        mods.od_override = reader.template read<f32>();
        using enum ModFlags;
        if(flags::has<Autopilot>(mods.flags)) {
            mods.autopilot_lenience = reader.template read<f32>();
        }
        if(flags::has<Timewarp>(mods.flags)) {
            mods.timewarp_multiplier = reader.template read<f32>();
        }
        if(flags::has<Minimize>(mods.flags)) {
            mods.minimize_multiplier = reader.template read<f32>();
        }
        if(flags::has<ARTimewarp>(mods.flags)) {
            mods.artimewarp_multiplier = reader.template read<f32>();
        }
        if(flags::has<ARWobble>(mods.flags)) {
            mods.arwobble_strength = reader.template read<f32>();
            mods.arwobble_interval = reader.template read<f32>();
        }
        if(flags::any<Wobble1 | Wobble2>(mods.flags)) {
            mods.wobble_strength = reader.template read<f32>();
            mods.wobble_frequency = reader.template read<f32>();
            mods.wobble_rotation_speed = reader.template read<f32>();
        }
        if(flags::any<Jigsaw1 | Jigsaw2>(mods.flags)) {
            mods.jigsaw_followcircle_radius_factor = reader.template read<f32>();
        }
        if(flags::has<Shirone>(mods.flags)) {
            mods.shirone_combo = reader.template read<f32>();
        }

        return mods;
    }

    template <GenericWriter W>
    static void pack_and_write(W &writer, const Replay::Mods &mods) {
        writer.template write<u64>(static_cast<u64>(mods.flags));
        writer.template write<f32>(mods.speed);
        writer.template write<i32>(mods.notelock_type);
        writer.template write<f32>(mods.ar_override);
        writer.template write<f32>(mods.ar_overridenegative);
        writer.template write<f32>(mods.cs_override);
        writer.template write<f32>(mods.cs_overridenegative);
        writer.template write<f32>(mods.hp_override);
        writer.template write<f32>(mods.od_override);
        using enum ModFlags;
        if(flags::has<Autopilot>(mods.flags)) {
            writer.template write<f32>(mods.autopilot_lenience);
        }
        if(flags::has<Timewarp>(mods.flags)) {
            writer.template write<f32>(mods.timewarp_multiplier);
        }
        if(flags::has<Minimize>(mods.flags)) {
            writer.template write<f32>(mods.minimize_multiplier);
        }
        if(flags::has<ARTimewarp>(mods.flags)) {
            writer.template write<f32>(mods.artimewarp_multiplier);
        }
        if(flags::has<ARWobble>(mods.flags)) {
            writer.template write<f32>(mods.arwobble_strength);
            writer.template write<f32>(mods.arwobble_interval);
        }
        if(flags::any<Wobble1 | Wobble2>(mods.flags)) {
            writer.template write<f32>(mods.wobble_strength);
            writer.template write<f32>(mods.wobble_frequency);
            writer.template write<f32>(mods.wobble_rotation_speed);
        }
        if(flags::any<Jigsaw1 | Jigsaw2>(mods.flags)) {
            writer.template write<f32>(mods.jigsaw_followcircle_radius_factor);
        }
        if(flags::has<Shirone>(mods.flags)) {
            writer.template write<f32>(mods.shirone_combo);
        }
    }
};

}  // namespace Replay
