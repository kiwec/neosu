#pragma once
// Copyright (c) 2026, kiwec, All rights reserved.

#include <memory>
#include "OsuConfig.h"
#include "Vectors.h"

class CWindowManager;
class KeyboardEvent;
class RenderTarget;
class UIOverlay;

class Changelog;
class Chat;
class HUD;
class Lobby;
class MainMenu;
class ModSelector;
class NotificationOverlay;
class OptionsMenu;
class OsuDirectScreen;
class PauseMenu;
class PromptScreen;
class RankingScreen;
class RoomScreen;
class SongBrowser;
class SpectatorScreen;
class TooltipOverlay;
class UIUserContextMenuScreen;
class UserStatsScreen;
class VolumeOverlay;

// global for convenience, created in osu constructor, destroyed in osu constructor
struct UI;
extern std::unique_ptr<UI> ui;

struct UI final {
    UI();
    ~UI();

    void update();
    void draw();
    void onKeyDown(KeyboardEvent &key);
    void onKeyUp(KeyboardEvent &key);
    void onChar(KeyboardEvent &e);
    void onResolutionChange(vec2 newResolution);
    void rebuildRenderTargets();
    void stealFocus();

    [[nodiscard]] inline UIOverlay *getScreen() { return this->active_screen; }
    void setScreen(UIOverlay *screen);

    [[nodiscard]] inline Changelog *getChangelog() { return this->changelog; }
    [[nodiscard]] inline Chat *getChat() { return this->chat; }
    [[nodiscard]] inline HUD *getHUD() { return this->hud; }
    [[nodiscard]] inline Lobby *getLobby() { return this->lobby; }
    [[nodiscard]] inline MainMenu *getMainMenu() { return this->mainMenu; }
    [[nodiscard]] inline ModSelector *getModSelector() { return this->modSelector; }
    [[nodiscard]] inline NotificationOverlay *getNotificationOverlay() { return this->notificationOverlay; }
    [[nodiscard]] inline OptionsMenu *getOptionsMenu() { return this->optionsMenu; }
    [[nodiscard]] inline OsuDirectScreen *getOsuDirectScreen() { return this->osuDirectScreen; }
    [[nodiscard]] inline PauseMenu *getPauseMenu() { return this->pauseMenu; }
    [[nodiscard]] inline PromptScreen *getPromptScreen() { return this->prompt; }
    [[nodiscard]] inline RankingScreen *getRankingScreen() { return this->rankingScreen; }
    [[nodiscard]] inline RoomScreen *getRoom() { return this->room; }
    [[nodiscard]] inline SongBrowser *getSongBrowser() { return this->songBrowser; }
    [[nodiscard]] inline SpectatorScreen *getSpectatorScreen() { return this->spectatorScreen; }
    [[nodiscard]] inline TooltipOverlay *getTooltipOverlay() { return this->tooltipOverlay; }
    [[nodiscard]] inline UIUserContextMenuScreen *getUserActions() { return this->user_actions; }
    [[nodiscard]] inline UserStatsScreen *getUserStatsScreen() { return this->userStats; }
    [[nodiscard]] inline VolumeOverlay *getVolumeOverlay() { return this->volumeOverlay; }

    [[nodiscard]] inline RenderTarget *getPlayfieldBuffer() const { return this->playfieldBuffer; }
    [[nodiscard]] inline RenderTarget *getSliderFrameBuffer() const { return this->sliderFrameBuffer; }
    [[nodiscard]] inline RenderTarget *getAAFrameBuffer() const { return this->AAFrameBuffer; }

   private:
    static constexpr auto NUM_OVERLAYS{19};  // make sure to update this if adding/removing overlays
    std::array<UIOverlay *, NUM_OVERLAYS> overlays{};
    UIOverlay *active_screen{nullptr};

    // UIOverlays, manually created + added to the "overlays" array and destroyed in reverse order in dtor
    Changelog *changelog{nullptr};
    Chat *chat{nullptr};
    HUD *hud{nullptr};
    Lobby *lobby{nullptr};
    MainMenu *mainMenu{nullptr};
    ModSelector *modSelector{nullptr};
    NotificationOverlay *notificationOverlay{nullptr};
    OptionsMenu *optionsMenu{nullptr};
    OsuDirectScreen *osuDirectScreen{nullptr};
    PauseMenu *pauseMenu{nullptr};
    PromptScreen *prompt{nullptr};
    RankingScreen *rankingScreen{nullptr};
    RoomScreen *room{nullptr};
    SongBrowser *songBrowser{nullptr};
    SpectatorScreen *spectatorScreen{nullptr};
    TooltipOverlay *tooltipOverlay{nullptr};
    UIUserContextMenuScreen *user_actions{nullptr};
    UserStatsScreen *userStats{nullptr};
    VolumeOverlay *volumeOverlay{nullptr};

    // interfaces (debugging)
    std::unique_ptr<CWindowManager> windowManager{nullptr};

    // rendering
    RenderTarget *backBuffer{nullptr};
    RenderTarget *playfieldBuffer{nullptr};
    RenderTarget *sliderFrameBuffer{nullptr};
    RenderTarget *AAFrameBuffer{nullptr};
};
