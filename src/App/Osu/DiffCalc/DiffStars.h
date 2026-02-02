#pragma once
// pre-calculated star ratings for common mod combinations

#include "ModFlags.h"
#include "types.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace DiffStars {

enum SPEEDS_ENUM : u8 { SPEEDS_MIN, _0_75 = SPEEDS_MIN, _0_8, _0_9, _1_0, _1_1, _1_2, _1_3, _1_4, _1_5, SPEEDS_NUM };
inline constexpr std::array SPEEDS{0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f};
static_assert(SPEEDS_NUM == SPEEDS.size());

// mod combo indices:
//  0 = None
//  1 = HR
//  2 = HD
//  3 = EZ
//  4 = HD|HR
//  5 = HD|EZ
inline constexpr uSz NUM_MOD_COMBOS = 6;
inline constexpr uSz NUM_PRECALC_RATINGS = SPEEDS_NUM * NUM_MOD_COMBOS;  // 54
inline constexpr uSz NOMOD_1X_INDEX = _1_0 * NUM_MOD_COMBOS;             // speed_idx=3 (1.0) * 6 + combo=0 (None) = 18

enum MOD_COMBO_INDEX : u8 { INVALID_MODCOMBO = 0xFF };
inline MOD_COMBO_INDEX mod_combo_index(ModFlags flags) {
    using namespace flags::operators;
    using enum ModFlags;

    // extract the 3 relevant bits into a key
    const u8 hr = flags::has<HardRock>(flags) << 0;
    const u8 hd = flags::has<Hidden>(flags) << 1;
    const u8 ez = flags::has<Easy>(flags) << 2;
    const u8 key = hr | hd | ez;

    static constexpr std::array LUT{0,      // 0: None
                                    1,      // 1: HR
                                    2,      // 2: HD
                                    4,      // 3: HR|HD
                                    3,      // 4: EZ
                                    0xFF,   // 5: EZ|HR (disallowed)
                                    5,      // 6: EZ|HD
                                    0xFF};  // 7: all (disallowed)

    return static_cast<MOD_COMBO_INDEX>(LUT[key]);
}

// never fail, return closest
inline uSz speed_index(f32 speed) {
    auto it = std::ranges::lower_bound(SPEEDS, speed);
    if(it == SPEEDS.begin()) return 0;
    if(it == SPEEDS.end()) return SPEEDS.size() - 1;

    auto prev = std::prev(it);
    return (speed - *prev <= *it - speed) ? std::distance(SPEEDS.begin(), prev) : std::distance(SPEEDS.begin(), it);
}

inline MOD_COMBO_INDEX index_of(ModFlags flags, f32 speed) {
    const uSz si = speed_index(speed);
    const uSz mi = mod_combo_index(flags);
    if(mi == INVALID_MODCOMBO) return INVALID_MODCOMBO;

    return static_cast<MOD_COMBO_INDEX>(si * NUM_MOD_COMBOS + mi);
}

using Ratings = std::array<f32, NUM_PRECALC_RATINGS>;

// currently active mod combination index, updated by Osu::updateMods()
inline u8 active_idx = NOMOD_1X_INDEX;

}  // namespace DiffStars
