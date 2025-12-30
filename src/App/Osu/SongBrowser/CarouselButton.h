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

#define DEF_BUTTON_TYPE(ClassName, TypeID, ParentClass)                 \
    static constexpr TypeId TYPE_ID = TypeID;                           \
    [[nodiscard]] TypeId getTypeId() const override { return TYPE_ID; } \
    [[nodiscard]] bool isTypeOf(TypeId typeId) const override {         \
        return typeId == TYPE_ID || ParentClass::isTypeOf(typeId);      \
    }

class CarouselButton : public CBaseUIButton {
    NOCOPY_NOMOVE(CarouselButton)
   public:
    enum TypeId : uint8_t {
        CarouselButton_ = 0,
        CollectionButton_ = 1,
        SongButton_ = 2,
        SongDifficultyButton_ = 3,
    };

    // manual RTTI
    static constexpr TypeId TYPE_ID = CarouselButton_;
    [[nodiscard]] virtual TypeId getTypeId() const { return TYPE_ID; }
    [[nodiscard]] virtual bool isTypeOf(TypeId typeId) const { return typeId == TYPE_ID; }

    template <typename T>
    [[nodiscard]] bool isType() const {
        return isTypeOf(T::TYPE_ID);
    }
    template <typename T>
    T *as() {
        return isType<T>() ? static_cast<T *>(this) : nullptr;
    }
    template <typename T>
    const T *as() const {
        return isType<T>() ? static_cast<const T *>(this) : nullptr;
    }

   public:
    CarouselButton(UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize, UString name);
    ~CarouselButton() override;
    void deleteAnimations();

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    virtual void updateLayoutEx();

    CarouselButton *setVisible(bool visible) override;

    // i hate how difficult it is to understand a sequence of unnamed boolean arguments
    struct SelOpts {
        bool noCallbacks{false};
        bool noSelectBottomChild{false};
        bool parentUnselected{false};
    };
    void select(SelOpts opts = {false, false, false});
    void deselect();

    virtual void resetAnimations();

    void setTargetRelPosY(float targetRelPosY);

    void setChildren(std::vector<SongButton *> children);
    void addChild(SongButton *child);
    void addChildren(std::vector<SongButton *> children);

    inline void setOffsetPercent(float offsetPercent) { this->fOffsetPercent = offsetPercent; }
    inline void setHideIfSelected(bool hideIfSelected) { this->bHideIfSelected = hideIfSelected; }
    inline void setIsSearchMatch(bool isSearchMatch) {
        this->bIsSearchMatch.store(isSearchMatch, std::memory_order_relaxed);
    }

    [[nodiscard]] vec2 getActualOffset() const;
    [[nodiscard]] inline vec2 getActualSize() const { return this->vSize - 2.f * this->getActualOffset(); }
    [[nodiscard]] inline vec2 getActualPos() const { return this->vPos + this->getActualOffset(); }
    [[nodiscard]] inline std::vector<SongButton *> &getChildren() { return this->children; }
    [[nodiscard]] inline const std::vector<SongButton *> &getChildren() const { return this->children; }

    [[nodiscard]] virtual DatabaseBeatmap *getDatabaseBeatmap() const { return nullptr; }
    [[nodiscard]] virtual Color getActiveBackgroundColor() const;
    [[nodiscard]] virtual Color getInactiveBackgroundColor() const;

    [[nodiscard]] inline bool isSelected() const { return this->bSelected; }
    [[nodiscard]] inline bool isHiddenIfSelected() const { return this->bHideIfSelected; }
    [[nodiscard]] inline bool isSearchMatch() const { return this->bIsSearchMatch.load(std::memory_order_relaxed); }

   protected:
    void drawMenuButtonBackground();

    virtual void onSelected(bool /*wasSelected*/, SelOpts /*opts*/) { ; }

    virtual void onRightMouseUpInside() { ; }
    void onClicked(bool left = true, bool right = false) override;

    void onMouseInside() override;
    void onMouseOutside() override;

    enum class MOVE_AWAY_STATE : uint8_t { MOVE_CENTER, MOVE_UP, MOVE_DOWN };

    void setMoveAwayState(MOVE_AWAY_STATE moveAwayState, bool animate = true);

    static int marginPixelsX;
    static int marginPixelsY;
    static float lastHoverSoundTime;

    UIContextMenu *contextMenu;

    McFont *font;
    McFont *fontBold;

    std::vector<SongButton *> children;

    float fTargetRelPosY;
    float fScale;
    float fOffsetPercent;
    float fHoverOffsetAnimation;
    float fHoverMoveAwayAnimation;
    float fCenterOffsetAnimation;
    float fCenterOffsetVelocityAnimation;

    std::atomic<bool> bIsSearchMatch;

    bool bHideIfSelected;
    bool bRightClick;
    bool bRightClickCheck;
    bool bSelected;
    bool bChildrenNeedSorting{true};
    bool bWasAnimationEverStarted{false};

    MOVE_AWAY_STATE moveAwayState;
};
