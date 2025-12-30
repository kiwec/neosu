#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "CBaseUIScrollView.h"

class SongBrowser;

class BeatmapCarousel final : public CBaseUIScrollView {
    NOCOPY_NOMOVE(BeatmapCarousel)
   public:
    BeatmapCarousel(SongBrowser *browser, float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0,
                    const UString &name = "")
        : CBaseUIScrollView(xPos, yPos, xSize, ySize, name), browser_ptr(browser) {
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
    void mouse_update(bool *propagate_clicks) override;

    // if we are actually right click scrolling at a "noticeable" velocity, so that we can skip
    // drawing some things for elements which the user will probably not notice anyways (backgrounds)

    // 0.00005 seems to empirically be small enough that it's not noticeable
    [[nodiscard]] inline bool isActuallyRightClickScrolling(f64 relativeYVelocityThreshold = 0.00005) const {
        return this->rightClickScrollRelYVelocity > relativeYVelocityThreshold;
    }

   private:
    SongBrowser *browser_ptr;

    // updated at the end of mouse_update
    f64 rightClickScrollRelYVelocity{0.0};
};
