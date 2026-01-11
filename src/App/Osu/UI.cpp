#include "UI.h"

#include <ranges>

#include "noinclude.h"
#include "Bancho.h"
#include "BeatmapInterface.h"
#include "CWindowManager.h"
#include "Engine.h"
#include "Graphics.h"
#include "ModFPoSu.h"
#include "Mouse.h"
#include "Osu.h"
#include "OsuConVars.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "UIOverlay.h"

#include "Changelog.h"
#include "Chat.h"
#include "HUD.h"
#include "Lobby.h"
#include "MainMenu.h"
#include "ModSelector.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "OsuDirectScreen.h"
#include "PauseMenu.h"
#include "PromptScreen.h"
#include "RankingScreen.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "SpectatorScreen.h"
#include "TooltipOverlay.h"
#include "UIUserContextMenu.h"
#include "UIContextMenu.h"
#include "UserStatsScreen.h"
#include "VolumeOverlay.h"

std::unique_ptr<UI> ui = nullptr;

#define X_(enumid__, type__, shortname__) /*                                                          */ \
    type__ *UI::get##shortname__() { return static_cast<type__ *>(this->overlays[(size_t)(OverlayKind::enumid__)]); }
ALL_OVERLAYS_(X_)
#undef X_

NotificationOverlay *UI::getNotificationOverlay() {
    return static_cast<NotificationOverlay *>(this->overlays[OV_NOTIFICATIONOVERLAY]);
}

class UI::NullScreen final : public UIOverlay {
    NOCOPY_NOMOVE(NullScreen)
   public:
    NullScreen() : UIOverlay() {}
    ~NullScreen() final = default;

    forceinline CBaseUIElement *setVisible(bool /*visible*/) final {
        this->bVisible = false;
        return this;
    }
    forceinline bool isVisible() final { return false; }
};

UI::UI() {
    this->overlays[OV_NULLSCREEN] = this->active_screen = new NullScreen();
    this->overlays[OV_NOTIFICATIONOVERLAY] = new NotificationOverlay();
}

UI::~UI() {
    // destroy screens in reverse order
    for(auto &screen : this->overlays | std::views::reverse) {
        SAFE_DELETE(screen);
    }
    this->overlays = {};
}

bool UI::init() {
#define X_(enumid__, type__, shortname__) /*                                                          */ \
    this->overlays[(size_t)(OverlayKind::enumid__)] = new type__();
    ALL_OVERLAYS_(X_)
#undef X_

    this->getNotificationOverlay()->addKeyListener(this->getOptionsMenu());
    this->active_screen = this->getMainMenu();

    // debug
    // this->windowManager = std::make_unique<CWindowManager>();
    return true;
}

void UI::update() {
    bool updated_active_screen = false;
    for(auto *screen : this->overlays) {
        screen->update();
        if(screen == this->active_screen) updated_active_screen = true;
        if(!mouse->propagate_clicks) break;
    }

    if(!updated_active_screen && mouse->propagate_clicks) {
        this->active_screen->update();
    }
}

void UI::draw() {
    // if we are not using the native window resolution, draw into the buffer
    const bool isBufferedDraw = (g->getResolution() != osu->getVirtScreenSize());
    if(isBufferedDraw) {
        osu->backBuffer->enable();
    }

    f32 fadingCursorAlpha = 1.f;
    const bool isFPoSu = (cv::mod_fposu.getBool());

    // draw everything in the correct order
    if(osu->isInPlayMode()) {  // if we are playing a beatmap
        if(isFPoSu) osu->playfieldBuffer->enable();

        // draw playfield (incl. flashlight/smoke etc.)
        osu->map_iface->draw();

        if(!isFPoSu) this->getHUD()->draw();

        // quick retry fadeout overlay
        if(osu->fQuickRetryTime != 0.0f && osu->bQuickRetryDown) {
            float alphaPercent = 1.0f - (osu->fQuickRetryTime - engine->getTime()) / cv::quick_retry_delay.getFloat();
            if(engine->getTime() > osu->fQuickRetryTime) alphaPercent = 1.0f;

            g->setColor(argb((int)(255 * alphaPercent), 0, 0, 0));
            g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
        }

        this->getPauseMenu()->draw();
        this->getModSelector()->draw();
        this->getChat()->draw();
        this->getUserActions()->draw();
        this->getOptionsMenu()->draw();

        if(!isFPoSu) {
            this->getHUD()->drawFps();
        }

        // this->windowManager->draw();

        if(isFPoSu && cv::draw_cursor_ripples.getBool()) this->getHUD()->drawCursorRipples();

        // draw FPoSu cursor trail
        fadingCursorAlpha =
            1.0f - std::clamp<float>((float)osu->score->getCombo() / cv::mod_fadingcursor_combo.getFloat(), 0.0f, 1.0f);
        if(this->getPauseMenu()->isVisible() || osu->map_iface->isContinueScheduled() ||
           !cv::mod_fadingcursor.getBool())
            fadingCursorAlpha = 1.0f;
        if(isFPoSu && cv::fposu_draw_cursor_trail.getBool()) {
            const vec2 trailpos = osu->map_iface->isPaused() ? mouse->getPos() : osu->map_iface->getCursorPos();
            this->getHUD()->drawCursorTrail(trailpos, fadingCursorAlpha);
        }

        if(isFPoSu) {
            osu->playfieldBuffer->disable();
            osu->fposu->draw();
            this->getHUD()->draw();
            this->getHUD()->drawFps();
        }

        // draw debug info on top of everything else
        if(cv::debug_draw_timingpoints.getBool()) osu->map_iface->drawDebug();

    } else {  // if we are not playing

        this->active_screen->draw();
        this->getChat()->draw();
        this->getUserActions()->draw();
        this->getOptionsMenu()->draw();
        this->getModSelector()->draw();
        this->getPromptScreen()->draw();

        this->getHUD()->drawFps();

        // this->windowManager->draw();
    }

    this->getTooltipOverlay()->draw();
    this->getNotificationOverlay()->draw();
    this->getVolumeOverlay()->draw();

    // loading spinner for some async tasks
    if((osu->bSkinLoadScheduled && osu->getSkin() != osu->skinScheduledToLoad)) {
        this->getHUD()->drawLoadingSmall("");
    }

    if(osu->isInPlayMode()) {
        // draw cursor (gameplay)
        const bool paused = osu->map_iface->isPaused();
        const vec2 cursorPos =
            isFPoSu ? (osu->getVirtScreenSize() / 2.0f) : (paused ? mouse->getPos() : osu->map_iface->getCursorPos());
        const bool drawSecondTrail = !paused && (cv::mod_autoplay.getBool() || cv::mod_autopilot.getBool() ||
                                                 osu->map_iface->is_watching || BanchoState::spectating);
        const bool updateAndDrawTrail = !isFPoSu;
        this->getHUD()->drawCursor(cursorPos, fadingCursorAlpha, drawSecondTrail, updateAndDrawTrail);
    } else {
        // draw cursor (menus)
        this->getHUD()->drawCursor(mouse->getPos());
    }

    osu->drawRuntimeInfo();

    // if we are not using the native window resolution
    if(isBufferedDraw) {
        // draw a scaled version from the buffer to the screen
        osu->backBuffer->disable();

        const vec2 offset{vec2{g->getResolution() - osu->getVirtScreenSize()} * 0.5f};
        g->setBlending(false);
        if(cv::letterboxing.getBool()) {
            osu->backBuffer->draw(offset.x * (1.0f + cv::letterboxing_offset_x.getFloat()),
                                  offset.y * (1.0f + cv::letterboxing_offset_y.getFloat()), osu->getVirtScreenWidth(),
                                  osu->getVirtScreenHeight());
        } else {
            if(cv::resolution_keep_aspect_ratio.getBool()) {
                const float scale = Osu::getImageScaleToFitResolution(osu->backBuffer->getSize(), g->getResolution());
                const float scaledWidth = osu->backBuffer->getWidth() * scale;
                const float scaledHeight = osu->backBuffer->getHeight() * scale;
                osu->backBuffer->draw(std::max(0.0f, g->getResolution().x / 2.0f - scaledWidth / 2.0f) *
                                          (1.0f + cv::letterboxing_offset_x.getFloat()),
                                      std::max(0.0f, g->getResolution().y / 2.0f - scaledHeight / 2.0f) *
                                          (1.0f + cv::letterboxing_offset_y.getFloat()),
                                      scaledWidth, scaledHeight);
            } else {
                osu->backBuffer->draw(0, 0, g->getResolution().x, g->getResolution().y);
            }
        }
        g->setBlending(true);
    }
}

void UI::onKeyDown(KeyboardEvent &key) {
    if(key.isConsumed()) return;

    for(auto *screen : this->overlays) {
        screen->onKeyDown(key);
        if(key.isConsumed()) break;
    }
}

void UI::onKeyUp(KeyboardEvent &key) {
    if(key.isConsumed()) return;

    for(auto *screen : this->overlays) {
        screen->onKeyUp(key);
        if(key.isConsumed()) break;
    }
}

void UI::onChar(KeyboardEvent &e) {
    if(e.isConsumed()) return;

    for(auto *screen : this->overlays) {
        screen->onChar(e);
        if(e.isConsumed()) break;
    }
}

void UI::onResolutionChange(vec2 newResolution) {
    for(auto *screen : this->overlays) {
        screen->onResolutionChange(newResolution);
    }
}

void UI::stealFocus() {
    for(auto *screen : this->overlays) {
        screen->stealFocus();
    }
}

void UI::hide() { this->active_screen->setVisible(false); }

void UI::show() { this->active_screen->setVisible(true); }

void UI::setScreen(UIOverlay *screen) {
    if(this->active_screen->isVisible()) {
        this->active_screen->setVisible(false);

        // Close any "temporary" overlays
        ui->getPromptScreen()->setVisible(false);
    }

    this->active_screen = screen;
    this->show();
}
