#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "HUD.h"
#include "UIAvatar.h"

struct ScoreboardSlot {
    ScoreboardSlot(const SCORE_ENTRY& score, int index);
    ~ScoreboardSlot();

    void draw();
    void updateIndex(int new_index, bool is_player, bool animate);

    UIAvatar* avatar = nullptr;
    SCORE_ENTRY score;
    int index;
    float y = 0.f;
    float fAlpha = 0.f;
    float fFlash = 0.f;
    bool is_friend = false;
    bool was_visible = false;
};
