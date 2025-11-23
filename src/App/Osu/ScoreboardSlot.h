#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "HUD.h"
#include "UIAvatar.h"

class ScoreboardSlot final {
    NOCOPY_NOMOVE(ScoreboardSlot)
   public:
    ScoreboardSlot(const SCORE_ENTRY& score, int index);
    ~ScoreboardSlot();

    void draw();
    void updateIndex(int new_index, bool is_player, bool animate);
    SCORE_ENTRY score;

   private:
    std::unique_ptr<UIAvatar> avatar{nullptr};

    int index;
    float y = 0.f;
    float fAlpha = 0.f;
    float fFlash = 0.f;
    bool is_friend = false;
    bool was_visible = false;
};
