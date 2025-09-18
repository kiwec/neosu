#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "DifficultyCalculator.h"  // for pp_info

class DatabaseBeatmap;

struct pp_calc_request {
    u32 mods_legacy;
    f32 speed;
    f32 AR;
    f32 CS;
    f32 OD;
    bool rx;
    bool td;
    i32 comboMax;
    i32 numMisses;
    i32 num300s;
    i32 num100s;
    i32 num50s;
};

// Set currently selected map. Clears pp cache. Pass NULL to init/reset.
void lct_set_map(DatabaseBeatmap* map);

// Get pp for given parameters. Returns -1 pp values if not computed yet.
pp_info lct_get_pp(pp_calc_request rqt);
