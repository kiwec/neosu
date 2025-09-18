#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "ConVar.h"
#include "ModSelector.h"
#include "MouseListener.h"
#include "Replay.h"
#include "score.h"

class AvatarManager;
class CWindowManager;
class ConVar;
class Image;
class McFont;
class RenderTarget;

class VolumeOverlay;
class UserCard;
class Chat;
class Lobby;
class RoomScreen;
class PromptScreen;
class UIUserContextMenuScreen;
class MainMenu;
class PauseMenu;
class OptionsMenu;
class SongBrowser;
class SpectatorScreen;
class BackgroundImageHandler;
class RankingScreen;
class UserStatsScreen;
class UpdateHandler;
class NotificationOverlay;
class TooltipOverlay;
class OsuScreen;
class Skin;
class HUD;
class Changelog;
class ModFPoSu;
class Playfield;

class Osu final : public MouseListener, public KeyboardListener {
    NOCOPY_NOMOVE(Osu)

   public:
    static constexpr const vec2 osuBaseResolution{640.0f, 480.0f};

    static float getImageScaleToFitResolution(Image *img, vec2 resolution);
    static float getImageScaleToFitResolution(vec2 size, vec2 resolution);
    static float getImageScaleToFillResolution(vec2 size, vec2 resolution);
    static float getImageScaleToFillResolution(Image *img, vec2 resolution);
    static float getImageScale(vec2 size, float osuSize);
    static float getImageScale(Image *img, float osuSize);
    static float getUIScale(float osuResolutionRatio);
    static float getUIScale();  // NOTE: includes premultiplied dpi scale!

    Osu();
    ~Osu() override;

    void draw();
    void update();
    bool isInPlayModeAndNotPaused();

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;
    void stealFocus();

    void onButtonChange(ButtonIndex button, bool down) override;

    void onResolutionChanged(vec2 newResolution);
    void onDPIChanged();

    void onFocusGained();
    void onFocusLost();
    inline void onRestored() { ; }
    void onMinimized();
    bool onShutdown();

    void onPlayEnd(FinishedScore score, bool quit = true, bool aborted = false);

    void toggleModSelection(bool waitForF1KeyUp = false);
    void toggleSongBrowser();
    void toggleOptionsMenu();
    void toggleChangelog();
    void toggleEditor();

    void saveScreenshot();

    void reloadSkin() { this->onSkinReload(); }

    static vec2 g_vInternalResolution;

    [[nodiscard]] inline vec2 getScreenSize() const { return g_vInternalResolution; }
    [[nodiscard]] inline int getScreenWidth() const { return (int)g_vInternalResolution.x; }
    [[nodiscard]] inline int getScreenHeight() const { return (int)g_vInternalResolution.y; }

    [[nodiscard]] inline OptionsMenu *getOptionsMenu() const { return this->optionsMenu; }
    [[nodiscard]] inline SongBrowser *getSongBrowser() const { return this->songBrowser2; }
    [[nodiscard]] inline BackgroundImageHandler *getBackgroundImageHandler() const {
        return this->backgroundImageHandler;
    }
    [[nodiscard]] inline Skin *getSkin() const { return this->skin; }
    [[nodiscard]] inline HUD *getHUD() const { return this->hud; }
    [[nodiscard]] inline TooltipOverlay *getTooltipOverlay() const { return this->tooltipOverlay; }
    [[nodiscard]] inline ModSelector *getModSelector() const { return this->modSelector; }
    [[nodiscard]] inline ModFPoSu *getFPoSu() const { return this->fposu; }
    [[nodiscard]] inline PauseMenu *getPauseMenu() const { return this->pauseMenu; }
    [[nodiscard]] inline MainMenu *getMainMenu() const { return this->mainMenu; }
    [[nodiscard]] inline RankingScreen *getRankingScreen() const { return this->rankingScreen; }
    [[nodiscard]] inline LiveScore *getScore() const { return this->score; }
    [[nodiscard]] inline UpdateHandler *getUpdateHandler() const { return this->updateHandler; }
    [[nodiscard]] inline const std::unique_ptr<AvatarManager> &getAvatarManager() const { return this->avatarManager; }

    [[nodiscard]] inline RenderTarget *getPlayfieldBuffer() const { return this->playfieldBuffer; }
    [[nodiscard]] inline RenderTarget *getSliderFrameBuffer() const { return this->sliderFrameBuffer; }
    [[nodiscard]] inline RenderTarget *getAAFrameBuffer() const { return this->AAFrameBuffer; }
    [[nodiscard]] inline RenderTarget *getFrameBuffer() const { return this->frameBuffer; }
    [[nodiscard]] inline RenderTarget *getFrameBuffer2() const { return this->frameBuffer2; }
    [[nodiscard]] inline McFont *getTitleFont() const { return this->titleFont; }
    [[nodiscard]] inline McFont *getSubTitleFont() const { return this->subTitleFont; }
    [[nodiscard]] inline McFont *getSongBrowserFont() const { return this->songBrowserFont; }
    [[nodiscard]] inline McFont *getSongBrowserFontBold() const { return this->songBrowserFontBold; }
    [[nodiscard]] inline McFont *getFontIcons() const { return this->fontIcons; }

    float getDifficultyMultiplier();
    float getCSDifficultyMultiplier();
    float getScoreMultiplier();
    float getAnimationSpeedMultiplier();

    [[nodiscard]] inline bool getModAuto() const { return cv::mod_autoplay.getBool(); }
    [[nodiscard]] inline bool getModAutopilot() const { return cv::mod_autopilot.getBool(); }
    [[nodiscard]] inline bool getModRelax() const { return cv::mod_relax.getBool(); }
    [[nodiscard]] inline bool getModSpunout() const { return cv::mod_spunout.getBool(); }
    [[nodiscard]] inline bool getModTarget() const { return cv::mod_target.getBool(); }
    [[nodiscard]] inline bool getModScorev2() const { return cv::mod_scorev2.getBool(); }
    [[nodiscard]] inline bool getModFlashlight() const { return cv::mod_flashlight.getBool(); }
    [[nodiscard]] inline bool getModNF() const { return cv::mod_nofail.getBool(); }
    [[nodiscard]] inline bool getModHD() const { return cv::mod_hidden.getBool(); }
    [[nodiscard]] inline bool getModHR() const { return cv::mod_hardrock.getBool(); }
    [[nodiscard]] inline bool getModEZ() const { return cv::mod_easy.getBool(); }
    [[nodiscard]] inline bool getModSD() const { return cv::mod_suddendeath.getBool(); }
    [[nodiscard]] inline bool getModSS() const { return cv::mod_perfect.getBool(); }
    [[nodiscard]] inline bool getModNightmare() const { return cv::mod_nightmare.getBool(); }
    [[nodiscard]] inline bool getModTD() const {
        return cv::mod_touchdevice.getBool() || cv::mod_touchdevice_always.getBool();
    }

    [[nodiscard]] inline std::vector<ConVar *> getExperimentalMods() const { return this->experimentalMods; }

    bool isInPlayMode();
    [[nodiscard]] inline bool isSkinLoading() const {
        return this->bSkinLoadScheduled ||
               (this->skin && this->skinScheduledToLoad && this->skin != this->skinScheduledToLoad);
    }

    [[nodiscard]] inline bool isSkipScheduled() const { return this->bSkipScheduled; }
    [[nodiscard]] inline bool isSeeking() const { return this->bSeeking; }
    [[nodiscard]] inline float getQuickSaveTime() const { return this->fQuickSaveTime; }

    bool shouldFallBackToLegacySliderRenderer();  // certain mods or actions require Sliders to render dynamically
                                                  // (e.g. wobble or the CS override slider)

    inline void useMods(const FinishedScore &score) { Replay::Mods::use(score.mods); }

    void updateMods();
    void updateCursorVisibility();
    void updateConfineCursor();
    void updateOsuFolder();
    void updateMouseSettings();
    void updateWindowsKeyDisable();

    // im not sure why this is a change callback but im too scared to change it at this point
    inline void updateModsForConVarTemplate(float /*oldValue*/, float /*newValue*/) { this->updateMods(); }

    void rebuildRenderTargets();
    void reloadFonts();
    void fireResolutionChanged();

    // callbacks
    void onWindowedResolutionChanged(const UString &oldValue, const UString &args);
    void onInternalResolutionChanged(const UString &oldValue, const UString &args);

    void onSkinReload();
    void onSkinChange(const UString &newValue);
    void onAnimationSpeedChange();
    void updateAnimationSpeed();

    void onSpeedChange(const UString &newValue);
    void onThumbnailsToggle();

    void onPlayfieldChange();

    void onUIScaleChange(const UString &oldValue, const UString &newValue);
    void onUIScaleToDPIChange(const UString &oldValue, const UString &newValue);
    void onLetterboxingChange(const UString &oldValue, const UString &newValue);

    void onKey1Change(bool pressed, bool mouse);
    void onKey2Change(bool pressed, bool mouse);

    void onModMafhamChange();
    void onModFPoSuChange();
    void onModFPoSu3DChange();
    void onModFPoSu3DSpheresChange();
    void onModFPoSu3DSpheresAAChange();

    void onLetterboxingOffsetChange();

    void onUserCardChange(const UString &new_username);

    void setupSoloud();

    // interfaces
    Playfield *playfield{nullptr};
    VolumeOverlay *volumeOverlay{nullptr};
    MainMenu *mainMenu{nullptr};
    OptionsMenu *optionsMenu{nullptr};
    Chat *chat{nullptr};
    Lobby *lobby{nullptr};
    RoomScreen *room{nullptr};
    PromptScreen *prompt{nullptr};
    UIUserContextMenuScreen *user_actions{nullptr};
    SongBrowser *songBrowser2{nullptr};
    BackgroundImageHandler *backgroundImageHandler{nullptr};
    ModSelector *modSelector{nullptr};
    RankingScreen *rankingScreen{nullptr};
    UserStatsScreen *userStats{nullptr};
    PauseMenu *pauseMenu{nullptr};
    Skin *skin{nullptr};
    HUD *hud{nullptr};
    TooltipOverlay *tooltipOverlay{nullptr};
    NotificationOverlay *notificationOverlay{nullptr};
    LiveScore *score{nullptr};
    Changelog *changelog{nullptr};
    UpdateHandler *updateHandler{nullptr};
    ModFPoSu *fposu{nullptr};
    SpectatorScreen *spectatorScreen{nullptr};

    std::unique_ptr<UserCard> userButton{nullptr};

    std::vector<OsuScreen *> screens;

    // rendering
    RenderTarget *backBuffer{nullptr};
    RenderTarget *playfieldBuffer{nullptr};
    RenderTarget *sliderFrameBuffer{nullptr};
    RenderTarget *AAFrameBuffer{nullptr};
    RenderTarget *frameBuffer{nullptr};
    RenderTarget *frameBuffer2{nullptr};
    vec2 vInternalResolution{0.f};

    Shader *actual_flashlight_shader{nullptr};
    Shader *flashlight_shader{nullptr};

    vec2 flashlight_position{0.f};

    // mods
    std::vector<ConVar *> experimentalMods;
    Replay::Mods previous_mods{0};
    bool bModAutoTemp{false};  // when ctrl+clicking a map, the auto mod should disable itself after the map finishes

    // keys
    bool bF1{false};
    bool bUIToggleCheck{false};
    bool bScoreboardToggleCheck{false};
    bool bEscape{false};
    bool bKeyboardKey1Down{false};
    bool bKeyboardKey12Down{false};
    bool bKeyboardKey2Down{false};
    bool bKeyboardKey22Down{false};
    bool bMouseKey1Down{false};
    bool bMouseKey2Down{false};
    bool bSkipScheduled{false};
    bool bQuickRetryDown{false};
    float fQuickRetryTime{0.f};
    bool bSeekKey{false};
    bool bSeeking{false};
    bool bClickedSkipButton{false};
    float fPrevSeekMousePosX{-1.f};
    float fQuickSaveTime{0.f};

    // async toggles
    // TODO: this way of doing things is bullshit
    bool bToggleModSelectionScheduled{false};
    bool bToggleOptionsMenuScheduled{false};
    bool bOptionsMenuFullscreen{true};
    bool bToggleChangelogScheduled{false};
    bool bToggleEditorScheduled{false};

    // global resources
    std::vector<McFont *> fonts;
    McFont *titleFont{nullptr};
    McFont *subTitleFont{nullptr};
    McFont *songBrowserFont{nullptr};
    McFont *songBrowserFontBold{nullptr};
    McFont *fontIcons{nullptr};

    // debugging
    CWindowManager *windowManager{nullptr};

    // replay
    UString watched_user_name;
    i32 watched_user_id{0};

    // custom
    bool music_unpause_scheduled{false};
    bool bScheduleEndlessModNextBeatmap{false};
    bool bWasBossKeyPaused{false};
    bool bSkinLoadScheduled{false};
    bool bSkinLoadWasReload{false};
    Skin *skinScheduledToLoad{nullptr};
    bool bFontReloadScheduled{false};
    bool bFireResolutionChangedScheduled{false};
    bool bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled{false};
    std::atomic<bool> should_pause_background_threads{false};

   private:
    std::unique_ptr<AvatarManager> avatarManager{nullptr};
};

extern Osu *osu;
