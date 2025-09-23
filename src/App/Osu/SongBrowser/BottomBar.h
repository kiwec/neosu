#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.

#include "types.h"

class Graphics;

// Bottom bar has some hacky logic to handle osu!stable skins properly.
// Standard input handling logic won't work, as buttons can overlap.

void update_bottombar(bool* propagate_clicks);
void draw_bottombar();
void press_bottombar_button(i32 btn_index);
f32 get_bottombar_height();
f32 bottombar_get_min_height();
