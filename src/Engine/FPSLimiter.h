#pragma once
// Copyright (c) 2025, WH, All rights reserved.

namespace FPSLimiter {
void limit_frames(int target_fps, bool precise_sleeps);
void reset();
};  // namespace FPSLimiter
