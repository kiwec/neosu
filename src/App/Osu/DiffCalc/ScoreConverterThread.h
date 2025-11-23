#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "types.h"
#include "noinclude.h"

struct MD5Hash;

class DatabaseBeatmap;
struct FinishedScore;
class ScoreConverter final {
    NOCOPY_NOMOVE(ScoreConverter)
   public:
    ScoreConverter() = delete;
    ~ScoreConverter() = delete;

    static u32 get_computed();
    static u32 get_total();

    // run calc on entire database scores
    static void start_calc();
    static void abort_calc();
    static bool running();

   private:
    static void update_ppv2(const FinishedScore& score);
    static void runloop();
};
