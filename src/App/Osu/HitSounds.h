#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.
#include <string>
#include "types.h"

namespace SampleSetType {
enum {
    NORMAL = 1,
    SOFT = 2,
    DRUM = 3,
};
}

namespace HitSoundType {
enum {
    NORMAL = (1 << 0),
    WHISTLE = (1 << 1),
    FINISH = (1 << 2),
    CLAP = (1 << 3),

    /* non-ppy */
    TICK = (1 << 4),

    VALID_HITSOUNDS = NORMAL | WHISTLE | FINISH | CLAP,
    VALID_SLIDER_HITSOUNDS = NORMAL | WHISTLE,
};
}

struct HitSamples {
    u8 hitSounds = 0;           // bitfield of HitSoundTypes to play
    u8 normalSet = 0;           // SampleSetType of the normal sound
    u8 additionSet = 0;         // SampleSetType of the whistle, finish and clap sounds
    u8 volume = 0;              // volume of the sample, 1-100. if 0, use timing point volume instead
    i32 index = 0;              // index of the sample (for custom map sounds). if 0, use skin sound instead
    std::string filename = "";  // when not empty, ignore all the above mess (except volume) and just play that file

    void play(f32 pan, i32 delta, bool is_sliderslide = false);
    void stop();

    i32 getAdditionSet();
    i32 getNormalSet();
};
