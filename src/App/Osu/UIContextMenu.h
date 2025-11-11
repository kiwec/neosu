#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUIButton.h"
#include "CBaseUIScrollView.h"
#include "CBaseUITextbox.h"

class CBaseUIContainer;

class UIContextMenuButton;
class UIContextMenuTextbox;

class UIContextMenu final : public CBaseUIScrollView {
   public:
    static void clampToBottomScreenEdge(UIContextMenu *menu);
    static void clampToRightScreenEdge(UIContextMenu *menu);

   public:
    UIContextMenu(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, const UString &name = "",
                  CBaseUIScrollView *parent = nullptr);

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyUp(KeyboardEvent &e) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    using ButtonClickCallback = SA::delegate<void(const UString &, int)>;
    void setClickCallback(const ButtonClickCallback &clickCallback) { this->clickCallback = clickCallback; }

    void begin(int minWidth = 0, bool bigStyle = false);
    UIContextMenuButton *addButtonJustified(const UString &text, TEXT_JUSTIFICATION j = TEXT_JUSTIFICATION::CENTERED,
                                            int id = -1);
    inline UIContextMenuButton *addButton(const UString &text, int id = -1) {
        return this->addButtonJustified(text, TEXT_JUSTIFICATION::LEFT, id);
    };
    UIContextMenuTextbox *addTextbox(const UString &text, int id = -1);

    void end(bool invertAnimation, bool clampUnderflowAndOverflowAndEnableScrollingIfNecessary);

    CBaseUIElement *setVisible(bool visible) override {
        // HACHACK: this->bVisible is always true, since we want to be able to put a context menu in a scrollview.
        //          When scrolling, scrollviews call setVisible(false) to clip items, and that breaks the menu.
        (void)visible;
        return this;
    }
    void setVisible2(bool visible2);

    bool isVisible() override { return this->bVisible2; }

   private:
    void onMouseDownOutside(bool left = true, bool right = false) override;

    void onClick(CBaseUIButton *button);
    void onHitEnter(UIContextMenuTextbox *textbox);

    CBaseUIScrollView *parent = nullptr;
    UIContextMenuTextbox *containedTextbox = nullptr;
    ButtonClickCallback clickCallback = {};

    i32 iYCounter = 0;
    i32 iWidthCounter = 0;

    bool bVisible2 = false;
    f32 fAnimation = 0.f;
    bool bInvertAnimation = false;

    bool bBigStyle;
    bool bClampUnderflowAndOverflowAndEnableScrollingIfNecessary;

    std::vector<CBaseUIElement *> selfDeletionCrashWorkaroundScheduledElementDeleteHack;
};

class UIContextMenuButton final : public CBaseUIButton {
    NOCOPY_NOMOVE(UIContextMenuButton)
   public:
    UIContextMenuButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text, int id);
    ~UIContextMenuButton() override { ; }

    void mouse_update(bool *propagate_clicks) override;

    void onMouseInside() override;
    void onMouseDownInside(bool left = true, bool right = false) override;

    [[nodiscard]] inline int getID() const { return this->iID; }

    void setTooltipText(const UString &text);

   private:
    int iID;

    std::vector<UString> tooltipTextLines;
};

class UIContextMenuTextbox final : public CBaseUITextbox {
    NOCOPY_NOMOVE(UIContextMenuTextbox)
   public:
    UIContextMenuTextbox(float xPos, float yPos, float xSize, float ySize, UString name, int id);
    ~UIContextMenuTextbox() override { ; }

    [[nodiscard]] inline int getID() const { return this->iID; }

   private:
    int iID;
};
