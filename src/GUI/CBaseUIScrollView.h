#pragma once
// Copyright (c) 2013, PG, All rights reserved.
#include "CBaseUIElement.h"
#include "Color.h"

class CBaseUIContainer;

class CBaseUIScrollView : public CBaseUIElement {
    NOCOPY_NOMOVE(CBaseUIScrollView)
   public:
    CBaseUIScrollView(f32 xPos = 0, f32 yPos = 0, f32 xSize = 0, f32 ySize = 0, const UString &name = "");
    ~CBaseUIScrollView() override;

    void invalidate();
    void freeElements();

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyUp(KeyboardEvent &e) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    // scrolling
    void scrollY(int delta, bool animated = true);
    void scrollX(int delta, bool animated = true);
    void scrollToY(int scrollPosY, bool animated = true);
    void scrollToX(int scrollPosX, bool animated = true);
    void scrollToElement(CBaseUIElement *element, int xOffset = 0, int yOffset = 0, bool animated = true);

    void scrollToLeft();
    void scrollToRight();
    void scrollToBottom();
    void scrollToTop();

    // set
    CBaseUIScrollView *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }
    CBaseUIScrollView *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUIScrollView *setDrawScrollbars(bool drawScrollbars) {
        this->bDrawScrollbars = drawScrollbars;
        return this;
    }

    CBaseUIScrollView *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }
    CBaseUIScrollView *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUIScrollView *setFrameBrightColor(Color frameBrightColor) {
        this->frameBrightColor = frameBrightColor;
        return this;
    }
    CBaseUIScrollView *setFrameDarkColor(Color frameDarkColor) {
        this->frameDarkColor = frameDarkColor;
        return this;
    }
    CBaseUIScrollView *setScrollbarColor(Color scrollbarColor) {
        this->scrollbarColor = scrollbarColor;
        return this;
    }

    CBaseUIScrollView *setHorizontalScrolling(bool horizontalScrolling) {
        this->bHorizontalScrolling = horizontalScrolling;
        return this;
    }
    CBaseUIScrollView *setVerticalScrolling(bool verticalScrolling) {
        this->bVerticalScrolling = verticalScrolling;
        return this;
    }
    CBaseUIScrollView *setScrollSizeToContent(int border = 5);
    CBaseUIScrollView *setScrollResistance(int scrollResistanceInPixels) {
        this->iScrollResistance = scrollResistanceInPixels;
        return this;
    }

    CBaseUIScrollView *setBlockScrolling(bool block) {
        this->bBlockScrolling = block;
        return this;
    }  // means: disable scrolling, not scrolling in 'blocks'

    void setScrollMouseWheelMultiplier(f32 scrollMouseWheelMultiplier) {
        this->fScrollMouseWheelMultiplier = scrollMouseWheelMultiplier;
    }
    void setScrollbarSizeMultiplier(f32 scrollbarSizeMultiplier) {
        this->fScrollbarSizeMultiplier = scrollbarSizeMultiplier;
    }

    // get
    [[nodiscard]] inline f64 getRelPosY() const { return this->vScrollPos.y; }
    [[nodiscard]] inline f64 getRelPosX() const { return this->vScrollPos.x; }
    [[nodiscard]] inline dvec2 getScrollSize() const { return this->vScrollSize; }
    [[nodiscard]] inline dvec2 getVelocity() const { return (this->vScrollPos - this->vVelocity); }

    [[nodiscard]] inline bool isAtBottom() const { return (this->vSize.y - this->vScrollPos.y) >= this->vScrollSize.y; }
    [[nodiscard]] inline bool isScrolling() const { return this->bScrolling; }
    bool isBusy() override;

    // events
    void onResized() override;
    void onMouseDownOutside(bool left = true, bool right = false) override;
    void onMouseDownInside(bool left = true, bool right = false) override;
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onMouseUpOutside(bool left = true, bool right = false) override;

    void onFocusStolen() override;
    void onEnabled() override;
    void onDisabled() override;

    // main container
    CBaseUIContainer *container;

   protected:
    void onMoved() override;

    // vars
    Color backgroundColor{0xff000000};
    Color frameColor{0xffffffff};
    Color frameBrightColor{0};
    Color frameDarkColor{0};
    Color scrollbarColor{0xaaffffff};

    dvec2 vScrollPos{1.f, 1.f};
    dvec2 vScrollPosBackup{0.f};
    vec2 vMouseBackup{0.f};

    f32 fScrollMouseWheelMultiplier{1.f};
    f32 fScrollbarSizeMultiplier{1.f};
    McRect verticalScrollbar;
    McRect horizontalScrollbar;

    // scroll logic
    dvec2 vScrollSize{1.f, 1.f};
    vec2 vMouseBackup2{0.f};
    vec2 vMouseBackup3{0.f};
    dvec2 vVelocity{0.f, 0.f};
    dvec2 vKineticAverage{0.f};

    int iPrevScrollDeltaX = 0;
    int iScrollResistance;

    unsigned bAutoScrollingX : 1 = false;
    unsigned bAutoScrollingY : 1 = false;

    unsigned bScrollResistanceCheck : 1 = false;
    unsigned bScrolling : 1 = false;
    unsigned bScrollbarScrolling : 1 = false;
    unsigned bScrollbarIsVerticalScrolling : 1 = false;
    unsigned bBlockScrolling : 1 = false;
    unsigned bHorizontalScrolling : 1 = false;
    unsigned bVerticalScrolling : 1 = true;
    unsigned bFirstScrollSizeToContent : 1 = true;

    // vars
    unsigned bDrawFrame : 1 = true;
    unsigned bDrawBackground : 1 = true;
    unsigned bDrawScrollbars : 1 = true;

   private:
    void updateClipping();
    void updateScrollbars();

    void scrollToYInt(int scrollPosY, bool animated = true, bool slow = true);
    void scrollToXInt(int scrollPosX, bool animated = true, bool slow = true);

   public:
    // When you scrolled to the bottom, and new content is added, setting this
    // to true makes it so you'll stay at the bottom.
    // Useful in places where you're waiting on new content, like chat logs.
    unsigned sticky : 1 = false;

    unsigned bHorizontalClipping : 1 = true;
    unsigned bVerticalClipping : 1 = true;
    unsigned bScrollbarOnLeft : 1 = false;
    unsigned bClippingDirty : 1 = true;  // start true for initial update
};
