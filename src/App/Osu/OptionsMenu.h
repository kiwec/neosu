#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "NotificationOverlay.h"
#include "ScreenBackable.h"
#include "VolumeOverlay.h"

#include <atomic>

class UIButton;
class UISlider;
class UIContextMenu;
class UISearchOverlay;

class OptionsMenuSliderPreviewElement;
class OptionsMenuCategoryButton;
class OptionsMenuKeyBindButton;
class OptionsMenuResetButton;

class CBaseUIContainer;
class CBaseUIImageButton;
class CBaseUICheckbox;
class CBaseUIButton;
class CBaseUISlider;
class CBaseUIScrollView;
class CBaseUILabel;
class CBaseUITextbox;

class ConVar;

class OptionsMenu final : public ScreenBackable, public NotificationOverlayKeyListener {
    friend bool VolumeOverlay::canChangeVolume();  // for contextMenu
    NOCOPY_NOMOVE(OptionsMenu)
   public:
    OptionsMenu();
    ~OptionsMenu() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onResolutionChange(vec2 newResolution) override;

    void onKey(KeyboardEvent &e) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void save();

    void openAndScrollToSkinSection();

    void setFullscreen(bool fullscreen) { this->bFullscreen = fullscreen; }

    void setUsername(UString username);

    [[nodiscard]] inline bool isFullscreen() const { return this->bFullscreen; }
    bool isMouseInside() override;
    bool isBusy() override;

    // "illegal", used by BassSoundEngine, can easily be turned into callbacks like for SoLoud instead
    void scheduleLayoutUpdate() { this->bSearchLayoutUpdateScheduled = true; }
    void onOutputDeviceResetUpdate();
    CBaseUISlider *asioBufferSizeSlider = nullptr;
    CBaseUILabel *outputDeviceLabel;

    // used by Osu
    void onSkinSelect();

    // used by Chat
    void askForLoginDetails();

    // used by networking stuff
    void update_login_button(bool loggedIn = false);

    // used by WindowsMain for osk handling (this needs to be moved...)
    void updateSkinNameLabel();

    CBaseUITextbox *osuFolderTextbox;

   private:
    struct RenderCondition {
        enum r : uint8_t { NONE, ASIO_ENABLED, WASAPI_ENABLED, SCORE_SUBMISSION_POLICY, PASSWORD_AUTH } rc{NONE};
        std::function<bool()> shouldrender{nullptr};

        RenderCondition() = default;
        RenderCondition(r rc) : rc(rc) {}
        RenderCondition(std::function<bool()> shouldrender) : shouldrender(std::move(shouldrender)) {}

        ~RenderCondition() = default;
        RenderCondition &operator=(const RenderCondition &) = default;
        RenderCondition &operator=(RenderCondition &&) = default;
        RenderCondition(const RenderCondition &) = default;
        RenderCondition(RenderCondition &&) = default;

        auto operator()() { return this->shouldrender ? this->shouldrender() : true; }

        bool operator==(const RenderCondition &other) const { return this->rc != NONE && this->rc == other.rc; };
        bool operator==(r cond) const { return this->rc != NONE && this->rc == cond; };
    };

    enum class OPTIONS_MENU_ELEMENT_TYPE : int8_t {
        INIT_ELEM_TYPE = -1,  // initial value
        SPCR = 0,             // spacer
        SECT = 1,             // section
        SUBSECT = 2,          // sub-section
        LABEL = 3,            // ...
        BTN = 4,              // button
        BINDBTN = 5,          // keybind button
        CBX = 6,              // checkbox
        SLDR = 7,             // slider
        TBX = 8,              // textbox
        SKNPRVW = 9,          // skin preview
        CBX_BTN = 10          // checkbox+button
    };

    using enum OptionsMenu::OPTIONS_MENU_ELEMENT_TYPE;

    struct OPTIONS_ELEMENT {
        OptionsMenuResetButton *resetButton{nullptr};
        std::vector<CBaseUIElement *> baseElems{};
        std::unordered_map<CBaseUIElement *, ConVar *> cvars{};
        OPTIONS_MENU_ELEMENT_TYPE type{OPTIONS_MENU_ELEMENT_TYPE::INIT_ELEM_TYPE};

        float label1Width{0.f};
        float relSizeDPI{96.f};

        RenderCondition render_condition{RenderCondition::NONE};

        bool allowOverscale{false};
        bool allowUnderscale{false};

        UString searchTags;
    };

    void updateLayout() override;
    void onBack() override;

    void setVisibleInt(bool visible, bool fromOnBack = false);
    void scheduleSearchUpdate();

    void updateFposuDPI();
    void updateFposuCMper360();
    void updateNotelockSelectLabel();

    // options
    void onFullscreenChange(CBaseUICheckbox *checkbox);
    void onDPIScalingChange(CBaseUICheckbox *checkbox);
    void onRawInputToAbsoluteWindowChange(CBaseUICheckbox *checkbox);
    void openCurrentSkinFolder();
    void onSkinSelect2(const UString &skinName, int id = -1);
    void onSkinReload();
    void onSkinRandom();
    void onResolutionSelect();
    void onResolutionSelect2(const UString &resolution, int id = -1);
    void onOutputDeviceSelect();
    void onOutputDeviceSelect2(const UString &outputDeviceName, int id = -1);
    void onOutputDeviceResetClicked();
    void onOutputDeviceRestart();
    void onLogInClicked(bool left, bool right);
    void onCM360CalculatorLinkClicked();
    void onNotelockSelect();
    void onNotelockSelect2(const UString &notelockType, int id = -1);
    void onNotelockSelectResetClicked();
    void onNotelockSelectResetUpdate();

    void onCheckboxChange(CBaseUICheckbox *checkbox);
    void onSliderChange(CBaseUISlider *slider);
    void onSliderChangeOneDecimalPlace(CBaseUISlider *slider);
    void onSliderChangeTwoDecimalPlaces(CBaseUISlider *slider);
    void onSliderChangeOneDecimalPlaceMeters(CBaseUISlider *slider);
    void onSliderChangeInt(CBaseUISlider *slider);
    void onSliderChangeIntMS(CBaseUISlider *slider);
    void onSliderChangeFloatMS(CBaseUISlider *slider);
    void onSliderChangePercent(CBaseUISlider *slider);
    void onFPSSliderChange(CBaseUISlider *slider);

    void onKeyBindingButtonPressed(CBaseUIButton *button);
    void onKeyUnbindButtonPressed(CBaseUIButton *button);
    void onKeyBindingsResetAllPressed(CBaseUIButton *button);
    void onSliderChangeSliderQuality(CBaseUISlider *slider);
    void onSliderChangeLetterboxingOffset(CBaseUISlider *slider);
    void onSliderChangeUIScale(CBaseUISlider *slider);

    void OpenASIOSettings();
    void onASIOBufferChange(CBaseUISlider *slider);
    void onWASAPIBufferChange(CBaseUISlider *slider);
    void onWASAPIPeriodChange(CBaseUISlider *slider);
    void onLoudnessNormalizationToggle(CBaseUICheckbox *checkbox);
    void onModChangingToggle(CBaseUICheckbox *checkbox);

    void onUseSkinsSoundSamplesChange();
    void onHighQualitySlidersCheckboxChange(CBaseUICheckbox *checkbox);
    void onHighQualitySlidersConVarChange(float newValue);

    // categories
    void onCategoryClicked(CBaseUIButton *button);

    // reset
    void onResetUpdate(CBaseUIButton *button);
    void onResetClicked(CBaseUIButton *button);
    void onResetEverythingClicked(CBaseUIButton *button);
    void onImportSettingsFromStable(CBaseUIButton *button);

    // categories
    OptionsMenuCategoryButton *addCategory(CBaseUIElement *section, char16_t icon);

    // elements
    void addSpacer();
    CBaseUILabel *addSection(const UString &text);
    CBaseUILabel *addSubSection(const UString &text, UString searchTags = "");
    CBaseUILabel *addLabel(const UString &text);
    UIButton *addButton(const UString &text);
    OPTIONS_ELEMENT *addButton(const UString &text, const UString &labelText, bool withResetButton = false);
    OPTIONS_ELEMENT *addButtonButton(const UString &text1, const UString &text2);
    OPTIONS_ELEMENT *addButtonButtonLabel(const UString &text1, const UString &text2, const UString &labelText,
                                          bool withResetButton = false);
    OptionsMenuKeyBindButton *addKeyBindButton(const UString &text, ConVar *cvar);
    CBaseUICheckbox *addCheckbox(const UString &text, ConVar *cvar);
    CBaseUICheckbox *addCheckbox(const UString &text, const UString &tooltipText = "", ConVar *cvar = nullptr);
    OPTIONS_ELEMENT *addButtonCheckbox(const UString &buttontext, const UString &cbxtooltip);
    UISlider *addSlider(const UString &text, float min = 0.0f, float max = 1.0f, ConVar *cvar = nullptr,
                        float label1Width = 0.0f, bool allowOverscale = false, bool allowUnderscale = false);
    CBaseUITextbox *addTextbox(UString text, ConVar *cvar = nullptr);
    CBaseUITextbox *addTextbox(UString text, const UString &labelText, ConVar *cvar = nullptr);
    CBaseUIElement *addSkinPreview();
    CBaseUIElement *addSliderPreview();

    // vars
    CBaseUIScrollView *categories;
    CBaseUIScrollView *options;
    UIContextMenu *contextMenu;
    UISearchOverlay *search;
    CBaseUILabel *spacer;
    OptionsMenuCategoryButton *fposuCategoryButton;
    std::vector<OptionsMenuCategoryButton *> categoryButtons;
    std::vector<OPTIONS_ELEMENT *> elemContainers;

    // custom
    bool bFullscreen;
    float fAnimation;

    CBaseUICheckbox *fullscreenCheckbox;
    CBaseUISlider *backgroundDimSlider;
    CBaseUISlider *backgroundBrightnessSlider;
    CBaseUISlider *hudSizeSlider;
    CBaseUISlider *hudComboScaleSlider;
    CBaseUISlider *hudScoreScaleSlider;
    CBaseUISlider *hudAccuracyScaleSlider;
    CBaseUISlider *hudHiterrorbarScaleSlider;
    CBaseUISlider *hudHiterrorbarURScaleSlider;
    CBaseUISlider *hudProgressbarScaleSlider;
    CBaseUISlider *hudScoreBarScaleSlider;
    CBaseUISlider *hudScoreBoardScaleSlider;
    CBaseUISlider *hudInputoverlayScaleSlider;
    CBaseUISlider *playfieldBorderSizeSlider;
    CBaseUISlider *statisticsOverlayScaleSlider;
    CBaseUISlider *statisticsOverlayXOffsetSlider;
    CBaseUISlider *statisticsOverlayYOffsetSlider;
    CBaseUISlider *cursorSizeSlider;
    CBaseUILabel *skinLabel;
    CBaseUIElement *skinSelectLocalButton;
    CBaseUIButton *resolutionSelectButton;
    CBaseUILabel *resolutionLabel;
    CBaseUIButton *outputDeviceSelectButton;
    OptionsMenuResetButton *outputDeviceResetButton;
    CBaseUISlider *wasapiBufferSizeSlider;
    CBaseUISlider *wasapiPeriodSizeSlider;
    OptionsMenuResetButton *asioBufferSizeResetButton;
    OptionsMenuResetButton *wasapiBufferSizeResetButton;
    OptionsMenuResetButton *wasapiPeriodSizeResetButton;
    CBaseUISlider *sliderQualitySlider;
    CBaseUISlider *letterboxingOffsetXSlider;
    CBaseUISlider *letterboxingOffsetYSlider;
    CBaseUIButton *letterboxingOffsetResetButton;
    OptionsMenuSliderPreviewElement *sliderPreviewElement;
    CBaseUITextbox *dpiTextbox;
    CBaseUITextbox *cm360Textbox;
    CBaseUIElement *skinSection;
    CBaseUISlider *uiScaleSlider;
    OptionsMenuResetButton *uiScaleResetButton;
    CBaseUIElement *notelockSelectButton;
    CBaseUILabel *notelockSelectLabel;
    OptionsMenuResetButton *notelockSelectResetButton;
    CBaseUIElement *hpDrainSelectButton;
    CBaseUILabel *hpDrainSelectLabel;
    OptionsMenuResetButton *hpDrainSelectResetButton;

    CBaseUIElement *sectionGeneral;
    CBaseUITextbox *serverTextbox;
    CBaseUICheckbox *submitScoresCheckbox;
    CBaseUITextbox *nameTextbox;
    CBaseUITextbox *passwordTextbox;
    UIButton *logInButton;

    ConVar *waitingKey = nullptr;

    float fOsuFolderTextboxInvalidAnim;
    float fVibrationStrengthExampleTimer;
    bool bLetterboxingOffsetUpdateScheduled;
    bool bUIScaleChangeScheduled;
    bool bUIScaleScrollToSliderScheduled;
    bool bDPIScalingScrollToSliderScheduled;
    bool bASIOBufferChangeScheduled;
    bool bWASAPIBufferChangeScheduled;
    bool bWASAPIPeriodChangeScheduled;
    std::atomic<bool> bSearchLayoutUpdateScheduled;

    int iNumResetAllKeyBindingsPressed;
    int iNumResetEverythingPressed;

    // search
    UString sSearchString{u""};
    float fSearchOnCharKeybindHackTime;

    // notelock
    std::vector<UString> notelockTypes;

    bool updating_layout = false;

    bool should_use_oauth_login();
};
