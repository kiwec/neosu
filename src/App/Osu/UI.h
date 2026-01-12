#pragma once
// Copyright (c) 2026, kiwec, All rights reserved.

#include <memory>

#include "config.h"
#include "noinclude.h"

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

// UIOverlays, manually created + added to the "overlays" array and destroyed in reverse order in dtor
// (enum, typename, "short name" for getter)
#define ALL_OVERLAYS_(X)                                                \
    X(OV_VOLUMEOVERLAY, VolumeOverlay, VolumeOverlay)                   \
    X(OV_PROMPTSCREEN, PromptScreen, PromptScreen)                      \
    X(OV_MODSELECTOR, ModSelector, ModSelector)                         \
    X(OV_UIUSERCONTEXTMENUSCREEN, UIUserContextMenuScreen, UserActions) \
    X(OV_ROOMSCREEN, RoomScreen, Room)                                  \
    X(OV_CHAT, Chat, Chat)                                              \
    X(OV_OPTIONSMENU, OptionsMenu, OptionsMenu)                         \
    X(OV_RANKINGSCREEN, RankingScreen, RankingScreen)                   \
    X(OV_USERSTATSSCREEN, UserStatsScreen, UserStatsScreen)             \
    X(OV_SPECTATORSCREEN, SpectatorScreen, SpectatorScreen)             \
    X(OV_PAUSEMENU, PauseMenu, PauseMenu)                               \
    X(OV_HUD, HUD, HUD)                                                 \
    X(OV_SONGBROWSER, SongBrowser, SongBrowser)                         \
    X(OV_OSUDIRECTSCREEN, OsuDirectScreen, OsuDirectScreen)             \
    X(OV_LOBBY, Lobby, Lobby)                                           \
    X(OV_CHANGELOG, Changelog, Changelog)                               \
    X(OV_MAINMENU, MainMenu, MainMenu)                                  \
    X(OV_TOOLTIPOVERLAY, TooltipOverlay, TooltipOverlay)

struct UI final {
    NOCOPY_NOMOVE(UI)

   private:
    class NullScreen;

    enum OverlayKind : uint8_t {
        OV_NULLSCREEN,                   /* created early */
        OV_NOTIFICATIONOVERLAY,          /* created early */
#define X_(enumid__, type__, shortname_) /*                                                                       */ \
    enumid__,
        ALL_OVERLAYS_(X_) /*                                                                                      */
        OV_MAX
#undef X_
    };

   public:
    UI();
    ~UI();

    bool init();
    void hide();
    void show();

    void update();
    void draw();
    void onKeyDown(KeyboardEvent &key);
    void onKeyUp(KeyboardEvent &key);
    void onChar(KeyboardEvent &e);
    void onResolutionChange(vec2 newResolution);
    void stealFocus();

    [[nodiscard]] inline UIOverlay *getScreen() { return this->active_screen; }
    inline void setScreen(std::nullptr_t) { this->hide(); }
    void setScreen(UIOverlay *screen);

    [[nodiscard]] NotificationOverlay *getNotificationOverlay();  // created early, in ctor, for error notifications

#define X_(enumid__, type__, shortname__) /*                                                          */ \
    [[nodiscard]] type__ *get##shortname__();

    ALL_OVERLAYS_(X_)
#undef X_

   private:
    static constexpr const size_t EARLY_OVERLAYS{2};  // dummy+notificationOverlay

    std::array<UIOverlay *, OV_MAX> overlays{};
    UIOverlay *active_screen;

    // interfaces (debugging)
    // std::unique_ptr<CWindowManager> windowManager{nullptr};
};
