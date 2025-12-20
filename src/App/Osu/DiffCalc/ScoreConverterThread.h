#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "types.h"
#include "noinclude.h"

struct MD5Hash;

class DatabaseBeatmap;
struct FinishedScore;
namespace ScoreConverter {
u32 get_computed();
u32 get_total();

// run calc on entire database scores
void start_calc();
void abort_calc();
bool running();

struct internal;
}  // namespace ScoreConverter
