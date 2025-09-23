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
class BeatmapInterface;

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
    static float getUIScale(float osuSize);
    static float getUIScale();  // NOTE: includes premultiplied dpi scale!

    Osu();
    ~Osu() override;

    void draw();
    void update();

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

    inline void reloadSkin() { this->onSkinReload(); }

    void reloadMapInterface();

    // threading-related
    [[nodiscard]] bool isInPlayModeAndNotPaused() const;
    [[nodiscard]] inline bool shouldPauseBGThreads() const {
        return this->pause_bg_threads.load(std::memory_order_acquire);
    }
    inline void setShouldPauseBGThreads(bool pause) { this->pause_bg_threads.store(pause, std::memory_order_release); }

    [[nodiscard]] inline vec2 getVirtScreenSize() const { return vInternalResolution; }
    [[nodiscard]] inline int getVirtScreenWidth() const { return (int)vInternalResolution.x; }
    [[nodiscard]] inline int getVirtScreenHeight() const { return (int)vInternalResolution.y; }

    [[nodiscard]] inline const std::unique_ptr<OptionsMenu> &getOptionsMenu() const { return this->optionsMenu; }
    [[nodiscard]] inline const std::unique_ptr<SongBrowser> &getSongBrowser() const { return this->songBrowser; }
    [[nodiscard]] inline const std::unique_ptr<Changelog> &getChangelog() const { return this->changelog; }
    [[nodiscard]] inline const std::unique_ptr<UserCard> &getUserButton() const { return this->userButton; }
    [[nodiscard]] inline const std::unique_ptr<Lobby> &getLobby() const { return this->lobby; }
    [[nodiscard]] inline const std::unique_ptr<SpectatorScreen> &getSpectatorScreen() const {
        return this->spectatorScreen;
    }
    [[nodiscard]] inline const std::unique_ptr<BackgroundImageHandler> &getBackgroundImageHandler() const {
        return this->backgroundImageHandler;
    }
    [[nodiscard]] inline const std::unique_ptr<HUD> &getHUD() const { return this->hud; }
    [[nodiscard]] inline const std::unique_ptr<TooltipOverlay> &getTooltipOverlay() const {
        return this->tooltipOverlay;
    }
    [[nodiscard]] inline const std::unique_ptr<ModSelector> &getModSelector() const { return this->modSelector; }
    [[nodiscard]] inline const std::unique_ptr<ModFPoSu> &getFPoSu() const { return this->fposu; }
    [[nodiscard]] inline const std::unique_ptr<PauseMenu> &getPauseMenu() const { return this->pauseMenu; }
    [[nodiscard]] inline const std::unique_ptr<Chat> &getChat() const { return this->chat; }
    [[nodiscard]] inline const std::unique_ptr<PromptScreen> &getPromptScreen() const { return this->prompt; }
    [[nodiscard]] inline const std::unique_ptr<UIUserContextMenuScreen> &getUserActions() const {
        return this->user_actions;
    }
    [[nodiscard]] inline const std::unique_ptr<RoomScreen> &getRoom() const { return this->room; }
    [[nodiscard]] inline const std::unique_ptr<NotificationOverlay> &getNotificationOverlay() const {
        return this->notificationOverlay;
    }
    [[nodiscard]] inline const std::unique_ptr<VolumeOverlay> &getVolumeOverlay() const { return this->volumeOverlay; }
    [[nodiscard]] inline const std::unique_ptr<MainMenu> &getMainMenu() const { return this->mainMenu; }
    [[nodiscard]] inline const std::unique_ptr<RankingScreen> &getRankingScreen() const { return this->rankingScreen; }
    [[nodiscard]] inline const std::unique_ptr<LiveScore> &getScore() const { return this->score; }
    [[nodiscard]] inline const std::unique_ptr<UserStatsScreen> &getUserStatsScreen() const { return this->userStats; }
    [[nodiscard]] inline const std::unique_ptr<UpdateHandler> &getUpdateHandler() const { return this->updateHandler; }
    [[nodiscard]] inline const std::unique_ptr<BeatmapInterface> &getMapInterface() const { return this->map_iface; }
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
    [[nodiscard]] inline const std::unique_ptr<Skin> &getSkin() const { return this->skin; }

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

    [[nodiscard]] bool isInPlayMode() const;
    [[nodiscard]] inline bool isSkinLoading() const {
        return this->bSkinLoadScheduled ||
               (this->skin && this->skinScheduledToLoad && this->skin.get() != this->skinScheduledToLoad);
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

   private:
    // NOTE: unique_ptrs are destroyed in reverse order of declaration in header

    // interfaces (other)
    std::unique_ptr<UpdateHandler> updateHandler{nullptr};
    std::unique_ptr<AvatarManager> avatarManager{nullptr};
    std::unique_ptr<UserCard> userButton{nullptr};
    std::unique_ptr<BackgroundImageHandler> backgroundImageHandler{nullptr};
    std::unique_ptr<Skin> skin{nullptr};
    std::unique_ptr<BeatmapInterface> map_iface{nullptr};
    std::unique_ptr<LiveScore> score{nullptr};
    std::unique_ptr<ModFPoSu> fposu{nullptr};

    // interfaces ("OsuScreen"s)

    // for looping through all screens in mouse_update/draw etc.
    // the order matters to determine priority for event handling/consumption
#define ALL_OSU_SCREENS                         \
    X(VolumeOverlay, volumeOverlay)             \
    X(PromptScreen, prompt)                     \
    X(ModSelector, modSelector)                 \
    X(UIUserContextMenuScreen, user_actions)    \
    X(RoomScreen, room)                         \
    X(NotificationOverlay, notificationOverlay) \
    X(Chat, chat)                               \
    X(OptionsMenu, optionsMenu)                 \
    X(RankingScreen, rankingScreen)             \
    X(UserStatsScreen, userStats)               \
    X(SpectatorScreen, spectatorScreen)         \
    X(PauseMenu, pauseMenu)                     \
    X(HUD, hud)                                 \
    X(SongBrowser, songBrowser)                 \
    X(Lobby, lobby)                             \
    X(Changelog, changelog)                     \
    X(MainMenu, mainMenu)                       \
    X(TooltipOverlay, tooltipOverlay)

    // declare all screen unique_ptrs
#define X(ptr_type__, name__) /*                                                          */ \
    std::unique_ptr<ptr_type__> name__{nullptr};
    ALL_OSU_SCREENS
#undef X

    // interfaces (debugging)
    std::unique_ptr<CWindowManager> windowManager{nullptr};

    // rendering
    RenderTarget *backBuffer{nullptr};
    RenderTarget *playfieldBuffer{nullptr};
    RenderTarget *sliderFrameBuffer{nullptr};
    RenderTarget *AAFrameBuffer{nullptr};
    RenderTarget *frameBuffer{nullptr};
    RenderTarget *frameBuffer2{nullptr};

    Shader *actual_flashlight_shader{nullptr};
    Shader *flashlight_shader{nullptr};

    vec2 vInternalResolution{0.f};
    // wtf is this? why?
    vec2 vInternalResolution2{0.f};

    // i don't like how these have to be public, but it's too annoying to change for now.
    // public members just mean their values can get rugpulled from under your feet at any moment,
    // and make it more annoying to find everywhere its actually changed
   public:
    vec2 flashlight_position{0.f};

    // mods
   public:  // public because of many external access
    std::vector<ConVar *> experimentalMods;
    Replay::Mods previous_mods{0};
    bool bModAutoTemp{false};  // when ctrl+clicking a map, the auto mod should disable itself after the map finishes

    // keys
   public:  // public due to "stuck key fix" in BeatmapInterface
    bool bKeyboardKey1Down{false};
    bool bKeyboardKey12Down{false};
    bool bKeyboardKey2Down{false};
    bool bKeyboardKey22Down{false};
    bool bMouseKey1Down{false};
    bool bMouseKey2Down{false};

   private:
    bool bF1{false};
    bool bUIToggleCheck{false};
    bool bScoreboardToggleCheck{false};
    bool bEscape{false};
    bool bSkipScheduled{false};
    bool bQuickRetryDown{false};
    bool bSeekKey{false};
    bool bSeeking{false};
    bool bClickedSkipButton{false};
    float fPrevSeekMousePosX{-1.f};
    float fQuickRetryTime{0.f};

   public:  // public due to BeatmapInterface access
    float fQuickSaveTime{0.f};

   private:
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
    Skin *skinScheduledToLoad{nullptr};

    // replay
   public:
    UString watched_user_name;
    i32 watched_user_id{0};

    // custom
   private:
    std::atomic<bool> pause_bg_threads{false};
    bool bScheduleEndlessModNextBeatmap{false};
    bool bWasBossKeyPaused{false};
    bool bSkinLoadScheduled{false};
    bool bSkinLoadWasReload{false};
    bool bFontReloadScheduled{false};
    bool bFireResolutionChangedScheduled{false};
    bool bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled{false};
    bool bScreensReady{false};

   public:  // public due to BassSoundEngine access
    bool music_unpause_scheduled{false};

    // helpers to do something for each screen, in order
   private:
    template <auto MemberFunc, typename Condition, typename... Args>
    inline void forEachScreenWhile(const Condition &condition, Args &&...args)
        requires std::is_invocable_v<Condition>
    {
        if(unlikely(!this->bScreensReady || !condition())) return;
        // for each screen unique_ptr, as long as condition (lambda) is true
#define X(unused__, name__)                                             \
    std::invoke(MemberFunc, this->name__, std::forward<Args>(args)...); \
    if(!condition()) return;
        ALL_OSU_SCREENS
#undef X
    }

    template <auto MemberFunc, typename Condition, typename... Args>
    inline void forEachScreenWhile(Condition &condition, Args &&...args)
        requires(!std::is_invocable_v<Condition>)
    {
        if(unlikely(!this->bScreensReady || !condition)) return;
        // for each screen unique_ptr, as long as condition (lvalue) is true
#define X(unused__, name__)                                             \
    std::invoke(MemberFunc, this->name__, std::forward<Args>(args)...); \
    if(!condition) return;
        ALL_OSU_SCREENS
#undef X
    }

    // passthrough for unconditional iteration
    template <auto MemberFunc, typename... Args>
    inline void forEachScreen(Args &&...args) {
        static constexpr const bool always_true{true};  // so it's an lvalue...
        this->forEachScreenWhile<MemberFunc>(always_true, std::forward<Args>(args)...);
    }

    // we want to destroy them in the same order as we listed them, to match the old (raw pointer) behavior
    void destroyAllScreensInOrder();
};

extern Osu *osu;
