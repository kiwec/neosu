#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "ModFlags.h"
#include "UIScreen.h"
#include "Skin.h"

class SongBrowser;

class CBaseUIElement;
class CBaseUIContainer;
class CBaseUIScrollView;
class CBaseUIButton;
class CBaseUILabel;
class CBaseUISlider;
class CBaseUICheckbox;

class UIModSelectorModButton;
class ModSelectorOverrideSliderDescButton;
class UIButton;
class UICheckbox;

class ConVar;

class ModSelector final : public UIScreen {
    NOCOPY_NOMOVE(ModSelector)
   public:
    ModSelector();
    ~ModSelector() override;

    void draw() override;
    void update(CBaseUIEventCtx &c) override;

    void onKeyDown(KeyboardEvent &key) override;
    void onResolutionChange(vec2 newResolution) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void enableAuto();
    void toggleAuto();
    void resetModsUserInitiated();
    void resetMods();
    LegacyFlags getModFlags();
    void enableModsFromFlags(LegacyFlags flags);

    bool isInCompactMode();
    bool isCSOverrideSliderActive();
    bool isMouseInScrollView();
    bool isMouseInside() override;

    void updateButtons(bool initial = false);
    void updateExperimentalButtons();
    void updateOverrideSliderLabels();
    void updateScoreMultiplierLabelText();
    void updateLayout();
    void updateExperimentalLayout();

    CBaseUILabel *nonSubmittableWarning;
    UIModSelectorModButton *modButtonHalftime;
    UIModSelectorModButton *modButtonDoubletime;
    UIModSelectorModButton *modButtonAuto;

    CBaseUISlider *CSSlider;
    CBaseUISlider *ARSlider;
    CBaseUISlider *ODSlider;
    CBaseUISlider *HPSlider;
    CBaseUISlider *speedSlider;
    CBaseUICheckbox *ARLock;
    CBaseUICheckbox *ODLock;

    using SkinImageSkinMember = SkinImage *Skin::*;

   private:
    struct OVERRIDE_SLIDER {
        CBaseUICheckbox *lock{nullptr};
        ModSelectorOverrideSliderDescButton *desc{nullptr};
        CBaseUISlider *slider{nullptr};
        CBaseUILabel *label{nullptr};
        ConVar *cvar{nullptr};
        ConVar *lockCvar{nullptr};
    };

    struct EXPERIMENTAL_MOD {
        CBaseUIElement *element;
        ConVar *cvar;
    };

    UIModSelectorModButton *setModButtonOnGrid(int x, int y, int state, bool initialState, ConVar *modCvar,
                                               UString modName, const UString &tooltipText,
                                               SkinImageSkinMember getSkinImageMember);
    UIModSelectorModButton *getModButtonOnGrid(int x, int y);

    OVERRIDE_SLIDER addOverrideSlider(UString text, const UString &labelText, ConVar *cvar, float min, float max,
                                      UString tooltipText = {}, ConVar *lockCvar = nullptr);
    void onOverrideSliderChange(CBaseUISlider *slider);
    void onOverrideSliderLockChange(CBaseUICheckbox *checkbox);
    void onOverrideARSliderDescClicked(CBaseUIButton *button);
    void onOverrideODSliderDescClicked(CBaseUIButton *button);
    UString getOverrideSliderLabelText(const OVERRIDE_SLIDER &s, bool active);

    CBaseUILabel *addExperimentalLabel(const UString &text);
    UICheckbox *addExperimentalCheckbox(const UString &text, const UString &tooltipText, ConVar *cvar = nullptr);
    void onCheckboxChange(CBaseUICheckbox *checkbox);

    UIButton *addActionButton(const UString &text);

    void close(bool force);

    float fAnimation;
    float fExperimentalAnimation;
    bool bScheduledHide;
    bool bExperimentalVisible;
    CBaseUIContainer *overrideSliderContainer;
    CBaseUIScrollView *experimentalContainer;

    bool bWaitForCSChangeFinished;
    bool bWaitForSpeedChangeFinished;
    bool bWaitForHPChangeFinished;

    // override sliders
    std::vector<OVERRIDE_SLIDER> overrideSliders;
    bool bShowOverrideSliderALTHint;

    // mod grid buttons
    int iGridWidth;
    int iGridHeight;
    std::vector<UIModSelectorModButton *> modButtons;
    UIModSelectorModButton *modButtonEasy;
    UIModSelectorModButton *modButtonNofail;
    UIModSelectorModButton *modButtonHardrock;
    UIModSelectorModButton *modButtonSuddendeath;
    UIModSelectorModButton *modButtonHidden;
    UIModSelectorModButton *modButtonFlashlight;
    UIModSelectorModButton *modButtonRelax;
    UIModSelectorModButton *modButtonAutopilot;
    UIModSelectorModButton *modButtonSpunout;
    UIModSelectorModButton *modButtonScoreV2;
    UIModSelectorModButton *modButtonTD;

    // experimental mods
    std::vector<EXPERIMENTAL_MOD> experimentalMods;

    // score multiplier info label
    CBaseUILabel *scoreMultiplierLabel;

    // action buttons
    std::vector<UIButton *> actionButtons;
    UIButton *resetModsButton;
    UIButton *closeButton;
};
