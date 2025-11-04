#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.
#include "types.h"
#include "Replay.h"

struct pp_info;
class DatabaseBeatmap;

struct pp_calc_request {
    Replay::Mods mods;
    f32 AR{};
    f32 CS{};
    f32 OD{};
    i32 comboMax{-1};
    i32 numMisses{};
    i32 num300s{-1};
    i32 num100s{};
    i32 num50s{};
    bool rx{};
    bool td{};

    bool operator==(const pp_calc_request&) const = default;
};

// Set currently selected map. Clears pp cache. Pass NULL to init/reset.
void lct_set_map(const DatabaseBeatmap* map);

// Get pp for given parameters. Returns -1 pp values if not computed yet.
// Second parameter == true forces calculation during gameplay (hack)
pp_info lct_get_pp(const pp_calc_request& rqt, bool ignoreBGThreadPause = false);
