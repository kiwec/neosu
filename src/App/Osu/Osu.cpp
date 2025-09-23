// Copyright (c) 2015, PG, All rights reserved.
#include "Osu.h"

#include <algorithm>
#include <utility>

#include "AnimationHandler.h"
#include "AvatarManager.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BeatmapInterface.h"
#include "CBaseUIScrollView.h"
#include "CBaseUISlider.h"
#include "CBaseUITextbox.h"
#include "CWindowManager.h"
#include "Changelog.h"
#include "Chat.h"
#include "ConVar.h"
#include "Console.h"
#include "ConsoleBox.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"
#include "Engine.h"
#include "File.h"
#include "Environment.h"
#include "GameRules.h"
#include "HUD.h"
#include "HitObjects.h"
#include "Icons.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "LegacyReplay.h"
#include "Lobby.h"
#include "MainMenu.h"
#include "ModFPoSu.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "PauseMenu.h"
#include "PeppyImporter.h"
#include "Profiler.h"
#include "PromptScreen.h"
#include "RankingScreen.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "RoomScreen.h"
#include "Shader.h"
#include "Skin.h"
#include "SongBrowser/LeaderboardPPCalcThread.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/MapCalcThread.h"
#include "SongBrowser/ScoreConverterThread.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "TooltipOverlay.h"
#include "UIContextMenu.h"
#include "UIModSelectorModButton.h"
#include "UIUserContextMenu.h"
#include "UpdateHandler.h"
#include "UserCard.h"
#include "UserStatsScreen.h"
#include "VolumeOverlay.h"
#include "Logging.h"

#include "crypto.h"
#include "score.h"

#include "shaders.h"

Osu *osu = nullptr;

Osu::Osu() {
    osu = this;
    srand(crypto::rng::get_rand<u32>());

    if(Env::cfg(BUILD::DEBUG)) {
        BanchoState::neosu_version = UString::fmt("dev-{}", cv::build_timestamp.getVal<u64>());
    } else if(cv::is_bleedingedge.getBool()) {  // FIXME: isn't this always false here...?
        BanchoState::neosu_version = UString::fmt("bleedingedge-{}", cv::build_timestamp.getVal<u64>());
    } else {
        BanchoState::neosu_version = UString::fmt("release-{:.2f}", cv::version.getFloat());
    }

    BanchoState::user_agent = "Mozilla/5.0 (compatible; neosu/";
    BanchoState::user_agent.append(BanchoState::neosu_version);
    BanchoState::user_agent.append("; " OS_NAME "; +https://" NEOSU_DOMAIN "/)");

    // experimental mods list
    this->experimentalMods.push_back(&cv::fposu_mod_strafing);
    this->experimentalMods.push_back(&cv::mod_wobble);
    this->experimentalMods.push_back(&cv::mod_arwobble);
    this->experimentalMods.push_back(&cv::mod_timewarp);
    this->experimentalMods.push_back(&cv::mod_artimewarp);
    this->experimentalMods.push_back(&cv::mod_minimize);
    this->experimentalMods.push_back(&cv::mod_fadingcursor);
    this->experimentalMods.push_back(&cv::mod_fps);
    this->experimentalMods.push_back(&cv::mod_jigsaw1);
    this->experimentalMods.push_back(&cv::mod_jigsaw2);
    this->experimentalMods.push_back(&cv::mod_fullalternate);
    this->experimentalMods.push_back(&cv::mod_reverse_sliders);
    this->experimentalMods.push_back(&cv::mod_no50s);
    this->experimentalMods.push_back(&cv::mod_no100s);
    this->experimentalMods.push_back(&cv::mod_ming3012);
    this->experimentalMods.push_back(&cv::mod_halfwindow);
    this->experimentalMods.push_back(&cv::mod_millhioref);
    this->experimentalMods.push_back(&cv::mod_mafham);
    this->experimentalMods.push_back(&cv::mod_strict_tracking);
    this->experimentalMods.push_back(&cv::playfield_mirror_horizontal);
    this->experimentalMods.push_back(&cv::playfield_mirror_vertical);
    this->experimentalMods.push_back(&cv::mod_wobble2);
    this->experimentalMods.push_back(&cv::mod_shirone);
    this->experimentalMods.push_back(&cv::mod_approach_different);

    env->setWindowTitle("neosu");

    engine->getConsoleBox()->setRequireShiftToActivate(true);
    mouse->addListener(this);

    // set default fullscreen resolution to match primary display
    auto default_res = env->getNativeScreenSize();
    cv::resolution.setValue(UString::format("%ix%i", (i32)default_res.x, (i32)default_res.y));

    // convar callbacks
    cv::resolution.setCallback(SA::MakeDelegate<&Osu::onInternalResolutionChanged>(this));
    cv::windowed_resolution.setCallback(SA::MakeDelegate<&Osu::onWindowedResolutionChanged>(this));
    cv::animation_speed_override.setCallback(SA::MakeDelegate<&Osu::onAnimationSpeedChange>(this));
    cv::ui_scale.setCallback(SA::MakeDelegate<&Osu::onUIScaleChange>(this));
    cv::ui_scale_to_dpi.setCallback(SA::MakeDelegate<&Osu::onUIScaleToDPIChange>(this));
    cv::letterboxing.setCallback(SA::MakeDelegate<&Osu::onLetterboxingChange>(this));
    cv::letterboxing_offset_x.setCallback(SA::MakeDelegate<&Osu::onLetterboxingOffsetChange>(this));
    cv::letterboxing_offset_y.setCallback(SA::MakeDelegate<&Osu::onLetterboxingOffsetChange>(this));
    cv::confine_cursor_windowed.setCallback(SA::MakeDelegate<&Osu::updateConfineCursor>(this));
    cv::confine_cursor_fullscreen.setCallback(SA::MakeDelegate<&Osu::updateConfineCursor>(this));
    cv::confine_cursor_never.setCallback(SA::MakeDelegate<&Osu::updateConfineCursor>(this));
    cv::osu_folder.setCallback(SA::MakeDelegate<&Osu::updateOsuFolder>(this));

    // debug
    this->windowManager = std::make_unique<CWindowManager>();

    // renderer
    this->vInternalResolution = engine->getScreenSize();

    this->backBuffer =
        resourceManager->createRenderTarget(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());
    this->playfieldBuffer = resourceManager->createRenderTarget(0, 0, 64, 64);
    this->sliderFrameBuffer =
        resourceManager->createRenderTarget(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());
    this->AAFrameBuffer = resourceManager->createRenderTarget(
        0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight(), Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_4X);
    this->frameBuffer = resourceManager->createRenderTarget(0, 0, 64, 64);
    this->frameBuffer2 = resourceManager->createRenderTarget(0, 0, 64, 64);

    // load a few select subsystems very early
    this->map_iface = std::make_unique<BeatmapInterface>();
    this->notificationOverlay = std::make_unique<NotificationOverlay>();
    this->score = std::make_unique<LiveScore>(false);
    this->updateHandler = std::make_unique<UpdateHandler>();
    this->avatarManager = std::make_unique<AvatarManager>();

    // load main menu icon before skin
    resourceManager->loadImage("neosu.png", "NEOSU_LOGO");

    // exec the main config file (this must be right here!)
    Console::execConfigFile("underride");  // same as override, but for defaults
    Console::execConfigFile("osu");
    Console::execConfigFile("override");  // used for quickfixing live builds without redeploying/recompiling

    // clear screen in case cfg switched to fullscreen mode
    // (loading the rest of the app can take a bit of time)
    engine->onPaint();

    // if we don't have an osu.cfg, import
    if(!Environment::fileExists(MCENGINE_DATA_DIR "cfg/osu.cfg")) {
        PeppyImporter::import_settings_from_osu_stable();
    }

    // Initialize sound here so we can load the preferred device from config
    // Avoids initializing the sound device twice, which can take a while depending on the driver
    if(Env::cfg(AUD::BASS) && soundEngine->getTypeId() == SoundEngine::BASS) {
        soundEngine->updateOutputDevices(true);
        soundEngine->initializeOutputDevice(soundEngine->getWantedDevice());
        cv::snd_output_device.setValue(soundEngine->getOutputDeviceName());
        cv::snd_freq.setCallback(SA::MakeDelegate<&SoundEngine::onFreqChanged>(soundEngine.get()));
        cv::cmd::snd_restart.setCallback(SA::MakeDelegate<&SoundEngine::restart>(soundEngine.get()));
        cv::win_snd_wasapi_exclusive.setCallback(SA::MakeDelegate<&SoundEngine::onParamChanged>(soundEngine.get()));
        cv::win_snd_wasapi_buffer_size.setCallback(SA::MakeDelegate<&SoundEngine::onParamChanged>(soundEngine.get()));
        cv::win_snd_wasapi_period_size.setCallback(SA::MakeDelegate<&SoundEngine::onParamChanged>(soundEngine.get()));
        cv::asio_buffer_size.setCallback(SA::MakeDelegate<&SoundEngine::onParamChanged>(soundEngine.get()));
    } else if(Env::cfg(AUD::SOLOUD) && soundEngine->getTypeId() == SoundEngine::SOLOUD) {
        this->setupSoloud();
    }

    // Initialize skin after sound engine has started, or else sounds won't load properly
    cv::skin.setCallback(SA::MakeDelegate<&Osu::onSkinChange>(this));
    cv::skin_reload.setCallback(SA::MakeDelegate<&Osu::onSkinReload>(this));
    this->onSkinChange(cv::skin.getString().c_str());

    // Convar callbacks that should be set after loading the config
    cv::mod_mafham.setCallback(SA::MakeDelegate<&Osu::onModMafhamChange>(this));
    cv::mod_fposu.setCallback(SA::MakeDelegate<&Osu::onModFPoSuChange>(this));
    cv::playfield_mirror_horizontal.setCallback(SA::MakeDelegate<&Osu::updateModsForConVarTemplate>(this));
    cv::playfield_mirror_vertical.setCallback(SA::MakeDelegate<&Osu::updateModsForConVarTemplate>(this));
    cv::playfield_rotation.setCallback(SA::MakeDelegate<&Osu::onPlayfieldChange>(this));
    cv::speed_override.setCallback(SA::MakeDelegate<&Osu::onSpeedChange>(this));
    cv::mod_doubletime_dummy.setCallback(
        [] { cv::speed_override.setValue(cv::mod_doubletime_dummy.getBool() ? "1.5" : "-1.0"); });
    cv::mod_halftime_dummy.setCallback(
        [] { cv::speed_override.setValue(cv::mod_halftime_dummy.getBool() ? "0.75" : "-1.0"); });
    cv::draw_songbrowser_thumbnails.setCallback(SA::MakeDelegate<&Osu::onThumbnailsToggle>(this));
    cv::bleedingedge.setCallback(SA::MakeDelegate<&UpdateHandler::onBleedingEdgeChanged>(this->updateHandler.get()));

    // load global resources
    const int baseDPI = 96;
    const int newDPI = Osu::getUIScale() * baseDPI;

    McFont *defaultFont = resourceManager->loadFont("weblysleekuisb.ttf", "FONT_DEFAULT", 15, true, newDPI);
    this->titleFont = resourceManager->loadFont("SourceSansPro-Semibold.otf", "FONT_OSU_TITLE", 60, true, newDPI);
    this->subTitleFont = resourceManager->loadFont("SourceSansPro-Semibold.otf", "FONT_OSU_SUBTITLE", 21, true, newDPI);
    this->songBrowserFont =
        resourceManager->loadFont("SourceSansPro-Regular.otf", "FONT_OSU_SONGBROWSER", 35, true, newDPI);
    this->songBrowserFontBold =
        resourceManager->loadFont("SourceSansPro-Bold.otf", "FONT_OSU_SONGBROWSER_BOLD", 30, true, newDPI);
    this->fontIcons =
        resourceManager->loadFont("fontawesome-webfont.ttf", "FONT_OSU_ICONS", Icons::icons, 26, true, newDPI);
    this->fonts.push_back(defaultFont);
    this->fonts.push_back(this->titleFont);
    this->fonts.push_back(this->subTitleFont);
    this->fonts.push_back(this->songBrowserFont);
    this->fonts.push_back(this->songBrowserFontBold);
    this->fonts.push_back(this->fontIcons);

    float averageIconHeight = 0.0f;
    for(wchar_t icon : Icons::icons) {
        UString iconString;
        iconString.insert(0, icon);
        const float height = this->fontIcons->getStringHeight(iconString.toUtf8());
        if(height > averageIconHeight) averageIconHeight = height;
    }
    this->fontIcons->setHeight(averageIconHeight);

    if(defaultFont->getDPI() != newDPI) {
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }

    // load skin
    {
        std::string skinFolder{cv::osu_folder.getString()};
        skinFolder.append("/");
        skinFolder.append(cv::osu_folder_sub_skins.getString());
        skinFolder.append(cv::skin.getString());
        skinFolder.append("/");
        if(!this->skin.get())  // the skin may already be loaded by Console::execConfigFile() above
            this->onSkinChange(cv::skin.getString().c_str());

        // enable async skin loading for user-action skin changes (but not during startup)
        cv::skin_async.setValue(1.0f);
    }

    // load subsystems, add them to the screens array
    this->userButton = std::make_unique<UserCard>(BanchoState::get_uid());

    this->songBrowser = std::make_unique<SongBrowser>();
    this->volumeOverlay = std::make_unique<VolumeOverlay>();
    this->tooltipOverlay = std::make_unique<TooltipOverlay>();
    this->optionsMenu = std::make_unique<OptionsMenu>();
    this->mainMenu = std::make_unique<MainMenu>();  // has to be after options menu
    this->backgroundImageHandler = std::make_unique<BackgroundImageHandler>();
    this->modSelector = std::make_unique<ModSelector>();
    this->rankingScreen = std::make_unique<RankingScreen>();
    this->userStats = std::make_unique<UserStatsScreen>();
    this->pauseMenu = std::make_unique<PauseMenu>();
    this->hud = std::make_unique<HUD>();
    this->changelog = std::make_unique<Changelog>();
    this->fposu = std::make_unique<ModFPoSu>();
    this->chat = std::make_unique<Chat>();
    this->lobby = std::make_unique<Lobby>();
    this->room = std::make_unique<RoomScreen>();
    this->prompt = std::make_unique<PromptScreen>();
    this->user_actions = std::make_unique<UIUserContextMenuScreen>();
    this->spectatorScreen = std::make_unique<SpectatorScreen>();

    this->bScreensReady = true;

    this->mainMenu->setVisible(true);

    // update mod settings
    this->updateMods();

    // Init online functionality (multiplayer/leaderboards/etc)
    if(cv::mp_autologin.getBool()) {
        BanchoState::reconnect();
    }

#ifndef _DEBUG
    // don't auto update if this env var is set to anything other than 0 or empty (if it is set)
    const std::string extUpdater = Environment::getEnvVariable("NEOSU_EXTERNAL_UPDATE_PROVIDER");
    if(cv::auto_update.getBool() && (extUpdater.empty() || strtol(extUpdater.c_str(), nullptr, 10) == 0)) {
        bool force_update = cv::bleedingedge.getBool() != cv::is_bleedingedge.getBool();
        this->updateHandler->checkForUpdates(force_update);
    }
#endif

    bool reloading_db = env->getEnvInterop().handle_cmdline_args();
    if(!reloading_db && cv::load_db_immediately.getBool()) {
        // Start loading db early
        this->songBrowser->refreshBeatmaps();
    }

    // Not the type of shader you want players to tweak or delete, so loading from string
    this->actual_flashlight_shader = resourceManager->createShader(
        std::string(reinterpret_cast<const char *>(actual_flashlight_vsh), actual_flashlight_vsh_size()),
        std::string(reinterpret_cast<const char *>(actual_flashlight_fsh), actual_flashlight_fsh_size()),
        "actual_flashlight");

    this->flashlight_shader = resourceManager->createShader(
        std::string(reinterpret_cast<const char *>(flashlight_vsh), flashlight_vsh_size()),
        std::string(reinterpret_cast<const char *>(flashlight_fsh), flashlight_fsh_size()), "flashlight");

    env->setCursorVisible(!McRect{{}, this->vInternalResolution}.contains(mouse->getPos()));
}

// we want to destroy them in the same order as we listed them, to match the old (raw pointer) behavior
// maybe unnecessary, but might avoid violating weird assumptions baked into the existing code
void Osu::destroyAllScreensInOrder() {
    if(!this->bScreensReady) return;
#define X(unused__, name__) /*                                                                     */ \
    this->name__.reset();
    ALL_OSU_SCREENS
#undef X
#undef ALL_OSU_SCREENS
    this->bScreensReady = false;
}

Osu::~Osu() {
    sct_abort();
    lct_set_map(nullptr);
    VolNormalization::shutdown();
    MapCalcThread::shutdown();
    BANCHO::Net::cleanup_networking();

    this->destroyAllScreensInOrder();
}

void Osu::draw() {
    if(!this->skin.get() || this->flashlight_shader == nullptr)  // sanity check
    {
        g->setColor(0xff000000);
        g->fillRect(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());
        return;
    }

    // if we are not using the native window resolution, draw into the buffer
    const bool isBufferedDraw = (g->getResolution() != this->vInternalResolution);
    if(isBufferedDraw) {
        this->backBuffer->enable();
    }

    f32 fadingCursorAlpha = 1.f;

    // draw everything in the correct order
    if(this->isInPlayMode()) {  // if we are playing a beatmap
        const bool isFPoSu = (cv::mod_fposu.getBool());

        if(isFPoSu) this->playfieldBuffer->enable();

        this->map_iface->draw();

        auto actual_flashlight_enabled = cv::mod_actual_flashlight.getBool();
        if(cv::mod_flashlight.getBool() || actual_flashlight_enabled) {
            // Convert screen mouse -> osu mouse pos
            vec2 cursorPos = this->map_iface->getCursorPos();
            vec2 mouse_position = cursorPos - GameRules::getPlayfieldOffset();
            mouse_position /= GameRules::getPlayfieldScaleFactor();

            // Update flashlight position
            double follow_delay = cv::flashlight_follow_delay.getFloat();
            double frame_time = std::min(engine->getFrameTime(), follow_delay);
            float t = frame_time / follow_delay;
            t = t * (2.f - t);
            this->flashlight_position += t * (mouse_position - this->flashlight_position);
            vec2 flashlightPos =
                this->flashlight_position * GameRules::getPlayfieldScaleFactor() + GameRules::getPlayfieldOffset();

            float base_fl_radius = cv::flashlight_radius.getFloat() * GameRules::getPlayfieldScaleFactor();
            float anti_fl_radius = base_fl_radius * 0.625f;
            float fl_radius = base_fl_radius;
            if(this->getScore()->getCombo() >= 200 || cv::flashlight_always_hard.getBool()) {
                anti_fl_radius = base_fl_radius;
                fl_radius *= 0.625f;
            } else if(this->getScore()->getCombo() >= 100) {
                anti_fl_radius = base_fl_radius * 0.8125f;
                fl_radius *= 0.8125f;
            }

            if(cv::mod_flashlight.getBool()) {
                // Dim screen when holding a slider
                float opacity = 1.f;
                if(this->map_iface->holding_slider && !cv::avoid_flashes.getBool()) {
                    opacity = 0.2f;
                }

                this->flashlight_shader->enable();
                this->flashlight_shader->setUniform1f("max_opacity", opacity);
                this->flashlight_shader->setUniform1f("flashlight_radius", fl_radius);
                this->flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x,
                                                      this->getVirtScreenSize().y - flashlightPos.y);

                g->setColor(argb(255, 0, 0, 0));
                g->fillRect(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());

                this->flashlight_shader->disable();
            }
            if(actual_flashlight_enabled) {
                // Brighten screen when holding a slider
                float opacity = 1.f;
                if(this->map_iface->holding_slider && !cv::avoid_flashes.getBool()) {
                    opacity = 0.8f;
                }

                this->actual_flashlight_shader->enable();
                this->actual_flashlight_shader->setUniform1f("max_opacity", opacity);
                this->actual_flashlight_shader->setUniform1f("flashlight_radius", anti_fl_radius);
                this->actual_flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x,
                                                             this->getVirtScreenSize().y - flashlightPos.y);

                g->setColor(argb(255, 0, 0, 0));
                g->fillRect(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());

                this->actual_flashlight_shader->disable();
            }
        }

        if(!isFPoSu) this->hud->draw();

        // quick retry fadeout overlay
        if(this->fQuickRetryTime != 0.0f && this->bQuickRetryDown) {
            float alphaPercent = 1.0f - (this->fQuickRetryTime - engine->getTime()) / cv::quick_retry_delay.getFloat();
            if(engine->getTime() > this->fQuickRetryTime) alphaPercent = 1.0f;

            g->setColor(argb((int)(255 * alphaPercent), 0, 0, 0));
            g->fillRect(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());
        }

        this->pauseMenu->draw();
        this->modSelector->draw();
        this->chat->draw();
        this->user_actions->draw();
        this->optionsMenu->draw();

        if(cv::draw_fps.getBool() && (!isFPoSu)) this->hud->drawFps();

        this->windowManager->draw();

        if(isFPoSu && cv::draw_cursor_ripples.getBool()) this->hud->drawCursorRipples();

        // draw FPoSu cursor trail
        fadingCursorAlpha =
            1.0f -
            std::clamp<float>((float)this->score->getCombo() / cv::mod_fadingcursor_combo.getFloat(), 0.0f, 1.0f);
        if(this->pauseMenu->isVisible() || this->map_iface->isContinueScheduled() || !cv::mod_fadingcursor.getBool())
            fadingCursorAlpha = 1.0f;
        if(isFPoSu && cv::fposu_draw_cursor_trail.getBool())
            this->hud->drawCursorTrail(this->map_iface->getCursorPos(), fadingCursorAlpha);

        if(isFPoSu) {
            this->playfieldBuffer->disable();
            this->fposu->draw();
            this->hud->draw();

            if(cv::draw_fps.getBool()) this->hud->drawFps();
        }
    } else {  // if we are not playing
        this->spectatorScreen->draw();

        this->lobby->draw();
        this->room->draw();

        if(this->songBrowser != nullptr) this->songBrowser->draw();

        this->mainMenu->draw();
        this->changelog->draw();
        this->rankingScreen->draw();
        this->userStats->draw();
        this->chat->draw();
        this->user_actions->draw();
        this->optionsMenu->draw();
        this->modSelector->draw();
        this->prompt->draw();

        if(cv::draw_fps.getBool()) this->hud->drawFps();

        this->windowManager->draw();
    }

    this->tooltipOverlay->draw();
    this->notificationOverlay->draw();
    this->volumeOverlay->draw();

    // loading spinner for some async tasks
    if((this->bSkinLoadScheduled && this->skin.get() != this->skinScheduledToLoad)) {
        this->hud->drawLoadingSmall("");
    }

    // draw cursor
    if(this->isInPlayMode()) {
        vec2 cursorPos = this->map_iface->getCursorPos();
        bool drawSecondTrail = (cv::mod_autoplay.getBool() || cv::mod_autopilot.getBool() ||
                                this->map_iface->is_watching || BanchoState::spectating);
        bool updateAndDrawTrail = true;
        if(cv::mod_fposu.getBool()) {
            cursorPos = this->getVirtScreenSize() / 2.0f;
            updateAndDrawTrail = false;
        }
        this->hud->drawCursor(cursorPos, fadingCursorAlpha, drawSecondTrail, updateAndDrawTrail);
    } else {
        this->hud->drawCursor(mouse->getPos());
    }

    // if we are not using the native window resolution
    if(isBufferedDraw) {
        // draw a scaled version from the buffer to the screen
        this->backBuffer->disable();

        vec2 offset = vec2(g->getResolution().x / 2 - this->vInternalResolution.x / 2,
                           g->getResolution().y / 2 - this->vInternalResolution.y / 2);
        g->setBlending(false);
        if(cv::letterboxing.getBool()) {
            this->backBuffer->draw(offset.x * (1.0f + cv::letterboxing_offset_x.getFloat()),
                                   offset.y * (1.0f + cv::letterboxing_offset_y.getFloat()),
                                   this->vInternalResolution.x, this->vInternalResolution.y);
        } else {
            if(cv::resolution_keep_aspect_ratio.getBool()) {
                const float scale = getImageScaleToFitResolution(this->backBuffer->getSize(), g->getResolution());
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

void Osu::update() {
    if(this->skin.get()) this->skin->update();

    this->fposu->update();

    // only update if not playing
    if(!this->isInPlayModeAndNotPaused()) this->avatarManager->update();

    bool propagate_clicks = true;
    this->forEachScreenWhile<&OsuScreen::mouse_update>(propagate_clicks, &propagate_clicks);

    if(this->music_unpause_scheduled && soundEngine->isReady()) {
        if(this->map_iface->getMusic() != nullptr) {
            soundEngine->play(this->map_iface->getMusic());
        }
        this->music_unpause_scheduled = false;
    }

    // main playfield update
    this->bSeeking = false;
    if(this->isInPlayMode()) {
        map_iface->update();

        // NOTE: force keep loaded background images while playing
        this->backgroundImageHandler->scheduleFreezeCache();

        // skip button clicking
        bool can_skip = map_iface->isInSkippableSection() && !this->bClickedSkipButton;
        can_skip &= !map_iface->isPaused() && !this->volumeOverlay->isBusy();
        if(can_skip) {
            const bool isAnyOsuKeyDown =
                (this->bKeyboardKey1Down || this->bKeyboardKey12Down || this->bKeyboardKey2Down ||
                 this->bKeyboardKey22Down || this->bMouseKey1Down || this->bMouseKey2Down);
            const bool isAnyKeyDown = (isAnyOsuKeyDown || mouse->isLeftDown());

            if(isAnyKeyDown) {
                if(this->hud->getSkipClickRect().contains(mouse->getPos())) {
                    if(!this->bSkipScheduled) {
                        this->bSkipScheduled = true;
                        this->bClickedSkipButton = true;

                        if(BanchoState::is_playing_a_multi_map()) {
                            Packet packet;
                            packet.id = MATCH_SKIP_REQUEST;
                            BANCHO::Net::send_packet(packet);
                        }
                    }
                }
            }
        }

        // skipping
        if(this->bSkipScheduled) {
            const bool isLoading = map_iface->isLoading();

            if(map_iface->isInSkippableSection() && !map_iface->isPaused() && !isLoading) {
                bool can_skip_intro = (cv::skip_intro_enabled.getBool() && map_iface->iCurrentHitObjectIndex < 1);
                bool can_skip_break = (cv::skip_breaks_enabled.getBool() && map_iface->iCurrentHitObjectIndex > 0);
                if(BanchoState::is_playing_a_multi_map()) {
                    can_skip_intro = BanchoState::room.all_players_skipped;
                    can_skip_break = false;
                }

                if(can_skip_intro || can_skip_break) {
                    map_iface->skipEmptySection();
                }
            }

            if(!isLoading) this->bSkipScheduled = false;
        }

        // Reset m_bClickedSkipButton on mouse up
        // We only use m_bClickedSkipButton to prevent seeking when clicking the skip button
        if(this->bClickedSkipButton && !map_iface->isInSkippableSection()) {
            if(!mouse->isLeftDown()) {
                this->bClickedSkipButton = false;
            }
        }

        // scrubbing/seeking
        this->bSeeking = (this->bSeekKey || map_iface->is_watching);
        this->bSeeking &= !this->volumeOverlay->isBusy();
        this->bSeeking &= !BanchoState::is_playing_a_multi_map() && !this->bClickedSkipButton;
        this->bSeeking &= !BanchoState::spectating;
        if(this->bSeeking) {
            f32 mousePosX = std::round(mouse->getPos().x);
            f32 percent = std::clamp<f32>(mousePosX / (f32)this->getVirtScreenWidth(), 0.0f, 1.0f);
            f32 seek_to_ms = percent * (map_iface->getStartTimePlayable() + map_iface->getLengthPlayable());

            if(mouse->isLeftDown()) {
                if(mousePosX != this->fPrevSeekMousePosX || !cv::scrubbing_smooth.getBool()) {
                    this->fPrevSeekMousePosX = mousePosX;

                    // special case: allow cancelling the failing animation here
                    if(map_iface->hasFailed()) map_iface->cancelFailing();

                    // when seeking during gameplay, add nofail for convenience
                    if(!map_iface->is_watching && !cv::mod_nofail.getBool()) {
                        cv::mod_nofail.setValue(true);
                    }

                    map_iface->seekMS(seek_to_ms);
                }
            } else {
                this->fPrevSeekMousePosX = -1.0f;
            }

            if(mouse->isRightDown()) {
                this->fQuickSaveTime = seek_to_ms;
            }
        }

        // quick retry timer
        if(this->bQuickRetryDown && this->fQuickRetryTime != 0.0f && engine->getTime() > this->fQuickRetryTime) {
            this->fQuickRetryTime = 0.0f;

            if(!BanchoState::is_playing_a_multi_map()) {
                map_iface->restart(true);
                map_iface->update();
                this->pauseMenu->setVisible(false);
            }
        }
    }

    // background image cache tick
    this->backgroundImageHandler->update(
        this->songBrowser->isVisible());  // NOTE: must be before the asynchronous ui toggles due to potential 1-frame
                                          // unloads after invisible songbrowser

    // asynchronous ui toggles
    // TODO: this is cancer, why did I even write this section
    if(this->bToggleModSelectionScheduled) {
        this->bToggleModSelectionScheduled = false;
        this->modSelector->setVisible(!this->modSelector->isVisible());

        if(BanchoState::is_in_a_multi_room()) {
            this->room->setVisible(!this->modSelector->isVisible());
        } else if(!this->isInPlayMode() && this->songBrowser != nullptr) {
            this->songBrowser->setVisible(!this->modSelector->isVisible());
        }
    }
    if(this->bToggleOptionsMenuScheduled) {
        this->bToggleOptionsMenuScheduled = false;

        const bool wasFullscreen = this->optionsMenu->isFullscreen();
        this->optionsMenu->setFullscreen(false);
        this->optionsMenu->setVisible(!this->optionsMenu->isVisible());
        if(wasFullscreen) this->mainMenu->setVisible(!this->optionsMenu->isVisible());
    }
    if(this->bToggleChangelogScheduled) {
        this->bToggleChangelogScheduled = false;

        this->mainMenu->setVisible(!this->mainMenu->isVisible());
        this->changelog->setVisible(!this->mainMenu->isVisible());
    }
    if(this->bToggleEditorScheduled) {
        this->bToggleEditorScheduled = false;

        this->mainMenu->setVisible(!this->mainMenu->isVisible());
    }

    this->updateCursorVisibility();

    // endless mod
    if(this->bScheduleEndlessModNextBeatmap) {
        this->bScheduleEndlessModNextBeatmap = false;
        this->songBrowser->playNextRandomBeatmap();
    }

    // multiplayer/networking update
    {
        VPROF_BUDGET_DBG("Bancho::update", VPROF_BUDGETGROUP_UPDATE);
        BANCHO::Net::update_networking();
    }
    {
        VPROF_BUDGET_DBG("Bancho::recvapi", VPROF_BUDGETGROUP_UPDATE);
        BANCHO::Net::receive_api_responses();
    }
    {
        VPROF_BUDGET_DBG("Bancho::recvpkt", VPROF_BUDGETGROUP_UPDATE);
        BANCHO::Net::receive_bancho_packets();
    }

    // skin async loading
    if(this->bSkinLoadScheduled) {
        if((!this->skin.get() || this->skin->isReady()) && this->skinScheduledToLoad != nullptr &&
           this->skinScheduledToLoad->isReady()) {
            this->bSkinLoadScheduled = false;

            if(this->skin.get() != this->skinScheduledToLoad) {
                this->skin.reset(this->skinScheduledToLoad);
            }

            this->skinScheduledToLoad = nullptr;

            // force layout update after all skin elements have been loaded
            this->fireResolutionChanged();

            // notify if done after reload
            if(this->bSkinLoadWasReload) {
                this->bSkinLoadWasReload = false;

                this->notificationOverlay->addNotification(
                    this->skin->getName().length() > 0
                        ? UString::format("Skin reloaded! (%s)", this->skin->getName().c_str())
                        : UString("Skin reloaded!"),
                    0xffffffff, false, 0.75f);
            }
        }
    }

    // (must be before m_bFontReloadScheduled and m_bFireResolutionChangedScheduled are handled!)
    if(this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled) {
        this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = false;

        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }

    // delayed font reloads (must be before layout updates!)
    if(this->bFontReloadScheduled) {
        this->bFontReloadScheduled = false;
        this->reloadFonts();
    }

    // delayed layout updates
    if(this->bFireResolutionChangedScheduled) {
        this->bFireResolutionChangedScheduled = false;
        this->fireResolutionChanged();
    }
}

bool Osu::isInPlayModeAndNotPaused() const { return this->isInPlayMode() && !this->map_iface->isPaused(); }

void Osu::updateMods() {
    this->getScore()->mods = Replay::Mods::from_cvars();
    this->getScore()->setCheated();

    if(this->isInPlayMode()) {
        // notify the possibly running playfield of mod changes
        // e.g. recalculating stacks dynamically if HR is toggled
        this->map_iface->onModUpdate();
    }

    // handle windows key disable/enable
    this->updateWindowsKeyDisable();
}

void Osu::onKeyDown(KeyboardEvent &key) {
    // global hotkeys

    // global hotkey
    if(key == KEY_O && keyboard->isControlDown()) {
        this->toggleOptionsMenu();
        key.consume();
        return;
    }

    // special hotkeys
    // reload & recompile shaders
    if(keyboard->isAltDown() && keyboard->isControlDown() && key == KEY_R) {
        Shader *sliderShader = resourceManager->getShader("slider");
        Shader *cursorTrailShader = resourceManager->getShader("cursortrail");

        if(sliderShader != nullptr) sliderShader->reload();
        if(cursorTrailShader != nullptr) cursorTrailShader->reload();

        key.consume();
    }

    // reload skin (alt)
    if(keyboard->isAltDown() && keyboard->isControlDown() && key == KEY_S) {
        this->onSkinReload();
        key.consume();
    }

    if(key == (KEYCODE)cv::OPEN_SKIN_SELECT_MENU.getInt()) {
        this->optionsMenu->onSkinSelect();
        key.consume();
        return;
    }

    // disable mouse buttons hotkey
    if(key == (KEYCODE)cv::DISABLE_MOUSE_BUTTONS.getInt()) {
        if(cv::disable_mousebuttons.getBool()) {
            cv::disable_mousebuttons.setValue(0.0f);
            this->notificationOverlay->addNotification("Mouse buttons are enabled.");
        } else {
            cv::disable_mousebuttons.setValue(1.0f);
            this->notificationOverlay->addNotification("Mouse buttons are disabled.");
        }
    }

    if(key == (KEYCODE)cv::TOGGLE_MAP_BACKGROUND.getInt()) {
        auto diff = this->map_iface->beatmap;
        if(!diff) {
            this->notificationOverlay->addNotification("No beatmap is currently selected.");
        } else {
            diff->draw_background = !diff->draw_background;
            diff->update_overrides();
        }
        key.consume();
        return;
    }

    // F8 toggle chat
    if(key == (KEYCODE)cv::TOGGLE_CHAT.getInt()) {
        if(!BanchoState::is_online()) {
            this->optionsMenu->askForLoginDetails();
        } else if(this->optionsMenu->isVisible()) {
            // When options menu is open, instead of toggling chat, always open chat
            this->optionsMenu->setVisible(false);
            this->chat->user_wants_chat = true;
            this->chat->updateVisibility();
        } else {
            this->chat->user_wants_chat = !this->chat->user_wants_chat;
            this->chat->updateVisibility();
        }
    }

    // F9 toggle extended chat
    if(key == (KEYCODE)cv::TOGGLE_EXTENDED_CHAT.getInt()) {
        if(!BanchoState::is_online()) {
            this->optionsMenu->askForLoginDetails();
        } else if(this->optionsMenu->isVisible()) {
            // When options menu is open, instead of toggling extended chat, always enable it
            this->optionsMenu->setVisible(false);
            this->chat->user_wants_chat = true;
            this->chat->user_list->setVisible(true);
            this->chat->updateVisibility();
        } else {
            if(this->chat->user_wants_chat) {
                this->chat->user_list->setVisible(!this->chat->user_list->isVisible());
                this->chat->updateVisibility();
            } else {
                this->chat->user_wants_chat = true;
                this->chat->user_list->setVisible(true);
                this->chat->updateVisibility();
            }
        }
    }

    // screenshots
    if(key == (KEYCODE)cv::SAVE_SCREENSHOT.getInt()) this->saveScreenshot();

    // boss key (minimize + mute)
    if(key == (KEYCODE)cv::BOSS_KEY.getInt()) {
        env->minimize();
        this->bWasBossKeyPaused = this->map_iface->isPreviewMusicPlaying();
        this->map_iface->pausePreviewMusic(false);
    }

    // local hotkeys (and gameplay keys)

    // while playing (and not in options)
    if(this->isInPlayMode() && !this->optionsMenu->isVisible() && !this->chat->isVisible()) {
        // instant replay
        if((this->map_iface->isPaused() || this->map_iface->hasFailed())) {
            if(!key.isConsumed() && key == (KEYCODE)cv::INSTANT_REPLAY.getInt()) {
                if(!this->map_iface->is_watching && !BanchoState::spectating) {
                    FinishedScore score;
                    score.replay = this->map_iface->live_replay;
                    score.beatmap_hash = this->map_iface->beatmap->getMD5Hash();
                    score.mods = this->getScore()->mods;

                    score.playerName = BanchoState::get_username();
                    score.player_id = std::max(0, BanchoState::get_uid());

                    f64 pos_seconds = this->map_iface->getTime() - cv::instant_replay_duration.getFloat();
                    u32 pos_ms = (u32)(std::max(0.0, pos_seconds) * 1000.0);
                    this->map_iface->cancelFailing();
                    this->map_iface->watch(score, pos_ms);
                    return;
                }
            }
        }

        // while playing and not paused
        if(!this->map_iface->isPaused()) {
            // K1
            {
                const bool isKeyLeftClick = (key == (KEYCODE)cv::LEFT_CLICK.getInt());
                const bool isKeyLeftClick2 = (key == (KEYCODE)cv::LEFT_CLICK_2.getInt());
                if((!this->bKeyboardKey1Down && isKeyLeftClick) || (!this->bKeyboardKey12Down && isKeyLeftClick2)) {
                    if(isKeyLeftClick2)
                        this->bKeyboardKey12Down = true;
                    else
                        this->bKeyboardKey1Down = true;

                    this->onKey1Change(true, false);

                    if(!this->map_iface->hasFailed()) key.consume();
                } else if(isKeyLeftClick || isKeyLeftClick2) {
                    if(!this->map_iface->hasFailed()) key.consume();
                }
            }

            // K2
            {
                const bool isKeyRightClick = (key == (KEYCODE)cv::RIGHT_CLICK.getInt());
                const bool isKeyRightClick2 = (key == (KEYCODE)cv::RIGHT_CLICK_2.getInt());
                if((!this->bKeyboardKey2Down && isKeyRightClick) || (!this->bKeyboardKey22Down && isKeyRightClick2)) {
                    if(isKeyRightClick2)
                        this->bKeyboardKey22Down = true;
                    else
                        this->bKeyboardKey2Down = true;

                    this->onKey2Change(true, false);

                    if(!this->map_iface->hasFailed()) key.consume();
                } else if(isKeyRightClick || isKeyRightClick2) {
                    if(!this->map_iface->hasFailed()) key.consume();
                }
            }

            // Smoke
            if(key == (KEYCODE)cv::SMOKE.getInt()) {
                this->map_iface->current_keys |= LegacyReplay::Smoke;
                key.consume();
            }

            // handle skipping
            if(key == KEY_ENTER || key == KEY_NUMPAD_ENTER || key == (KEYCODE)cv::SKIP_CUTSCENE.getInt())
                this->bSkipScheduled = true;

            // toggle ui
            if(!key.isConsumed() && key == (KEYCODE)cv::TOGGLE_SCOREBOARD.getInt() && !this->bScoreboardToggleCheck) {
                this->bScoreboardToggleCheck = true;

                if(keyboard->isShiftDown()) {
                    if(!this->bUIToggleCheck) {
                        this->bUIToggleCheck = true;
                        cv::draw_hud.setValue(!cv::draw_hud.getBool());
                        this->notificationOverlay->addNotification(cv::draw_hud.getBool()
                                                                       ? "In-game interface has been enabled."
                                                                       : "In-game interface has been disabled.",
                                                                   0xffffffff, false, 0.1f);

                        key.consume();
                    }
                } else {
                    if(BanchoState::is_playing_a_multi_map()) {
                        cv::draw_scoreboard_mp.setValue(!cv::draw_scoreboard_mp.getBool());
                        this->notificationOverlay->addNotification(
                            cv::draw_scoreboard_mp.getBool() ? "Scoreboard is shown." : "Scoreboard is hidden.",
                            0xffffffff, false, 0.1f);
                    } else {
                        cv::draw_scoreboard.setValue(!cv::draw_scoreboard.getBool());
                        this->notificationOverlay->addNotification(
                            cv::draw_scoreboard.getBool() ? "Scoreboard is shown." : "Scoreboard is hidden.",
                            0xffffffff, false, 0.1f);
                    }

                    key.consume();
                }
            }

            // allow live mod changing while playing
            if(!key.isConsumed() && (key == KEY_F1 || key == (KEYCODE)cv::TOGGLE_MODSELECT.getInt()) &&
               ((KEY_F1 != (KEYCODE)cv::LEFT_CLICK.getInt() && KEY_F1 != (KEYCODE)cv::LEFT_CLICK_2.getInt()) ||
                (!this->bKeyboardKey1Down && !this->bKeyboardKey12Down)) &&
               ((KEY_F1 != (KEYCODE)cv::RIGHT_CLICK.getInt() && KEY_F1 != (KEYCODE)cv::RIGHT_CLICK_2.getInt()) ||
                (!this->bKeyboardKey2Down && !this->bKeyboardKey22Down)) &&
               !this->bF1 && !this->map_iface->hasFailed() &&
               !BanchoState::is_playing_a_multi_map())  // only if not failed though
            {
                this->bF1 = true;
                this->toggleModSelection(true);
            }

            // quick save/load
            if(!BanchoState::is_playing_a_multi_map()) {
                if(key == (KEYCODE)cv::QUICK_SAVE.getInt()) this->fQuickSaveTime = this->map_iface->getTime();

                if(key == (KEYCODE)cv::QUICK_LOAD.getInt()) {
                    // special case: allow cancelling the failing animation here
                    if(this->map_iface->hasFailed()) this->map_iface->cancelFailing();

                    this->map_iface->seekMS(this->fQuickSaveTime);
                }
            }

            // quick seek
            if(!BanchoState::is_playing_a_multi_map()) {
                const bool backward = (key == (KEYCODE)cv::SEEK_TIME_BACKWARD.getInt());
                const bool forward = (key == (KEYCODE)cv::SEEK_TIME_FORWARD.getInt());
                if(backward || forward) {
                    i32 diff = 0;
                    if(backward) diff -= cv::seek_delta.getInt();
                    if(forward) diff += cv::seek_delta.getInt();
                    if(diff != 0) {
                        // special case: allow cancelling the failing animation here
                        if(this->map_iface->hasFailed()) this->map_iface->cancelFailing();

                        this->map_iface->seekMS(this->map_iface->getTime() + diff);
                    }
                }
            }
        }

        // while paused or maybe not paused

        // handle quick restart
        if(((key == (KEYCODE)cv::QUICK_RETRY.getInt() ||
             (keyboard->isControlDown() && !keyboard->isAltDown() && key == KEY_R)) &&
            !this->bQuickRetryDown)) {
            this->bQuickRetryDown = true;
            this->fQuickRetryTime = engine->getTime() + cv::quick_retry_delay.getFloat();
        }

        // handle seeking
        if(key == (KEYCODE)cv::SEEK_TIME.getInt()) this->bSeekKey = true;

        // handle fposu key handling
        this->fposu->onKeyDown(key);
    }

    // forward to all subsystem, if not already consumed
    this->forEachScreenWhile<&OsuScreen::onKeyDown>([&key]() -> bool { return !key.isConsumed(); }, key);

    // special handling, after subsystems, if still not consumed
    if(!key.isConsumed()) {
        // if playing
        if(this->isInPlayMode()) {
            // toggle pause menu
            bool pressed_pause = (key == (KEYCODE)cv::GAME_PAUSE.getInt()) || (key == KEY_ESCAPE);
            pressed_pause &= !this->bEscape;  // ignore repeat events when key is held down
            if(pressed_pause) {
                this->bEscape = true;
                key.consume();

                if(!BanchoState::is_playing_a_multi_map()) {
                    // bit of a misnomer, this pauses OR unpauses the music
                    this->map_iface->pause();
                }

                if(this->pauseMenu->isVisible() && this->map_iface->hasFailed()) {
                    // quit if we try to 'escape' the pause menu when dead (satisfying ragequit mechanic)
                    this->map_iface->stop(true);
                } else {
                    // else just toggle the pause menu
                    this->pauseMenu->setVisible(!this->pauseMenu->isVisible());
                }
            }

            // local offset
            if(key == (KEYCODE)cv::INCREASE_LOCAL_OFFSET.getInt()) {
                long offsetAdd = keyboard->isAltDown() ? 1 : 5;
                this->map_iface->beatmap->setLocalOffset(this->map_iface->beatmap->getLocalOffset() + offsetAdd);
                this->notificationOverlay->addNotification(
                    UString::format("Local beatmap offset set to %ld ms", this->map_iface->beatmap->getLocalOffset()));
            }
            if(key == (KEYCODE)cv::DECREASE_LOCAL_OFFSET.getInt()) {
                long offsetAdd = -(keyboard->isAltDown() ? 1 : 5);
                this->map_iface->beatmap->setLocalOffset(this->map_iface->beatmap->getLocalOffset() + offsetAdd);
                this->notificationOverlay->addNotification(
                    UString::format("Local beatmap offset set to %ld ms", this->map_iface->beatmap->getLocalOffset()));
            }
        }
    }
}

void Osu::onKeyUp(KeyboardEvent &key) {
    // clicks
    {
        // K1
        {
            const bool isKeyLeftClick = (key == (KEYCODE)cv::LEFT_CLICK.getInt());
            const bool isKeyLeftClick2 = (key == (KEYCODE)cv::LEFT_CLICK_2.getInt());
            if((isKeyLeftClick && this->bKeyboardKey1Down) || (isKeyLeftClick2 && this->bKeyboardKey12Down)) {
                if(isKeyLeftClick2)
                    this->bKeyboardKey12Down = false;
                else
                    this->bKeyboardKey1Down = false;

                if(this->isInPlayMode()) this->onKey1Change(false, false);
            }
        }

        // K2
        {
            const bool isKeyRightClick = (key == (KEYCODE)cv::RIGHT_CLICK.getInt());
            const bool isKeyRightClick2 = (key == (KEYCODE)cv::RIGHT_CLICK_2.getInt());
            if((isKeyRightClick && this->bKeyboardKey2Down) || (isKeyRightClick2 && this->bKeyboardKey22Down)) {
                if(isKeyRightClick2)
                    this->bKeyboardKey22Down = false;
                else
                    this->bKeyboardKey2Down = false;

                if(this->isInPlayMode()) this->onKey2Change(false, false);
            }
        }

        // Smoke
        if(this->map_iface && (key == (KEYCODE)cv::SMOKE.getInt())) {
            this->map_iface->current_keys &= ~LegacyReplay::Smoke;
            key.consume();
        }
    }

    // forward to all subsystems, if not consumed
    this->forEachScreenWhile<&OsuScreen::onKeyUp>([&key]() -> bool { return !key.isConsumed(); }, key);

    // misc hotkeys release
    // XXX: handle keypresses in the engine, instead of doing this hacky mess
    if(key == KEY_F1 || key == (KEYCODE)cv::TOGGLE_MODSELECT.getInt()) this->bF1 = false;
    if(key == (KEYCODE)cv::GAME_PAUSE.getInt() || key == KEY_ESCAPE) this->bEscape = false;
    if(key == KEY_LSHIFT || key == KEY_RSHIFT) this->bUIToggleCheck = false;
    if(key == (KEYCODE)cv::TOGGLE_SCOREBOARD.getInt()) {
        this->bScoreboardToggleCheck = false;
        this->bUIToggleCheck = false;
    }
    if(key == (KEYCODE)cv::QUICK_RETRY.getInt() || key == KEY_R) this->bQuickRetryDown = false;
    if(key == (KEYCODE)cv::SEEK_TIME.getInt()) this->bSeekKey = false;

    // handle fposu key handling
    this->fposu->onKeyUp(key);
}

void Osu::stealFocus() { this->forEachScreen<&OsuScreen::stealFocus>(); }

void Osu::onChar(KeyboardEvent &e) {
    this->forEachScreenWhile<&OsuScreen::onChar>([&e]() -> bool { return !e.isConsumed(); }, e);
}

void Osu::onButtonChange(ButtonIndex button, bool down) {
    using enum ButtonIndex;
    if((button != BUTTON_LEFT && button != BUTTON_RIGHT) ||
       (this->isInPlayMode() && !this->map_iface->isPaused() && cv::disable_mousebuttons.getBool()))
        return;

    switch(button) {
        case BUTTON_LEFT: {
            if(!this->bMouseKey1Down && down) {
                this->bMouseKey1Down = true;
                this->onKey1Change(true, true);
            } else if(this->bMouseKey1Down) {
                this->bMouseKey1Down = false;
                this->onKey1Change(false, true);
            }
            break;
        }
        case BUTTON_RIGHT: {
            if(!this->bMouseKey2Down && down) {
                this->bMouseKey2Down = true;
                this->onKey2Change(true, true);
            } else if(this->bMouseKey2Down) {
                this->bMouseKey2Down = false;
                this->onKey2Change(false, true);
            }
            break;
        }
        default:
            break;
    }
}

void Osu::toggleModSelection(bool waitForF1KeyUp) {
    this->bToggleModSelectionScheduled = true;
    this->modSelector->setWaitForF1KeyUp(waitForF1KeyUp);
}

void Osu::toggleSongBrowser() {
    if(BanchoState::spectating) return;

    if(this->mainMenu->isVisible() && this->optionsMenu->isVisible()) this->optionsMenu->setVisible(false);

    this->songBrowser->setVisible(!this->songBrowser->isVisible());

    // try refreshing if we have no beatmaps and are not already refreshing
    if(this->songBrowser->isVisible() && this->songBrowser->beatmapsets.size() == 0 && !this->songBrowser->bBeatmapRefreshScheduled) {
        this->songBrowser->refreshBeatmaps();
    }

    if(BanchoState::is_in_a_multi_room()) {
        // We didn't select a map; revert to previously selected one
        auto map = this->songBrowser->lastSelectedBeatmap;
        if(map != nullptr) {
            BanchoState::room.map_name = UString::format("%s - %s [%s]", map->getArtist().c_str(),
                                                         map->getTitle().c_str(), map->getDifficultyName().c_str());
            BanchoState::room.map_md5 = map->getMD5Hash();
            BanchoState::room.map_id = map->getID();

            Packet packet;
            packet.id = MATCH_CHANGE_SETTINGS;
            BanchoState::room.pack(packet);
            BANCHO::Net::send_packet(packet);

            this->room->on_map_change();
        }
    } else {
        this->mainMenu->setVisible(!this->songBrowser->isVisible());
    }

    this->updateConfineCursor();
}

void Osu::toggleOptionsMenu() {
    this->bToggleOptionsMenuScheduled = true;
    this->bOptionsMenuFullscreen = this->mainMenu->isVisible();
}

void Osu::toggleChangelog() { this->bToggleChangelogScheduled = true; }

void Osu::toggleEditor() { this->bToggleEditorScheduled = true; }

void Osu::reloadMapInterface() { this->map_iface = std::make_unique<BeatmapInterface>(); }

void Osu::saveScreenshot() {
    static i32 screenshotNumber = 0;

    if(!env->directoryExists("screenshots") && !env->createDirectory("screenshots")) {
        this->notificationOverlay->addNotification("Error: Couldn't create screenshots folder.", 0xffff0000, false,
                                                   3.0f);
        return;
    }

    while(env->fileExists(fmt::format("screenshots/screenshot{}.png", screenshotNumber))) screenshotNumber++;

    const auto screenshotFilename{fmt::format("screenshots/screenshot{}.png", screenshotNumber)};

    constexpr u8 screenshotChannels{3};
    std::vector<u8> pixels = g->getScreenshot(false);

    if(pixels.empty()) {
        static uint8_t once = 0;
        if(!once++)
            this->notificationOverlay->addNotification("Error: Couldn't grab a screenshot :(", 0xffff0000, false, 3.0f);
        debugLog("failed to get pixel data for screenshot");
        return;
    }

    const f32 outerWidth = g->getResolution().x;
    const f32 outerHeight = g->getResolution().y;
    const f32 innerWidth = this->vInternalResolution.x;
    const f32 innerHeight = this->vInternalResolution.y;

    soundEngine->play(this->skin->getShutter());
    this->notificationOverlay->addToast(UString::format("Saved screenshot to %s", screenshotFilename.c_str()),
                                        CHAT_TOAST, [screenshotFilename] { env->openFileBrowser(screenshotFilename); });

    // don't need cropping
    if(static_cast<i32>(innerWidth) == static_cast<i32>(outerWidth) &&
       static_cast<i32>(innerHeight) == static_cast<i32>(outerHeight)) {
        Image::saveToImage(pixels.data(), static_cast<i32>(innerWidth), static_cast<i32>(innerHeight),
                           screenshotChannels, screenshotFilename);
        return;
    }

    // need cropping
    f32 offsetXpct = 0, offsetYpct = 0;
    if((g->getResolution() != this->vInternalResolution) && cv::letterboxing.getBool()) {
        offsetXpct = cv::letterboxing_offset_x.getFloat();
        offsetYpct = cv::letterboxing_offset_y.getFloat();
    }

    const i32 startX = std::clamp<i32>(static_cast<i32>((outerWidth - innerWidth) * (1 + offsetXpct) / 2), 0,
                                       static_cast<i32>(outerWidth - innerWidth));
    const i32 startY = std::clamp<i32>(static_cast<i32>((outerHeight - innerHeight) * (1 + offsetYpct) / 2), 0,
                                       static_cast<i32>(outerHeight - innerHeight));

    std::vector<u8> croppedPixels(static_cast<size_t>(innerWidth * innerHeight * screenshotChannels));

    for(sSz y = 0; y < static_cast<sSz>(innerHeight); ++y) {
        auto srcRowStart = pixels.begin() + ((startY + y) * static_cast<sSz>(outerWidth) + startX) * screenshotChannels;
        auto destRowStart = croppedPixels.begin() + (y * static_cast<sSz>(innerWidth)) * screenshotChannels;
        // copy the entire row
        std::ranges::copy_n(srcRowStart, static_cast<sSz>(innerWidth) * screenshotChannels, destRowStart);
    }

    Image::saveToImage(croppedPixels.data(), static_cast<i32>(innerWidth), static_cast<i32>(innerHeight),
                       screenshotChannels, screenshotFilename);
}

void Osu::onPlayEnd(FinishedScore score, bool quit, bool /*aborted*/) {
    cv::snd_change_check_interval.setValue(cv::snd_change_check_interval.getDefaultFloat());

    if(!quit) {
        if(!cv::mod_endless.getBool()) {
            // NOTE: the order of these two calls matters
            this->rankingScreen->setScore(std::move(score));
            this->rankingScreen->setBeatmapInfo(this->map_iface->beatmap);

            soundEngine->play(this->skin->getApplause());
        } else {
            this->bScheduleEndlessModNextBeatmap = true;
            return;  // nothing more to do here
        }
    }

    this->mainMenu->setVisible(false);
    this->modSelector->setVisible(false);
    this->pauseMenu->setVisible(false);

    if(this->songBrowser != nullptr) this->songBrowser->onPlayEnd(quit);

    // When playing in multiplayer, screens are toggled in Room
    if(!BanchoState::is_playing_a_multi_map()) {
        if(quit) {
            this->toggleSongBrowser();
        } else {
            this->rankingScreen->setVisible(true);
        }
    }

    this->updateConfineCursor();
    this->updateWindowsKeyDisable();
}

float Osu::getDifficultyMultiplier() {
    float difficultyMultiplier = 1.0f;

    if(cv::mod_hardrock.getBool()) difficultyMultiplier = 1.4f;
    if(cv::mod_easy.getBool()) difficultyMultiplier = 0.5f;

    return difficultyMultiplier;
}

float Osu::getCSDifficultyMultiplier() {
    float difficultyMultiplier = 1.0f;

    if(cv::mod_hardrock.getBool()) difficultyMultiplier = 1.3f;  // different!
    if(cv::mod_easy.getBool()) difficultyMultiplier = 0.5f;

    return difficultyMultiplier;
}

float Osu::getScoreMultiplier() {
    float multiplier = 1.0f;

    // Dumb formula, but the values for HT/DT were dumb to begin with
    f32 s = this->map_iface->getSpeedMultiplier();
    if(s > 1.f) {
        multiplier *= (0.24 * s) + 0.76;
    } else if(s < 1.f) {
        multiplier *= 0.008 * std::exp(4.81588 * s);
    }

    if(cv::mod_easy.getBool() || (cv::mod_nofail.getBool() && !cv::mod_scorev2.getBool())) multiplier *= 0.50f;
    if(cv::mod_hardrock.getBool()) {
        if(cv::mod_scorev2.getBool())
            multiplier *= 1.10f;
        else
            multiplier *= 1.06f;
    }
    if(cv::mod_flashlight.getBool()) multiplier *= 1.12f;
    if(cv::mod_hidden.getBool()) multiplier *= 1.06f;
    if(cv::mod_spunout.getBool()) multiplier *= 0.90f;

    if(cv::mod_relax.getBool() || cv::mod_autopilot.getBool()) multiplier *= 0.f;

    return multiplier;
}

float Osu::getAnimationSpeedMultiplier() {
    float animationSpeedMultiplier = this->map_iface->getSpeedMultiplier();

    if(cv::animation_speed_override.getFloat() >= 0.0f) return std::max(cv::animation_speed_override.getFloat(), 0.05f);

    return animationSpeedMultiplier;
}

bool Osu::isInPlayMode() const { return (this->songBrowser != nullptr && this->songBrowser->bHasSelectedAndIsPlaying); }

bool Osu::shouldFallBackToLegacySliderRenderer() {
    return cv::force_legacy_slider_renderer.getBool() || cv::mod_wobble.getBool() || cv::mod_wobble2.getBool() ||
           cv::mod_minimize.getBool() || this->modSelector->isCSOverrideSliderActive()
        /* || (this->osu_playfield_rotation->getFloat() < -0.01f || m_osu_playfield_rotation->getFloat() > 0.01f)*/;
}

void Osu::onResolutionChanged(vec2 newResolution) {
    debugLog("Osu::onResolutionChanged({:d}, {:d}), minimized = {:d}", (int)newResolution.x, (int)newResolution.y,
             (int)engine->isMinimized());

    if(engine->isMinimized()) return;  // ignore if minimized

    const float prevUIScale = getUIScale();

    // save setting
    auto res_str = UString::format("%ix%i", (i32)newResolution.x, (i32)newResolution.y);
    if(env->isFullscreen()) {
        cv::resolution.setValue(res_str, false);
    } else {
        cv::windowed_resolution.setValue(res_str, false);
    }

    // We just force disable letterboxing while windowed.
    if(cv::letterboxing.getBool() && env->isFullscreen()) {
        // clamp upwards to internal resolution (osu_resolution)
        if(this->vInternalResolution.x < this->vInternalResolution2.x)
            this->vInternalResolution.x = this->vInternalResolution2.x;
        if(this->vInternalResolution.y < this->vInternalResolution2.y)
            this->vInternalResolution.y = this->vInternalResolution2.y;

        // clamp downwards to engine resolution
        if(newResolution.x < this->vInternalResolution.x) this->vInternalResolution.x = newResolution.x;
        if(newResolution.y < this->vInternalResolution.y) this->vInternalResolution.y = newResolution.y;
    } else {
        this->vInternalResolution = newResolution;
    }

    // update dpi specific engine globals
    cv::ui_scrollview_scrollbarwidth.setValue(15.0f * Osu::getUIScale());  // not happy with this as a convar

    // interfaces
    this->forEachScreen<&OsuScreen::onResolutionChange>(this->vInternalResolution);

    // rendertargets
    this->rebuildRenderTargets();

    // mouse scale/offset
    this->updateMouseSettings();

    // cursor clipping
    this->updateConfineCursor();

    // see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323
    struct LossyComparisonToFixExcessFPUPrecisionBugBecauseFuckYou {
        static bool equalEpsilon(float f1, float f2) { return std::abs(f1 - f2) < 0.00001f; }
    };

    // a bit hacky, but detect resolution-specific-dpi-scaling changes and force a font and layout reload after a 1
    // frame delay (1/2)
    if(!LossyComparisonToFixExcessFPUPrecisionBugBecauseFuckYou::equalEpsilon(getUIScale(), prevUIScale))
        this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = true;
}

void Osu::onDPIChanged() {
    // delay
    this->bFontReloadScheduled = true;
    this->bFireResolutionChangedScheduled = true;
}

void Osu::rebuildRenderTargets() {
    debugLog("Osu::rebuildRenderTargets: {:f}x{:f}", this->vInternalResolution.x, this->vInternalResolution.y);

    this->backBuffer->rebuild(0, 0, this->vInternalResolution.x, this->vInternalResolution.y);

    if(cv::mod_fposu.getBool())
        this->playfieldBuffer->rebuild(0, 0, this->vInternalResolution.x, this->vInternalResolution.y);
    else
        this->playfieldBuffer->rebuild(0, 0, 64, 64);

    this->sliderFrameBuffer->rebuild(0, 0, this->vInternalResolution.x, this->vInternalResolution.y,
                                     Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);

    this->AAFrameBuffer->rebuild(0, 0, this->vInternalResolution.x, this->vInternalResolution.y);

    if(cv::mod_mafham.getBool()) {
        this->frameBuffer->rebuild(0, 0, this->vInternalResolution.x, this->vInternalResolution.y);
        this->frameBuffer2->rebuild(0, 0, this->vInternalResolution.x, this->vInternalResolution.y);
    } else {
        this->frameBuffer->rebuild(0, 0, 64, 64);
        this->frameBuffer2->rebuild(0, 0, 64, 64);
    }
}

void Osu::reloadFonts() {
    const int baseDPI = 96;
    const int newDPI = Osu::getUIScale() * baseDPI;

    for(McFont *font : this->fonts) {
        if(font->getDPI() != newDPI) {
            font->setDPI(newDPI);
            resourceManager->reloadResource(font);
        }
    }
}

void Osu::updateMouseSettings() {
    // mouse scaling & offset
    vec2 offset = vec2(0, 0);
    vec2 scale = vec2(1, 1);
    if((g->getResolution() != this->vInternalResolution) && cv::letterboxing.getBool()) {
        offset = -vec2((engine->getScreenWidth() / 2.f - this->vInternalResolution.x / 2.f) *
                           (1.0f + cv::letterboxing_offset_x.getFloat()),
                       (engine->getScreenHeight() / 2.f - this->vInternalResolution.y / 2.f) *
                           (1.0f + cv::letterboxing_offset_y.getFloat()));

        scale = vec2(this->vInternalResolution.x / engine->getScreenWidth(),
                     this->vInternalResolution.y / engine->getScreenHeight());
    }

    mouse->setOffset(offset);
    mouse->setScale(scale);
}

void Osu::updateWindowsKeyDisable() {
    if(cv::debug_osu.getBool()) debugLog("Osu::updateWindowsKeyDisable()");
    const bool isPlayerPlaying = engine->hasFocus() && this->isInPlayMode() &&
                                 (!this->map_iface->isPaused() || this->map_iface->isRestartScheduled()) &&
                                 !cv::mod_autoplay.getBool();
    if(cv::win_disable_windows_key_while_playing.getBool()) {
        env->grabKeyboard(isPlayerPlaying);
    } else {
        env->grabKeyboard(false);
    }

    // this is kind of a weird place to put this, but we don't care about text input when in gameplay
    // on some platforms, text input being enabled might result in an on-screen keyboard showing up
    // TODO: check if this breaks chat while playing
    env->listenToTextInput(!isPlayerPlaying);
}

void Osu::fireResolutionChanged() { this->onResolutionChanged(this->vInternalResolution); }

void Osu::onWindowedResolutionChanged(const UString & /*oldValue*/, const UString &args) {
    if(env->isFullscreen()) return;
    if(args.length() < 7) return;

    std::vector<UString> resolution = args.split("x");
    if(resolution.size() != 2) {
        debugLog(
            "Error: Invalid parameter count for command 'osu_resolution'! (Usage: e.g. \"osu_resolution 1280x720\")");
        return;
    }

    int width = resolution[0].toFloat();
    int height = resolution[1].toFloat();
    if(width < 300 || height < 240) {
        debugLog("Error: Invalid values for resolution for command 'osu_resolution'!");
        return;
    }

    env->setWindowSize(width, height);
    env->center();
}

void Osu::onInternalResolutionChanged(const UString & /*oldValue*/, const UString &args) {
    if(!env->isFullscreen()) return;
    if(args.length() < 7) return;

    std::vector<UString> resolution = args.split("x");
    if(resolution.size() != 2) {
        debugLog(
            "Error: Invalid parameter count for command 'osu_resolution'! (Usage: e.g. \"osu_resolution 1280x720\")");
        return;
    }

    int width = resolution[0].toFloat();
    int height = resolution[1].toFloat();
    if(width < 300 || height < 240) {
        debugLog("Error: Invalid values for resolution for command 'osu_resolution'!");
        return;
    }

    const float prevUIScale = getUIScale();
    vec2 newInternalResolution = vec2(width, height);

    // clamp requested internal resolution to current renderer resolution
    // however, this could happen while we are transitioning into fullscreen. therefore only clamp when not in
    // fullscreen or not in fullscreen transition
    bool isTransitioningIntoFullscreenHack =
        g->getResolution().x < env->getNativeScreenSize().x || g->getResolution().y < env->getNativeScreenSize().y;
    if(!env->isFullscreen() || !isTransitioningIntoFullscreenHack) {
        if(newInternalResolution.x > g->getResolution().x) newInternalResolution.x = g->getResolution().x;
        if(newInternalResolution.y > g->getResolution().y) newInternalResolution.y = g->getResolution().y;
    }

    // store, then force onResolutionChanged()
    this->vInternalResolution = newInternalResolution;
    this->vInternalResolution2 = newInternalResolution;
    this->fireResolutionChanged();

    // a bit hacky, but detect resolution-specific-dpi-scaling changes and force a font and layout reload after a 1
    // frame delay (2/2)
    if(getUIScale() != prevUIScale) this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = true;
}

void Osu::onFocusGained() {
    // cursor clipping
    this->updateConfineCursor();

    if(this->bWasBossKeyPaused) {
        this->bWasBossKeyPaused = false;

        // make sure playfield is fully constructed before accessing it
        this->map_iface->pausePreviewMusic();
    }

    this->updateWindowsKeyDisable();
    this->volumeOverlay->gainFocus();
}

void Osu::onFocusLost() {
    if(this->isInPlayMode() && !this->map_iface->isPaused() && cv::pause_on_focus_loss.getBool()) {
        if(!BanchoState::is_playing_a_multi_map() && !this->map_iface->is_watching && !BanchoState::spectating) {
            this->map_iface->pause(false);
            this->pauseMenu->setVisible(true);
            this->modSelector->setVisible(false);
        }
    }

    this->updateWindowsKeyDisable();
    this->volumeOverlay->loseFocus();

    // release cursor clip
    this->updateConfineCursor();
}

void Osu::onMinimized() { this->volumeOverlay->loseFocus(); }

bool Osu::onShutdown() {
    debugLog("Osu::onShutdown()");

    if(!cv::alt_f4_quits_even_while_playing.getBool() && this->isInPlayMode()) {
        this->map_iface->stop();
        return false;
    }

    // save everything
    this->optionsMenu->save();
    db->save();

    BanchoState::disconnect();

    return true;
}

void Osu::onSkinReload() {
    this->bSkinLoadWasReload = true;
    this->onSkinChange(cv::skin.getString().c_str());
}

void Osu::onSkinChange(const UString &newValue) {
    if(this->skin.get()) {
        if(this->bSkinLoadScheduled || this->skinScheduledToLoad != nullptr) return;
        if(newValue.length() < 1) return;
    }

    std::string newString{newValue.utf8View()};

    if(newString == "default") {
        this->skinScheduledToLoad = new Skin(newString.c_str(), MCENGINE_DATA_DIR "materials/default/", true);
        if(!this->skin.get()) this->skin.reset(this->skinScheduledToLoad);
        this->bSkinLoadScheduled = true;
        return;
    }

    std::string neosuSkinFolder = MCENGINE_DATA_DIR "skins/";
    neosuSkinFolder.append(newString);
    neosuSkinFolder.append("/");
    if(env->directoryExists(neosuSkinFolder)) {
        this->skinScheduledToLoad = new Skin(newString.c_str(), neosuSkinFolder, false);
    } else {
        std::string ppySkinFolder{cv::osu_folder.getString()};
        ppySkinFolder.append("/");
        ppySkinFolder.append(cv::osu_folder_sub_skins.getString());
        ppySkinFolder.append(newString);
        ppySkinFolder.append("/");
        std::string sf = ppySkinFolder;
        this->skinScheduledToLoad = new Skin(newString.c_str(), sf, false);
    }

    // initial load
    if(!this->skin.get()) this->skin.reset(this->skinScheduledToLoad);

    this->bSkinLoadScheduled = true;
}

void Osu::updateAnimationSpeed() {
    if(this->getSkin() != nullptr) {
        float speed = this->getAnimationSpeedMultiplier() / this->map_iface->getSpeedMultiplier();
        this->getSkin()->setAnimationSpeed(speed >= 0.0f ? speed : 0.0f);
    }
}

void Osu::onAnimationSpeedChange() { this->updateAnimationSpeed(); }

void Osu::onSpeedChange(const UString &newValue) {
    float speed = newValue.toFloat();
    this->map_iface->setSpeed(speed >= 0.0f ? speed : this->map_iface->getSpeedMultiplier());
    this->updateAnimationSpeed();

    // Update mod menu UI
    {
        // DT/HT buttons
        cv::mod_doubletime_dummy.setValue(speed == 1.5f, false);
        this->modSelector->modButtonDoubletime->setOn(speed == 1.5f, true);
        cv::mod_halftime_dummy.setValue(speed == 0.75f, false);
        this->modSelector->modButtonHalftime->setOn(speed == 0.75f, true);
        this->modSelector->updateButtons(true);

        // Speed slider ('+1' to compensate for turn-off area of the override sliders)
        this->modSelector->speedSlider->setValue(speed + 1.f, true, false);
        this->modSelector->updateOverrideSliderLabels();

        // Score multiplier
        this->modSelector->updateScoreMultiplierLabelText();
    }
}

void Osu::onThumbnailsToggle() {
    this->songBrowser->thumbnailYRatio = cv::draw_songbrowser_thumbnails.getBool() ? 1.333333f : 0.f;
}

void Osu::onPlayfieldChange() { this->map_iface->onModUpdate(); }

void Osu::onUIScaleChange(const UString &oldValue, const UString &newValue) {
    const float oldVal = oldValue.toFloat();
    const float newVal = newValue.toFloat();

    if(oldVal != newVal) {
        // delay
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }
}

void Osu::onUIScaleToDPIChange(const UString &oldValue, const UString &newValue) {
    const bool oldVal = oldValue.toFloat() > 0.0f;
    const bool newVal = newValue.toFloat() > 0.0f;

    if(oldVal != newVal) {
        // delay
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }
}

void Osu::onLetterboxingChange(const UString &oldValue, const UString &newValue) {
    bool oldVal = oldValue.toFloat() > 0.0f;
    bool newVal = newValue.toFloat() > 0.0f;

    if(oldVal != newVal) this->bFireResolutionChangedScheduled = true;  // delay
}

// Here, "cursor" is the Windows mouse cursor, not the game cursor
void Osu::updateCursorVisibility() {
    if(!env->isCursorInWindow()) {
        return;  // don't do anything
    }

    const bool currently_visible = env->isCursorVisible();
    bool forced_visible = false;

    if(this->isInPlayMode() && (cv::mod_autoplay.getBool() || cv::mod_autopilot.getBool() ||
                                this->map_iface->is_watching || BanchoState::spectating)) {
        forced_visible = true;
    }

    bool desired_vis = forced_visible;

    // if it's not forced visible, check whether it's inside the internal window
    if(!forced_visible) {
        const bool internal_contains_mouse = McRect{{}, this->vInternalResolution}.contains(mouse->getPos());
        if(internal_contains_mouse) {
            desired_vis = false;
        } else {
            desired_vis = true;
        }
    }

    // only change if it's different from the current mouse state
    if(desired_vis != currently_visible) {
        if(cv::debug_mouse.getBool()) {
            debugLog("current: {} desired: {}", currently_visible, desired_vis);
        }
        env->setCursorVisible(desired_vis);
    }
}

void Osu::updateConfineCursor() {
    McRect clip{};
    const bool effectivelyFS = env->isFullscreen() || env->isFullscreenWindowedBorderless();
    const bool playing = this->isInPlayMode();
    // we need relative mode (rawinput) for fposu without absolute mode
    const bool playing_fposu_nonabs = (playing && cv::mod_fposu.getBool() && !cv::fposu_absolute_mode.getBool());

    const bool might_confine = (playing_fposu_nonabs) ||                                         //
                               (effectivelyFS && cv::confine_cursor_fullscreen.getBool()) ||     //
                               (!effectivelyFS && cv::confine_cursor_windowed.getBool()) ||      //
                               (playing && !(this->pauseMenu && this->pauseMenu->isVisible()));  //

    const bool force_no_confine = !engine->hasFocus() ||                                            //
                                  (!playing_fposu_nonabs && cv::confine_cursor_never.getBool()) ||  //
                                  this->getModAuto() ||                                             //
                                  this->getModAutopilot() ||                                        //
                                  (this->map_iface && this->map_iface->is_watching) ||              //
                                  BanchoState::spectating;                                          //

    bool confine_cursor = might_confine && !force_no_confine;
    if(confine_cursor) {
        if((g->getResolution() != this->vInternalResolution) && cv::letterboxing.getBool()) {
            clip = McRect{(f32)(-mouse->getOffset().x), (f32)(-mouse->getOffset().y), this->vInternalResolution.x,
                          this->vInternalResolution.y};
        } else {
            clip = McRect{0, 0, (f32)(engine->getScreenWidth()), (f32)(engine->getScreenHeight())};
        }
    }

    if(cv::debug_mouse.getBool())
        debugLog("confined: {}, cliprect: {},{},{},{}", confine_cursor, clip.getMinX(), clip.getMinY(),
                 clip.getMaxX(), clip.getMaxY());

    env->setCursorClip(confine_cursor, clip);
}

void Osu::updateOsuFolder() {
    cv::osu_folder.setValue(env->normalizeDirectory(cv::osu_folder.getString()), false);

    if(this->optionsMenu) {
        this->optionsMenu->osuFolderTextbox->stealFocus();
        this->optionsMenu->osuFolderTextbox->setText(UString{cv::osu_folder.getString()});
    }
}

void Osu::onKey1Change(bool pressed, bool isMouse) {
    int numKeys1Down = 0;
    if(this->bKeyboardKey1Down) numKeys1Down++;
    if(this->bKeyboardKey12Down) numKeys1Down++;
    if(this->bMouseKey1Down) numKeys1Down++;

    // all key1 keys (incl. multiple bindings) act as one single key with state handover
    const bool isKeyPressed1Allowed = (numKeys1Down == 1);

    // WARNING: if paused, keyReleased*() will be called out of sequence every time due to the fix.
    //          do not put actions in it
    // NOTE: allow keyup even while beatmap is paused, to correctly not-continue immediately due to pressed keys
    if(this->isInPlayMode()) {
        if(!(isMouse && cv::disable_mousebuttons.getBool())) {
            // quickfix
            if(cv::disable_mousebuttons.getBool()) this->bMouseKey1Down = false;

            if(pressed && isKeyPressed1Allowed && !this->map_iface->isPaused())  // see above note
                this->map_iface->keyPressed1(isMouse);
            else if(!this->bKeyboardKey1Down && !this->bKeyboardKey12Down && !this->bMouseKey1Down)
                this->map_iface->keyReleased1(isMouse);
        }
    }

    // cursor anim + ripples
    const bool doAnimate =
        !(this->isInPlayMode() && !this->map_iface->isPaused() && isMouse && cv::disable_mousebuttons.getBool());
    if(doAnimate) {
        if(pressed && isKeyPressed1Allowed) {
            this->hud->animateCursorExpand();
            this->hud->addCursorRipple(mouse->getPos());
        } else if(!this->bKeyboardKey1Down && !this->bKeyboardKey12Down && !this->bMouseKey1Down &&
                  !this->bKeyboardKey2Down && !this->bKeyboardKey22Down && !this->bMouseKey2Down)
            this->hud->animateCursorShrink();
    }
}

void Osu::onKey2Change(bool pressed, bool isMouse) {
    int numKeys2Down = 0;
    if(this->bKeyboardKey2Down) numKeys2Down++;
    if(this->bKeyboardKey22Down) numKeys2Down++;
    if(this->bMouseKey2Down) numKeys2Down++;

    // all key2 keys (incl. multiple bindings) act as one single key with state handover
    const bool isKeyPressed2Allowed = (numKeys2Down == 1);

    // WARNING: if paused, keyReleased*() will be called out of sequence every time due to the fix.
    //          do not put actions in it
    // NOTE: allow keyup even while beatmap is paused, to correctly not-continue immediately due to pressed keys
    if(this->isInPlayMode()) {
        if(!(isMouse && cv::disable_mousebuttons.getBool())) {
            // quickfix
            if(cv::disable_mousebuttons.getBool()) this->bMouseKey2Down = false;

            if(pressed && isKeyPressed2Allowed && !this->map_iface->isPaused())  // see above note
                this->map_iface->keyPressed2(isMouse);
            else if(!this->bKeyboardKey2Down && !this->bKeyboardKey22Down && !this->bMouseKey2Down)
                this->map_iface->keyReleased2(isMouse);
        }
    }

    // cursor anim + ripples
    const bool doAnimate =
        !(this->isInPlayMode() && !this->map_iface->isPaused() && isMouse && cv::disable_mousebuttons.getBool());
    if(doAnimate) {
        if(pressed && isKeyPressed2Allowed) {
            this->hud->animateCursorExpand();
            this->hud->addCursorRipple(mouse->getPos());
        } else if(!this->bKeyboardKey2Down && !this->bKeyboardKey22Down && !this->bMouseKey2Down &&
                  !this->bKeyboardKey1Down && !this->bKeyboardKey12Down && !this->bMouseKey1Down)
            this->hud->animateCursorShrink();
    }
}

void Osu::onModMafhamChange() { this->rebuildRenderTargets(); }

void Osu::onModFPoSuChange() { this->rebuildRenderTargets(); }

void Osu::onModFPoSu3DChange() { this->rebuildRenderTargets(); }

void Osu::onModFPoSu3DSpheresChange() { this->rebuildRenderTargets(); }

void Osu::onModFPoSu3DSpheresAAChange() { this->rebuildRenderTargets(); }

void Osu::onLetterboxingOffsetChange() {
    this->updateMouseSettings();
    this->updateConfineCursor();
}

void Osu::onUserCardChange(const UString &new_username) {
    // NOTE: force update options textbox to avoid shutdown inconsistency
    this->getOptionsMenu()->setUsername(new_username);
    this->userButton->setID(BanchoState::get_uid());
}

float Osu::getImageScaleToFitResolution(vec2 size, vec2 resolution) {
    return resolution.x / size.x > resolution.y / size.y ? resolution.y / size.y : resolution.x / size.x;
}

float Osu::getImageScaleToFitResolution(Image *img, vec2 resolution) {
    return getImageScaleToFitResolution(vec2(img->getWidth(), img->getHeight()), resolution);
}

float Osu::getImageScaleToFillResolution(vec2 size, vec2 resolution) {
    return resolution.x / size.x < resolution.y / size.y ? resolution.y / size.y : resolution.x / size.x;
}

float Osu::getImageScaleToFillResolution(Image *img, vec2 resolution) {
    return getImageScaleToFillResolution(vec2(img->getWidth(), img->getHeight()), resolution);
}

float Osu::getImageScale(vec2 size, float osuSize) {
    int swidth = osu->getVirtScreenWidth();
    int sheight = osu->getVirtScreenHeight();

    if(swidth * 3 > sheight * 4)
        swidth = sheight * 4 / 3;
    else
        sheight = swidth * 3 / 4;

    const float xMultiplier = swidth / osuBaseResolution.x;
    const float yMultiplier = sheight / osuBaseResolution.y;

    const float xDiameter = osuSize * xMultiplier;
    const float yDiameter = osuSize * yMultiplier;

    return xDiameter / size.x > yDiameter / size.y ? xDiameter / size.x : yDiameter / size.y;
}

float Osu::getImageScale(Image *img, float osuSize) {
    return getImageScale(vec2(img->getWidth(), img->getHeight()), osuSize);
}

float Osu::getUIScale(float osuResolutionRatio) {
    int swidth = osu->getVirtScreenWidth();
    int sheight = osu->getVirtScreenHeight();

    if(swidth * 3 > sheight * 4)
        swidth = sheight * 4 / 3;
    else
        sheight = swidth * 3 / 4;

    const float xMultiplier = swidth / osuBaseResolution.x;
    const float yMultiplier = sheight / osuBaseResolution.y;

    const float xDiameter = osuResolutionRatio * xMultiplier;
    const float yDiameter = osuResolutionRatio * yMultiplier;

    return xDiameter > yDiameter ? xDiameter : yDiameter;
}

float Osu::getUIScale() {
    if(osu != nullptr) {
        if(osu->getVirtScreenWidth() < cv::ui_scale_to_dpi_minimum_width.getInt() ||
           osu->getVirtScreenHeight() < cv::ui_scale_to_dpi_minimum_height.getInt())
            return cv::ui_scale.getFloat();
    } else if(engine->getScreenWidth() < cv::ui_scale_to_dpi_minimum_width.getInt() ||
              engine->getScreenHeight() < cv::ui_scale_to_dpi_minimum_height.getInt())
        return cv::ui_scale.getFloat();

    return ((cv::ui_scale_to_dpi.getBool() ? env->getDPIScale() : 1.0f) * cv::ui_scale.getFloat());
}

void Osu::setupSoloud() {
    // need to save this state somewhere to share data between callback stages
    static bool was_playing = false;
    static unsigned long prev_position_ms = 0;

    static auto outputChangedBeforeCallback = []() -> void {
        if(osu && osu->getMapInterface() && osu->getMapInterface()->getMusic()) {
            was_playing = osu->getMapInterface()->getMusic()->isPlaying();
            prev_position_ms = osu->getMapInterface()->getMusic()->getPositionMS();
        } else {
            was_playing = false;
            prev_position_ms = 0;
        }
    };
    // the actual reset will be sandwiched between these during restart
    static auto outputChangedAfterCallback = []() -> void {
        // part 2 of callback
        if(osu && osu->getOptionsMenu() && osu->getOptionsMenu()->outputDeviceLabel && osu->getSkin()) {
            osu->getOptionsMenu()->outputDeviceLabel->setText(soundEngine->getOutputDeviceName());
            osu->getSkin()->reloadSounds();
            osu->getOptionsMenu()->onOutputDeviceResetUpdate();

            // start playing music again after audio device changed
            if(osu->getMapInterface() && osu->getMapInterface()->getMusic()) {
                if(osu->isInPlayMode()) {
                    osu->getMapInterface()->unloadMusic();
                    osu->getMapInterface()->loadMusic();
                    osu->getMapInterface()->getMusic()->setLoop(false);
                    osu->getMapInterface()->getMusic()->setPositionMS(prev_position_ms);
                } else {
                    osu->getMapInterface()->unloadMusic();
                    osu->getMapInterface()->selectBeatmap();
                    osu->getMapInterface()->getMusic()->setPositionMS(prev_position_ms);
                }
            }

            if(was_playing) {
                osu->music_unpause_scheduled = true;
            }
            osu->getOptionsMenu()->scheduleLayoutUpdate();
        }
    };
    soundEngine->setDeviceChangeBeforeCallback(outputChangedBeforeCallback);
    soundEngine->setDeviceChangeAfterCallback(outputChangedAfterCallback);

    // this sets convar callbacks for things that require a soundengine reinit, do it
    // only after init so config files don't restart it over and over again
    soundEngine->allowInternalCallbacks();
}
