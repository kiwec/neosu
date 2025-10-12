#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "ModFlags.h"

class DatabaseBeatmap;

namespace Replay {

namespace ModFlags {
enum ModFlags_ : u64 {

    // Green mods
    NoFail = 1ULL << 0,
    Easy = 1ULL << 1,
    Autopilot = 1ULL << 2,
    Relax = 1ULL << 3,

    // Red mods
    Hidden = 1ULL << 4,
    HardRock = 1ULL << 5,
    Flashlight = 1ULL << 6,
    SuddenDeath = 1ULL << 7,
    Perfect = SuddenDeath | (1ULL << 8),
    Nightmare = 1ULL << 9,

    // Special mods
    NoPitchCorrection = 1ULL << 10,
    TouchDevice = 1ULL << 11,
    SpunOut = 1ULL << 12,
    ScoreV2 = 1ULL << 13,
    FPoSu = 1ULL << 14,
    Target = 1ULL << 15,

    // Experimental mods
    AROverrideLock = 1ULL << 16,
    ODOverrideLock = 1ULL << 17,
    Timewarp = 1ULL << 18,
    ARTimewarp = 1ULL << 19,
    Minimize = 1ULL << 20,
    Jigsaw1 = 1ULL << 21,
    Jigsaw2 = 1ULL << 22,
    Wobble1 = 1ULL << 23,
    Wobble2 = 1ULL << 24,
    ARWobble = 1ULL << 25,
    FullAlternate = 1ULL << 26,
    Shirone = 1ULL << 27,
    Mafham = 1ULL << 28,
    HalfWindow = 1ULL << 29,
    HalfWindowAllow300s = 1ULL << 30,
    Ming3012 = 1ULL << 31,
    No100s = 1ULL << 32,
    No50s = 1ULL << 33,
    MirrorHorizontal = 1ULL << 34,
    MirrorVertical = 1ULL << 35,
    FPoSu_Strafing = 1ULL << 36,
    FadingCursor = 1ULL << 37,
    FPS = 1ULL << 38,
    ReverseSliders = 1ULL << 39,
    Millhioref = 1ULL << 40,
    StrictTracking = 1ULL << 41,
    ApproachDifferent = 1ULL << 42,

    // Only here so that replays for plays without HP drain don't fail.
    NoHP = 1ULL << 43,

    // Non-submittable
    Autoplay = 1ULL << 63

};

}  // namespace ModFlags

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
    u64 flags = 0;

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

    [[nodiscard]] inline bool has(u64 flag) const { return !!(ModMasks::eq(this->flags, flag)); }

    [[nodiscard]] u32 to_legacy() const;

    // Get AR/CS/OD, ignoring mods which change it over time
    // Used for ppv2 calculations.
    f32 get_naive_ar(const DatabaseBeatmap *map) const;
    f32 get_naive_cs(const DatabaseBeatmap *map) const;
    f32 get_naive_od(const DatabaseBeatmap *map) const;

    static Mods from_cvars();
    static Mods from_legacy(u32 legacy_flags);
    static void use(const Mods &mods);

    template <GenericReader R>
    static Mods unpack(R &reader) {
        Mods mods;

        mods.flags = reader.template read<u64>();
        mods.speed = reader.template read<f32>();
        mods.notelock_type = reader.template read<i32>();
        mods.ar_override = reader.template read<f32>();
        mods.ar_overridenegative = reader.template read<f32>();
        mods.cs_override = reader.template read<f32>();
        mods.cs_overridenegative = reader.template read<f32>();
        mods.hp_override = reader.template read<f32>();
        mods.od_override = reader.template read<f32>();
        using namespace ModMasks;
        using namespace ModFlags;
        if(eq(mods.flags, Autopilot)) {
            mods.autopilot_lenience = reader.template read<f32>();
        }
        if(eq(mods.flags, Timewarp)) {
            mods.timewarp_multiplier = reader.template read<f32>();
        }
        if(eq(mods.flags, Minimize)) {
            mods.minimize_multiplier = reader.template read<f32>();
        }
        if(eq(mods.flags, ARTimewarp)) {
            mods.artimewarp_multiplier = reader.template read<f32>();
        }
        if(eq(mods.flags, ARWobble)) {
            mods.arwobble_strength = reader.template read<f32>();
            mods.arwobble_interval = reader.template read<f32>();
        }
        if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
            mods.wobble_strength = reader.template read<f32>();
            mods.wobble_frequency = reader.template read<f32>();
            mods.wobble_rotation_speed = reader.template read<f32>();
        }
        if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
            mods.jigsaw_followcircle_radius_factor = reader.template read<f32>();
        }
        if(eq(mods.flags, Shirone)) {
            mods.shirone_combo = reader.template read<f32>();
        }

        return mods;
    }

    template <GenericWriter W>
    static void pack_and_write(W &writer, const Replay::Mods &mods) {
        writer.template write<u64>(mods.flags);
        writer.template write<f32>(mods.speed);
        writer.template write<i32>(mods.notelock_type);
        writer.template write<f32>(mods.ar_override);
        writer.template write<f32>(mods.ar_overridenegative);
        writer.template write<f32>(mods.cs_override);
        writer.template write<f32>(mods.cs_overridenegative);
        writer.template write<f32>(mods.hp_override);
        writer.template write<f32>(mods.od_override);
        using namespace ModMasks;
        using namespace Replay::ModFlags;
        if(eq(mods.flags, Autopilot)) {
            writer.template write<f32>(mods.autopilot_lenience);
        }
        if(eq(mods.flags, Timewarp)) {
            writer.template write<f32>(mods.timewarp_multiplier);
        }
        if(eq(mods.flags, Minimize)) {
            writer.template write<f32>(mods.minimize_multiplier);
        }
        if(eq(mods.flags, ARTimewarp)) {
            writer.template write<f32>(mods.artimewarp_multiplier);
        }
        if(eq(mods.flags, ARWobble)) {
            writer.template write<f32>(mods.arwobble_strength);
            writer.template write<f32>(mods.arwobble_interval);
        }
        if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
            writer.template write<f32>(mods.wobble_strength);
            writer.template write<f32>(mods.wobble_frequency);
            writer.template write<f32>(mods.wobble_rotation_speed);
        }
        if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
            writer.template write<f32>(mods.jigsaw_followcircle_radius_factor);
        }
        if(eq(mods.flags, Shirone)) {
            writer.template write<f32>(mods.shirone_combo);
        }
    }
};

}  // namespace Replay
