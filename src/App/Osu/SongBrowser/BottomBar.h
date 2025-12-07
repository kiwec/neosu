#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.

#include "types.h"

class Graphics;

// Bottom bar has some hacky logic to handle osu!stable skins properly.
// Standard input handling logic won't work, as buttons can overlap.

namespace BottomBar {
enum Button : i8 { NONE = -1, MODE = 0, MODS = 1, RANDOM = 2, OPTIONS = 3 };

void update(bool* propagate_clicks);
void draw();
void press_button(Button btn);
[[nodiscard]] f32 get_height();
[[nodiscard]] f32 get_min_height();
}  // namespace BottomBar
