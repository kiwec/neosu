#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include <utility>
#include <atomic>

#include "CBaseUIButton.h"

class BeatmapCarousel;
class DatabaseBeatmap;
class SongBrowser;
class SongButton;
class UIContextMenu;

class CarouselButton : public CBaseUIButton {
    NOCOPY_NOMOVE(CarouselButton)
   public:
    CarouselButton(SongBrowser *songBrowser, UIContextMenu *contextMenu, float xPos, float yPos, float xSize,
                   float ySize, UString name);
    ~CarouselButton() override;
    void deleteAnimations();

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    virtual void updateLayoutEx();

    CarouselButton *setVisible(bool visible) override;

    void select(bool fireCallbacks = true, bool autoSelectBottomMostChild = true, bool wasParentSelected = true);
    void deselect();

    void resetAnimations();

    void setTargetRelPosY(float targetRelPosY);
    void setChildren(std::vector<SongButton *> children) { this->children = std::move(children); }
    void setOffsetPercent(float offsetPercent) { this->fOffsetPercent = offsetPercent; }
    void setHideIfSelected(bool hideIfSelected) { this->bHideIfSelected = hideIfSelected; }
    void setIsSearchMatch(bool isSearchMatch) { this->bIsSearchMatch.store(isSearchMatch, std::memory_order_release); }

    [[nodiscard]] vec2 getActualOffset() const;
    [[nodiscard]] inline vec2 getActualSize() const { return this->vSize - 2.f * this->getActualOffset(); }
    [[nodiscard]] inline vec2 getActualPos() const { return this->vPos + this->getActualOffset(); }
    inline std::vector<SongButton *> &getChildren() { return this->children; }

    [[nodiscard]] virtual DatabaseBeatmap *getDatabaseBeatmap() const { return nullptr; }
    [[nodiscard]] virtual Color getActiveBackgroundColor() const;
    [[nodiscard]] virtual Color getInactiveBackgroundColor() const;

    [[nodiscard]] inline bool isSelected() const { return this->bSelected; }
    [[nodiscard]] inline bool isHiddenIfSelected() const { return this->bHideIfSelected; }
    [[nodiscard]] inline bool isSearchMatch() const { return this->bIsSearchMatch.load(std::memory_order_acquire); }

   protected:
    void drawMenuButtonBackground();

    virtual void onSelected(bool /*wasSelected*/, bool /*autoSelectBottomMostChild*/, bool /*wasParentSelected*/) { ; }
    virtual void onRightMouseUpInside() { ; }

    SongBrowser *songBrowser;
    UIContextMenu *contextMenu;

    McFont *font;
    McFont *fontBold;

    bool bSelected;

    std::vector<SongButton *> children;

   private:
    static int marginPixelsX;
    static int marginPixelsY;
    static float lastHoverSoundTime;

    enum class MOVE_AWAY_STATE : uint8_t { MOVE_CENTER, MOVE_UP, MOVE_DOWN };

    void onClicked(bool left = true, bool right = false) override;
    void onMouseInside() override;
    void onMouseOutside() override;

    void setMoveAwayState(MOVE_AWAY_STATE moveAwayState, bool animate = true);

    bool bRightClick;
    bool bRightClickCheck;

    float fTargetRelPosY;
    float fScale;
    float fOffsetPercent;
    float fHoverOffsetAnimation;
    float fHoverMoveAwayAnimation;
    float fCenterOffsetAnimation;
    float fCenterOffsetVelocityAnimation;

    std::atomic<bool> bIsSearchMatch;

    bool bHideIfSelected;

    MOVE_AWAY_STATE moveAwayState;
};
