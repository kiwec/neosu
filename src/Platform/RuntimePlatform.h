// Copyright (c) 2025, WH, All rights reserved.
#pragma once

// not compile-time platform, what we are actually running on
namespace RuntimePlatform {
enum VERSION : unsigned short {
    // TODO: glibc versions? does it even matter? these are just here for completeness but aren't used atm
    LINUX = 1 << 0,
    WASM = 1 << 1,
    MACOS = 1 << 2,
    // windows version bitmask (current() & WIN will always be true on windows)
    WIN = 1 << 3,
    WIN_XP = 1 << 4,
    WIN_VISTA = 1 << 5,
    WIN_7 = 1 << 6,
    WIN_8 = 1 << 7,
    WIN_10 = 1 << 9,
    WIN_11 = 1 << 10,
    WIN_UNKNOWN = 1 << 11,
    WIN_WINE = 1 << 12,
};

VERSION current();
const char* current_string();

}  // namespace RuntimePlatform