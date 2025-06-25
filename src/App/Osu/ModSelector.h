#pragma once
#include "OsuScreen.h"

class SkinImage;
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

class ModSelector : public OsuScreen {
   public:
    ModSelector();
    ~ModSelector() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyDown(KeyboardEvent &key) override;
    void onKeyUp(KeyboardEvent &key) override;

    void onResolutionChange(Vector2 newResolution) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void enableAuto();
    void toggleAuto();
    void resetModsUserInitiated();
    void resetMods();
    u32 getModFlags();
    void enableModsFromFlags(u32 flags);

    void setWaitForF1KeyUp(bool waitForF1KeyUp) { this->bWaitForF1KeyUp = waitForF1KeyUp; }

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

    CBaseUILabel *nonVanillaWarning;
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

   private:
    struct OVERRIDE_SLIDER {
        OVERRIDE_SLIDER() {
            this->lock = NULL;
            this->desc = NULL;
            this->slider = NULL;
            this->label = NULL;
            this->cvar = NULL;
            this->lockCvar = NULL;
        }

        CBaseUICheckbox *lock;
        ModSelectorOverrideSliderDescButton *desc;
        CBaseUISlider *slider;
        CBaseUILabel *label;
        ConVar *cvar;
        ConVar *lockCvar;
    };

    struct EXPERIMENTAL_MOD {
        CBaseUIElement *element;
        ConVar *cvar;
    };

    UIModSelectorModButton *setModButtonOnGrid(int x, int y, int state, bool initialState, ConVar *modCvar,
                                               UString modName, UString tooltipText,
                                               std::function<SkinImage *()> getImageFunc);
    UIModSelectorModButton *getModButtonOnGrid(int x, int y);

    OVERRIDE_SLIDER addOverrideSlider(UString text, UString labelText, ConVar *cvar, float min, float max,
                                      UString tooltipText = "", ConVar *lockCvar = NULL);
    void onOverrideSliderChange(CBaseUISlider *slider);
    void onOverrideSliderLockChange(CBaseUICheckbox *checkbox);
    void onOverrideARSliderDescClicked(CBaseUIButton *button);
    void onOverrideODSliderDescClicked(CBaseUIButton *button);
    UString getOverrideSliderLabelText(OVERRIDE_SLIDER s, bool active);

    CBaseUILabel *addExperimentalLabel(UString text);
    UICheckbox *addExperimentalCheckbox(UString text, UString tooltipText, ConVar *cvar = NULL);
    void onCheckboxChange(CBaseUICheckbox *checkbox);

    UIButton *addActionButton(UString text);

    void close();

    float fAnimation;
    float fExperimentalAnimation;
    bool bScheduledHide;
    bool bExperimentalVisible;
    CBaseUIContainer *overrideSliderContainer;
    CBaseUIScrollView *experimentalContainer;

    bool bWaitForF1KeyUp;

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
