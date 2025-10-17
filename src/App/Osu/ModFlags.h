#pragma once

#include "types.h"

namespace ModMasks {
static inline bool eq(u64 flags, u64 mod) { return (flags & mod) == mod; }
static inline bool legacy_eq(u32 legacy_flags, u32 legacy_mod) { return (legacy_flags & legacy_mod) == legacy_mod; }
}  // namespace ModMasks

namespace ModFlags {
enum : u64 {
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
    Singletap = 1ULL << 43,
    NoKeylock = 1ULL << 44,

    // Non-submittable
    NoHP = 1ULL << 62,
    Autoplay = 1ULL << 63
};
}

namespace LegacyFlags {
enum : u32 {
    NoFail = 1 << 0,
    Easy = 1 << 1,
    TouchDevice = 1 << 2,
    Hidden = 1 << 3,
    HardRock = 1 << 4,
    SuddenDeath = 1 << 5,
    DoubleTime = 1 << 6,
    Relax = 1 << 7,
    HalfTime = 1 << 8,
    Nightcore = DoubleTime | (1 << 9),
    Flashlight = 1 << 10,
    Autoplay = 1 << 11,
    SpunOut = 1 << 12,
    Autopilot = 1 << 13,
    Perfect = SuddenDeath | (1 << 14),
    Key4 = 1 << 15,
    Key5 = 1 << 16,
    Key6 = 1 << 17,
    Key7 = 1 << 18,
    Key8 = 1 << 19,
    FadeIn = 1 << 20,
    Random = 1 << 21,
    Cinema = 1 << 22,
    Target = 1 << 23,
    Key9 = 1 << 24,
    KeyCoop = 1 << 25,
    Key1 = 1 << 26,
    Key3 = 1 << 27,
    Key2 = 1 << 28,
    ScoreV2 = 1 << 29,
    Mirror = 1 << 30,

    Nightmare = Cinema,
    FPoSu = Key1,
};
}
