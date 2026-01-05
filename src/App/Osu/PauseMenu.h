#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "OsuScreen.h"

class SongBrowser;
class CBaseUIContainer;
class UIPauseMenuButton;
class Image;
struct Skin;
struct BasicSkinImage;

class PauseMenu final : public OsuScreen {
   public:
    using ImageSkinMember = BasicSkinImage Skin::*;

    PauseMenu();

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onResolutionChange(vec2 newResolution) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void setContinueEnabled(bool continueEnabled);

   private:
    void updateLayout();

    void onContinueClicked();
    void onRetryClicked();
    void onBackClicked();

    void onSelectionChange();

    void scheduleVisibilityChange(bool visible);

    UIPauseMenuButton *addButton(ImageSkinMember getImageFunc, UString name);

    bool bScheduledVisibilityChange;
    bool bScheduledVisibility;

    std::vector<UIPauseMenuButton *> buttons;
    UIPauseMenuButton *selectedButton;
    float fWarningArrowsAnimStartTime;
    float fWarningArrowsAnimAlpha;
    float fWarningArrowsAnimX;
    float fWarningArrowsAnimY;
    bool bInitialWarningArrowFlyIn;

    bool bContinueEnabled;
    bool bClick1Down;
    bool bClick2Down;

    float fDimAnim;
};
