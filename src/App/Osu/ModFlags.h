#pragma once

#include "types.h"

namespace ModMasks {
static inline bool eq(u64 flags, u64 mod) { return (flags & mod) == mod; }
static inline bool legacy_eq(u32 legacy_flags, u32 legacy_mod) { return (legacy_flags & legacy_mod) == legacy_mod; }
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
