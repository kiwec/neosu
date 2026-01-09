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
#include "UserStatsScreen.h"
#include "VolumeOverlay.h"

std::unique_ptr<UI> ui = nullptr;

UI::UI() {
    this->backBuffer = resourceManager->createRenderTarget(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
    this->playfieldBuffer = resourceManager->createRenderTarget(0, 0, 64, 64);
    this->sliderFrameBuffer =
        resourceManager->createRenderTarget(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
    this->AAFrameBuffer = resourceManager->createRenderTarget(
        0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight(), MultisampleType::MULTISAMPLE_4X);

    int overlayit = 0;
    this->overlays[overlayit++] = this->notificationOverlay = new NotificationOverlay();
    this->overlays[overlayit++] = this->volumeOverlay = new VolumeOverlay();
    this->overlays[overlayit++] = this->prompt = new PromptScreen();
    this->overlays[overlayit++] = this->modSelector = new ModSelector();
    this->overlays[overlayit++] = this->user_actions = new UIUserContextMenuScreen();
    this->overlays[overlayit++] = this->room = new RoomScreen();
    this->overlays[overlayit++] = this->chat = new Chat();
    this->overlays[overlayit++] = this->optionsMenu = new OptionsMenu();
    this->overlays[overlayit++] = this->rankingScreen = new RankingScreen();
    this->overlays[overlayit++] = this->userStats = new UserStatsScreen();
    this->overlays[overlayit++] = this->spectatorScreen = new SpectatorScreen();
    this->overlays[overlayit++] = this->pauseMenu = new PauseMenu();
    this->overlays[overlayit++] = this->hud = new HUD();
    this->overlays[overlayit++] = this->songBrowser = new SongBrowser();
    this->overlays[overlayit++] = this->osuDirectScreen = new OsuDirectScreen();
    this->overlays[overlayit++] = this->lobby = new Lobby();
    this->overlays[overlayit++] = this->changelog = new Changelog();
    this->overlays[overlayit++] = this->mainMenu = new MainMenu();
    this->overlays[overlayit++] = this->tooltipOverlay = new TooltipOverlay();
    assert(overlayit == NUM_OVERLAYS);

    this->notificationOverlay->addKeyListener(this->optionsMenu);

    // debug
    this->windowManager = std::make_unique<CWindowManager>();
}

UI::~UI() {
    // destroy screens in reverse order
    for(auto *screen : this->overlays | std::views::reverse) {
        SAFE_DELETE(screen);
    }
    this->overlays = {};
}

void UI::update() {
    bool updated_active_screen = false;
    bool propagate_clicks = true;
    for(auto *screen : this->overlays) {
        screen->mouse_update(&propagate_clicks);
        if(screen == ui->getScreen()) updated_active_screen = true;
        if(!propagate_clicks) break;
    }

    if(!updated_active_screen && ui->getScreen() != nullptr) {
        ui->getScreen()->mouse_update(&propagate_clicks);
    }
}

void UI::draw() {
    // if we are not using the native window resolution, draw into the buffer
    const bool isBufferedDraw = (g->getResolution() != osu->getVirtScreenSize());
    if(isBufferedDraw) {
        this->backBuffer->enable();
    }

    f32 fadingCursorAlpha = 1.f;
    const bool isFPoSu = (cv::mod_fposu.getBool());

    // draw everything in the correct order
    if(osu->isInPlayMode()) {  // if we are playing a beatmap
        if(isFPoSu) this->playfieldBuffer->enable();

        // draw playfield (incl. flashlight/smoke etc.)
        osu->getMapInterface()->draw();

        if(!isFPoSu) this->hud->draw();

        // quick retry fadeout overlay
        if(osu->fQuickRetryTime != 0.0f && osu->bQuickRetryDown) {
            float alphaPercent = 1.0f - (osu->fQuickRetryTime - engine->getTime()) / cv::quick_retry_delay.getFloat();
            if(engine->getTime() > osu->fQuickRetryTime) alphaPercent = 1.0f;

            g->setColor(argb((int)(255 * alphaPercent), 0, 0, 0));
            g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
        }

        this->pauseMenu->draw();
        this->modSelector->draw();
        this->chat->draw();
        this->user_actions->draw();
        this->optionsMenu->draw();

        if(!isFPoSu) {
            this->hud->drawFps();
        }

        this->windowManager->draw();

        if(isFPoSu && cv::draw_cursor_ripples.getBool()) this->hud->drawCursorRipples();

        // draw FPoSu cursor trail
        fadingCursorAlpha =
            1.0f -
            std::clamp<float>((float)osu->getScore()->getCombo() / cv::mod_fadingcursor_combo.getFloat(), 0.0f, 1.0f);
        if(this->pauseMenu->isVisible() || osu->getMapInterface()->isContinueScheduled() ||
           !cv::mod_fadingcursor.getBool())
            fadingCursorAlpha = 1.0f;
        if(isFPoSu && cv::fposu_draw_cursor_trail.getBool()) {
            const vec2 trailpos =
                osu->getMapInterface()->isPaused() ? mouse->getPos() : osu->getMapInterface()->getCursorPos();
            this->hud->drawCursorTrail(trailpos, fadingCursorAlpha);
        }

        if(isFPoSu) {
            this->playfieldBuffer->disable();
            osu->getFPoSu()->draw();
            this->hud->draw();
            this->hud->drawFps();
        }

        // draw debug info on top of everything else
        if(cv::debug_draw_timingpoints.getBool()) osu->getMapInterface()->drawDebug();

    } else {  // if we are not playing

        if(this->active_screen != nullptr) {
            this->active_screen->draw();
        }

        this->chat->draw();
        this->user_actions->draw();
        this->optionsMenu->draw();
        this->modSelector->draw();
        this->prompt->draw();

        this->hud->drawFps();

        this->windowManager->draw();
    }

    this->tooltipOverlay->draw();
    this->notificationOverlay->draw();
    this->volumeOverlay->draw();

    // loading spinner for some async tasks
    if((osu->bSkinLoadScheduled && osu->getSkin() != osu->skinScheduledToLoad)) {
        this->hud->drawLoadingSmall("");
    }

    if(osu->isInPlayMode()) {
        // draw cursor (gameplay)
        const bool paused = osu->getMapInterface()->isPaused();
        const vec2 cursorPos = isFPoSu ? (osu->getVirtScreenSize() / 2.0f)
                                       : (paused ? mouse->getPos() : osu->getMapInterface()->getCursorPos());
        const bool drawSecondTrail = !paused && (cv::mod_autoplay.getBool() || cv::mod_autopilot.getBool() ||
                                                 osu->getMapInterface()->is_watching || BanchoState::spectating);
        const bool updateAndDrawTrail = !isFPoSu;
        this->hud->drawCursor(cursorPos, fadingCursorAlpha, drawSecondTrail, updateAndDrawTrail);
    } else {
        // draw cursor (menus)
        this->hud->drawCursor(mouse->getPos());
    }

    osu->drawRuntimeInfo();

    // if we are not using the native window resolution
    if(isBufferedDraw) {
        // draw a scaled version from the buffer to the screen
        this->backBuffer->disable();

        const vec2 offset{vec2{g->getResolution() - osu->getVirtScreenSize()} * 0.5f};
        g->setBlending(false);
        if(cv::letterboxing.getBool()) {
            this->backBuffer->draw(offset.x * (1.0f + cv::letterboxing_offset_x.getFloat()),
                                   offset.y * (1.0f + cv::letterboxing_offset_y.getFloat()), osu->getVirtScreenWidth(),
                                   osu->getVirtScreenHeight());
        } else {
            if(cv::resolution_keep_aspect_ratio.getBool()) {
                const float scale = osu->getImageScaleToFitResolution(this->backBuffer->getSize(), g->getResolution());
                const float scaledWidth = this->backBuffer->getWidth() * scale;
                const float scaledHeight = this->backBuffer->getHeight() * scale;
                this->backBuffer->draw(std::max(0.0f, g->getResolution().x / 2.0f - scaledWidth / 2.0f) *
                                           (1.0f + cv::letterboxing_offset_x.getFloat()),
                                       std::max(0.0f, g->getResolution().y / 2.0f - scaledHeight / 2.0f) *
                                           (1.0f + cv::letterboxing_offset_y.getFloat()),
                                       scaledWidth, scaledHeight);
            } else {
                this->backBuffer->draw(0, 0, g->getResolution().x, g->getResolution().y);
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

void UI::rebuildRenderTargets() {
    const i32 w = osu->getVirtScreenWidth();
    const i32 h = osu->getVirtScreenHeight();

    if(cv::mod_fposu.getBool())
        this->playfieldBuffer->rebuild(0, 0, w, h);
    else
        this->playfieldBuffer->rebuild(0, 0, 64, 64);

    this->backBuffer->rebuild(0, 0, w, h);
    this->sliderFrameBuffer->rebuild(0, 0, w, h, MultisampleType::MULTISAMPLE_0X);
    this->AAFrameBuffer->rebuild(0, 0, w, h, MultisampleType::MULTISAMPLE_4X);
}

void UI::stealFocus() {
    for(auto *screen : this->overlays) {
        screen->stealFocus();
    }
}

void UI::setScreen(UIOverlay *screen) {
    if(this->active_screen && this->active_screen->isVisible()) {
        this->active_screen->setVisible(false);

        // Close any "temporary" overlays
        ui->getPromptScreen()->setVisible(false);
    }

    this->active_screen = screen;
    this->active_screen->setVisible(true);
}
