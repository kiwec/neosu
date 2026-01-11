#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "CBaseUIScrollView.h"

class SongBrowser;

class BeatmapCarousel final : public CBaseUIScrollView {
    NOCOPY_NOMOVE(BeatmapCarousel)
   public:
    BeatmapCarousel(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, const UString &name = "")
        : CBaseUIScrollView(xPos, yPos, xSize, ySize, name) {
        this->setDrawBackground(false);
        this->setDrawFrame(false);
        this->setHorizontalScrolling(false);
        this->setScrollResistance(15);
    }
    ~BeatmapCarousel() override;

    void onKeyUp(KeyboardEvent &e) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void draw() override;
    void update() override;

    // checks for context menu visibility
    bool isMouseInside() override;

    // if we are actually scrolling at a "noticeable" velocity, so that we can skip
    // drawing some things for elements which the user will probably not notice anyways (backgrounds)
    [[nodiscard]] inline bool isScrollingFast() const { return this->bIsScrollingFast; }

   private:
    // updated at the end of update()
    bool bIsScrollingFast{false};
};
