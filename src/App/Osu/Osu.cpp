// Copyright (c) 2015, PG, All rights reserved.
#include "Osu.h"

#include <algorithm>
#include <utility>

#include "AnimationHandler.h"
#include "AvatarManager.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoApi.h"
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
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Parsing.h"
#include "PauseMenu.h"
#include "PeppyImporter.h"
#include "Profiler.h"
#include "PromptScreen.h"
#include "RankingScreen.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "RoomScreen.h"
#include "SString.h"
#include "Shader.h"
#include "Skin.h"
#include "SongBrowser/LeaderboardPPCalcThread.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/MapCalcThread.h"
#include "SongBrowser/ScoreConverterThread.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "Timing.h"
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

void Osu::globalOnSetValueProtectedCallback() {
    if(likely(this->map_iface)) {
        this->map_iface->is_submittable = false;
    }
}

Osu *osu = nullptr;
Osu::Osu() {
    osu = this;
    srand(crypto::rng::get_rand<u32>());

    // prevents score submission when/if a protected convar is changed during gameplay
    // will be removed in destructor
    ConVar::setOnSetValueProtectedCallback(SA::MakeDelegate<&Osu::globalOnSetValueProtectedCallback>(this));

    if(Env::cfg(BUILD::DEBUG)) {
        BanchoState::neosu_version = fmt::format("dev-{}", cv::build_timestamp.getVal<u64>());
    } else if(cv::is_bleedingedge.getBool()) {  // FIXME: isn't this always false here...?
        BanchoState::neosu_version = fmt::format("bleedingedge-{}", cv::build_timestamp.getVal<u64>());
    } else {
        BanchoState::neosu_version = fmt::format("release-{:.2f}", cv::version.getFloat());
    }

    BanchoState::user_agent = "Mozilla/5.0 (compatible; neosu/";
    BanchoState::user_agent.append(BanchoState::neosu_version);
    BanchoState::user_agent.append("; " OS_NAME "; +https://" NEOSU_DOMAIN "/)");

    // create directories we will assume already exist later on
    Environment::createDirectory(NEOSU_AVATARS_PATH);
    Environment::createDirectory(NEOSU_CFG_PATH);
    Environment::createDirectory(NEOSU_MAPS_PATH);
    Environment::createDirectory(NEOSU_REPLAYS_PATH);
    Environment::createDirectory(NEOSU_SCREENSHOTS_PATH);
    Environment::createDirectory(NEOSU_SKINS_PATH);

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
    cv::prefer_cjk.setCallback(SA::MakeDelegate<&Osu::preferCJKCallback>(this));
    this->bPreferCJK = cv::prefer_cjk.getBool();

    // debug
    this->windowManager = std::make_unique<CWindowManager>();

    // renderer
    this->internalRect = engine->getScreenRect();

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
    db = std::make_unique<Database>();  // global database instance
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
    if(!Environment::fileExists(MCENGINE_CFG_PATH "/osu.cfg")) {
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
    // load skin
    if(!this->skin)  // sanity (the skin may already be loaded by Console::execConfigFile() above, should be impossible?)
        this->onSkinChange(cv::skin.getString());

    // Convar callbacks that should be set after loading the config
    cv::mod_mafham.setCallback(SA::MakeDelegate<&Osu::rebuildRenderTargets>(this));
    cv::mod_fposu.setCallback(SA::MakeDelegate<&Osu::rebuildRenderTargets>(this));
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

    // These mods conflict with each other, prevent them from being enabled at the same time
    // TODO: allow fullalternate, needs extra logic to detect whether player is using 2K/3K/4K
    cv::mod_fullalternate.setCallback([] {
        if(!cv::mod_fullalternate.getBool()) return;
        cv::mod_no_keylock.setValue(false);
        cv::mod_singletap.setValue(false);
        osu->modSelector->updateExperimentalButtons();
    });
    cv::mod_singletap.setCallback([] {
        if(!cv::mod_singletap.getBool()) return;
        cv::mod_fullalternate.setValue(false);
        cv::mod_no_keylock.setValue(false);
        osu->modSelector->updateExperimentalButtons();
    });
    cv::mod_no_keylock.setCallback([] {
        if(!cv::mod_no_keylock.getBool()) return;
        cv::mod_fullalternate.setValue(false);
        cv::mod_singletap.setValue(false);
        osu->modSelector->updateExperimentalButtons();
    });

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
        resourceManager->loadFont("forkawesome-webfont.ttf", "FONT_OSU_ICONS", Icons::icons, 26, true, newDPI);
    this->fonts.push_back(defaultFont);
    this->fonts.push_back(this->titleFont);
    this->fonts.push_back(this->subTitleFont);
    this->fonts.push_back(this->songBrowserFont);
    this->fonts.push_back(this->songBrowserFontBold);
    this->fonts.push_back(this->fontIcons);

    float averageIconHeight = 0.0f;
    for(char16_t icon : Icons::icons) {
        UString iconString;
        iconString.insert(0, icon);
        const float height = this->fontIcons->getStringHeight(iconString);
        if(height > averageIconHeight) averageIconHeight = height;
    }
    this->fontIcons->setHeight(averageIconHeight);

    if(defaultFont->getDPI() != newDPI) {
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }

    // load subsystems, add them to the screens array
    this->userButton = std::make_unique<UserCard>(BanchoState::get_uid());

    this->songBrowser = std::make_unique<SongBrowser>();
    this->volumeOverlay = std::make_unique<VolumeOverlay>();
    this->tooltipOverlay = std::make_unique<TooltipOverlay>();
    this->optionsMenu = std::make_unique<OptionsMenu>();
    this->mainMenu = std::make_unique<MainMenu>();  // has to be after options menu
    this->backgroundImageHandler = std::make_unique<BGImageHandler>();
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

    // don't auto update if this env var is set to anything other than 0 or empty (if it is set)
    if constexpr(!Env::cfg(BUILD::DEBUG)) {  // don't auto-update debug builds
        const std::string extUpdater = Environment::getEnvVariable("NEOSU_EXTERNAL_UPDATE_PROVIDER");
        if(cv::auto_update.getBool() && (extUpdater.empty() || strtol(extUpdater.c_str(), nullptr, 10) == 0)) {
            bool force_update = cv::bleedingedge.getBool() != cv::is_bleedingedge.getBool();
            this->updateHandler->checkForUpdates(force_update);
        }
    }

    // now handle commandline arguments after we have loaded everything
    bool reloading_db = env->getEnvInterop().handle_cmdline_args();
    if(!reloading_db && cv::load_db_immediately.getBool()) {
        // Start loading db early
        this->songBrowser->refreshBeatmaps();
    }

    // Not the type of shader you want players to tweak or delete, so loading from string
    if(env->usingDX11()) {
#ifdef MCENGINE_FEATURE_DIRECTX11
        this->actual_flashlight_shader = resourceManager->createShader(
            std::string(reinterpret_cast<const char *>(DX11_actual_flashlight_vsh), DX11_actual_flashlight_vsh_size()),
            std::string(reinterpret_cast<const char *>(DX11_actual_flashlight_fsh), DX11_actual_flashlight_fsh_size()),
            "actual_flashlight");
        this->flashlight_shader = resourceManager->createShader(
            std::string(reinterpret_cast<const char *>(DX11_flashlight_vsh), DX11_flashlight_vsh_size()),
            std::string(reinterpret_cast<const char *>(DX11_flashlight_fsh), DX11_flashlight_fsh_size()), "flashlight");
#endif
    } else {
#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)
        this->actual_flashlight_shader = resourceManager->createShader(
            std::string(reinterpret_cast<const char *>(GL_actual_flashlight_vsh), GL_actual_flashlight_vsh_size()),
            std::string(reinterpret_cast<const char *>(GL_actual_flashlight_fsh), GL_actual_flashlight_fsh_size()),
            "actual_flashlight");
        this->flashlight_shader = resourceManager->createShader(
            std::string(reinterpret_cast<const char *>(GL_flashlight_vsh), GL_flashlight_vsh_size()),
            std::string(reinterpret_cast<const char *>(GL_flashlight_fsh), GL_flashlight_fsh_size()), "flashlight");
#endif
    }

    env->setCursorVisible(!this->internalRect.contains(mouse->getPos()));
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
    db.reset();  // shutdown db

    // remove the static callback
    ConVar::setOnSetValueProtectedCallback({});
}

void Osu::draw() {
    if(!this->skin.get() || this->flashlight_shader == nullptr)  // sanity check
    {
        g->setColor(0xff000000);
        g->fillRect(0, 0, this->getVirtScreenWidth(), this->getVirtScreenHeight());
        if(this->mainMenu && this->backgroundImageHandler && this->map_iface->getBeatmap()) {
            // try at least drawing background image during early loading
            this->mainMenu->draw();
        }
        return;
    }

    // if we are not using the native window resolution, draw into the buffer
    const bool isBufferedDraw = (g->getResolution() != this->getVirtScreenSize());
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

                if(env->usingDX11()) {  // don't flip Y position for DX11
                    this->flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x, flashlightPos.y);

                    g->forceUpdateTransform();
                    Matrix4 mvp = g->getMVP();
                    this->flashlight_shader->setUniformMatrix4fv("mvp", mvp);
                } else {
                    this->flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x,
                                                          this->getVirtScreenSize().y - flashlightPos.y);
                }

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

                if(env->usingDX11()) {  // don't flip Y position for DX11
                    this->actual_flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x, flashlightPos.y);

                    g->forceUpdateTransform();
                    Matrix4 mvp = g->getMVP();
                    this->actual_flashlight_shader->setUniformMatrix4fv("mvp", mvp);
                } else {
                    this->actual_flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x,
                                                                 this->getVirtScreenSize().y - flashlightPos.y);
                }

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

        if(!isFPoSu) this->hud->drawFps();

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
            this->hud->drawFps();
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

        this->hud->drawFps();

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

        vec2 offset = vec2(g->getResolution().x / 2 - this->internalRect.getWidth() / 2,
                           g->getResolution().y / 2 - this->internalRect.getHeight() / 2);
        g->setBlending(false);
        if(cv::letterboxing.getBool()) {
            this->backBuffer->draw(offset.x * (1.0f + cv::letterboxing_offset_x.getFloat()),
                                   offset.y * (1.0f + cv::letterboxing_offset_y.getFloat()),
                                   this->internalRect.getWidth(), this->internalRect.getHeight());
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
        this->map_iface->update();

        // NOTE: force keep loaded background images while playing
        this->backgroundImageHandler->scheduleFreezeCache();

        // skip button clicking
        bool can_skip = this->map_iface->isInSkippableSection() && !this->bClickedSkipButton;
        can_skip &= !this->map_iface->isPaused() && !this->volumeOverlay->isBusy();
        if(can_skip) {
            const bool isAnyOsuKeyDown =
                (this->bKeyboardKey1Down || this->bKeyboardKey2Down || this->bMouseKey1Down || this->bMouseKey2Down);
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
            const bool isLoading = this->map_iface->isLoading();

            if(this->map_iface->isInSkippableSection() && !this->map_iface->isPaused() && !isLoading) {
                bool can_skip_intro = (cv::skip_intro_enabled.getBool() && this->map_iface->iCurrentHitObjectIndex < 1);
                bool can_skip_break =
                    (cv::skip_breaks_enabled.getBool() && this->map_iface->iCurrentHitObjectIndex > 0);
                if(BanchoState::is_playing_a_multi_map()) {
                    can_skip_intro = BanchoState::room.all_players_skipped;
                    can_skip_break = false;
                }

                if(can_skip_intro || can_skip_break) {
                    this->map_iface->skipEmptySection();
                }
            }

            if(!isLoading) this->bSkipScheduled = false;
        }

        // Reset m_bClickedSkipButton on mouse up
        // We only use m_bClickedSkipButton to prevent seeking when clicking the skip button
        if(this->bClickedSkipButton && !this->map_iface->isInSkippableSection()) {
            if(!mouse->isLeftDown()) {
                this->bClickedSkipButton = false;
            }
        }

        // scrubbing/seeking
        this->bSeeking = (this->bSeekKey || this->map_iface->is_watching);
        this->bSeeking &= !this->volumeOverlay->isBusy();
        this->bSeeking &= !BanchoState::is_playing_a_multi_map() && !this->bClickedSkipButton;
        this->bSeeking &= !BanchoState::spectating;
        if(this->bSeeking) {
            f32 mousePosX = std::round(mouse->getPos().x);
            f32 percent = std::clamp<f32>(mousePosX / (f32)this->internalRect.getWidth(), 0.0f, 1.0f);
            f32 seek_to_ms = percent * (this->map_iface->getStartTimePlayable() + this->map_iface->getLengthPlayable());

            if(mouse->isLeftDown()) {
                if(mousePosX != this->fPrevSeekMousePosX || !cv::scrubbing_smooth.getBool()) {
                    this->fPrevSeekMousePosX = mousePosX;

                    // special case: allow cancelling the failing animation here
                    if(this->map_iface->hasFailed()) this->map_iface->cancelFailing();

                    // when seeking during gameplay, add nofail for convenience
                    if(!this->map_iface->is_watching && !cv::mod_nofail.getBool()) {
                        cv::mod_nofail.setValue(true);
                    }

                    this->map_iface->seekMS(seek_to_ms);
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
                this->map_iface->restart(true);
                this->map_iface->update();
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

    if(key == cv::OPEN_SKIN_SELECT_MENU.getVal<KEYCODE>()) {
        this->optionsMenu->onSkinSelect();
        key.consume();
        return;
    }

    // disable mouse buttons hotkey
    if(key == cv::DISABLE_MOUSE_BUTTONS.getVal<KEYCODE>()) {
        if(cv::disable_mousebuttons.getBool()) {
            cv::disable_mousebuttons.setValue(0.0f);
            this->notificationOverlay->addNotification("Mouse buttons are enabled.");
        } else {
            cv::disable_mousebuttons.setValue(1.0f);
            this->notificationOverlay->addNotification("Mouse buttons are disabled.");
        }
    }

    if(key == cv::TOGGLE_MAP_BACKGROUND.getVal<KEYCODE>()) {
        auto diff = this->map_iface->getBeatmap();
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
    if(key == cv::TOGGLE_CHAT.getVal<KEYCODE>()) {
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
    if(key == cv::TOGGLE_EXTENDED_CHAT.getVal<KEYCODE>()) {
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
    if(key == cv::SAVE_SCREENSHOT.getVal<KEYCODE>()) this->saveScreenshot();

    // boss key (minimize + mute)
    if(key == cv::BOSS_KEY.getVal<KEYCODE>()) {
        env->minimize();
        this->bWasBossKeyPaused = this->map_iface->isPreviewMusicPlaying();
        this->map_iface->pausePreviewMusic(false);
    }

    // local hotkeys (and gameplay keys)

    // while playing (and not in options)
    if(this->isInPlayMode() && !this->optionsMenu->isVisible() && !this->chat->isVisible()) {
        // instant replay
        if((this->map_iface->isPaused() || this->map_iface->hasFailed())) {
            if(!key.isConsumed() && key == cv::INSTANT_REPLAY.getVal<KEYCODE>()) {
                if(!this->map_iface->is_watching && !BanchoState::spectating) {
                    FinishedScore score;
                    score.replay = this->map_iface->live_replay;
                    score.beatmap_hash = this->map_iface->getBeatmap()->getMD5();
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
            if(key == cv::LEFT_CLICK.getVal<KEYCODE>()) {
                if(!this->bKeyboardKey1Down) {
                    this->bKeyboardKey1Down = true;
                    this->onKey1Change(true, false);
                }
                if(!this->map_iface->hasFailed()) key.consume();
            }

            // 'M1'
            if(key == cv::LEFT_CLICK_2.getVal<KEYCODE>()) {
                if(!this->bMouseKey1Down) {
                    this->bMouseKey1Down = true;
                    this->onKey1Change(true, true);
                }
                if(!this->map_iface->hasFailed()) key.consume();
            }

            // K2
            if(key == cv::RIGHT_CLICK.getVal<KEYCODE>()) {
                if(!this->bKeyboardKey2Down) {
                    this->bKeyboardKey2Down = true;
                    this->onKey2Change(true, false);
                }
                if(!this->map_iface->hasFailed()) key.consume();
            }

            // 'M2'
            if(key == cv::RIGHT_CLICK_2.getVal<KEYCODE>()) {
                if(!this->bMouseKey2Down) {
                    this->bMouseKey2Down = true;
                    this->onKey2Change(true, true);
                }
                if(!this->map_iface->hasFailed()) key.consume();
            }

            // Smoke
            if(key == cv::SMOKE.getVal<KEYCODE>()) {
                this->map_iface->current_keys |= LegacyReplay::Smoke;
                key.consume();
            }

            // handle skipping
            if(key == KEY_ENTER || key == KEY_NUMPAD_ENTER || key == cv::SKIP_CUTSCENE.getVal<KEYCODE>())
                this->bSkipScheduled = true;

            // toggle ui
            if(!key.isConsumed() && key == cv::TOGGLE_SCOREBOARD.getVal<KEYCODE>() && !this->bScoreboardToggleCheck) {
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
            if(!key.isConsumed() && (key == KEY_F1 || key == cv::TOGGLE_MODSELECT.getVal<KEYCODE>()) &&
               ((KEY_F1 != cv::LEFT_CLICK.getVal<KEYCODE>() && KEY_F1 != cv::LEFT_CLICK_2.getVal<KEYCODE>()) ||
                (!this->bKeyboardKey1Down && !this->bMouseKey1Down)) &&
               ((KEY_F1 != cv::RIGHT_CLICK.getVal<KEYCODE>() && KEY_F1 != cv::RIGHT_CLICK_2.getVal<KEYCODE>()) ||
                (!this->bKeyboardKey2Down && !this->bMouseKey2Down)) &&
               !this->bF1 && !this->map_iface->hasFailed() &&
               !BanchoState::is_playing_a_multi_map())  // only if not failed though
            {
                this->bF1 = true;
                this->toggleModSelection(true);
            }

            // quick save/load
            if(!BanchoState::is_playing_a_multi_map()) {
                if(key == cv::QUICK_SAVE.getVal<KEYCODE>()) this->fQuickSaveTime = this->map_iface->getTime();

                if(key == cv::QUICK_LOAD.getVal<KEYCODE>()) {
                    // special case: allow cancelling the failing animation here
                    if(this->map_iface->hasFailed()) this->map_iface->cancelFailing();

                    this->map_iface->seekMS(this->fQuickSaveTime);
                }
            }

            // quick seek
            if(!BanchoState::is_playing_a_multi_map()) {
                const bool backward = (key == cv::SEEK_TIME_BACKWARD.getVal<KEYCODE>());
                const bool forward = (key == cv::SEEK_TIME_FORWARD.getVal<KEYCODE>());
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
        if(((key == cv::QUICK_RETRY.getVal<KEYCODE>() ||
             (keyboard->isControlDown() && !keyboard->isAltDown() && key == KEY_R)) &&
            !this->bQuickRetryDown)) {
            this->bQuickRetryDown = true;
            this->fQuickRetryTime = engine->getTime() + cv::quick_retry_delay.getFloat();
        }

        // handle seeking
        if(key == cv::SEEK_TIME.getVal<KEYCODE>()) this->bSeekKey = true;

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
            bool pressed_pause = (key == cv::GAME_PAUSE.getVal<KEYCODE>()) || (key == KEY_ESCAPE);
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
            if(key == cv::INCREASE_LOCAL_OFFSET.getVal<KEYCODE>()) {
                i32 offsetAdd = keyboard->isAltDown() ? 1 : 5;
                this->map_iface->getBeatmap()->setLocalOffset(this->map_iface->getBeatmap()->getLocalOffset() +
                                                              offsetAdd);
                this->notificationOverlay->addNotification(
                    fmt::format("Local beatmap offset set to {} ms", this->map_iface->getBeatmap()->getLocalOffset()));
            }
            if(key == cv::DECREASE_LOCAL_OFFSET.getVal<KEYCODE>()) {
                i32 offsetAdd = -(keyboard->isAltDown() ? 1 : 5);
                this->map_iface->getBeatmap()->setLocalOffset(this->map_iface->getBeatmap()->getLocalOffset() +
                                                              offsetAdd);
                this->notificationOverlay->addNotification(
                    fmt::format("Local beatmap offset set to {} ms", this->map_iface->getBeatmap()->getLocalOffset()));
            }
        }
    }
}

void Osu::onKeyUp(KeyboardEvent &key) {
    // clicks
    {
        // K1
        if(key == cv::LEFT_CLICK.getVal<KEYCODE>()) {
            if(this->bKeyboardKey1Down) {
                this->bKeyboardKey1Down = false;
                if(this->isInPlayMode()) this->onKey1Change(false, false);
            }
        }

        // 'M1'
        if(key == cv::LEFT_CLICK_2.getVal<KEYCODE>()) {
            if(this->bMouseKey1Down) {
                this->bMouseKey1Down = false;
                if(this->isInPlayMode()) this->onKey1Change(false, true);
            }
        }

        // K2
        if(key == cv::RIGHT_CLICK.getVal<KEYCODE>()) {
            if(this->bKeyboardKey2Down) {
                this->bKeyboardKey2Down = false;
                if(this->isInPlayMode()) this->onKey2Change(false, false);
            }
        }

        // 'M2'
        if(key == cv::RIGHT_CLICK_2.getVal<KEYCODE>()) {
            if(this->bMouseKey2Down) {
                this->bMouseKey2Down = false;
                if(this->isInPlayMode()) this->onKey2Change(false, true);
            }
        }

        // Smoke
        if(this->map_iface && (key == cv::SMOKE.getVal<KEYCODE>())) {
            this->map_iface->current_keys &= ~LegacyReplay::Smoke;
            key.consume();
        }
    }

    // forward to all subsystems, if not consumed
    this->forEachScreenWhile<&OsuScreen::onKeyUp>([&key]() -> bool { return !key.isConsumed(); }, key);

    // misc hotkeys release
    // XXX: handle keypresses in the engine, instead of doing this hacky mess
    if(key == KEY_F1 || key == cv::TOGGLE_MODSELECT.getVal<KEYCODE>()) this->bF1 = false;
    if(key == cv::GAME_PAUSE.getVal<KEYCODE>() || key == KEY_ESCAPE) this->bEscape = false;
    if(key == KEY_LSHIFT || key == KEY_RSHIFT) this->bUIToggleCheck = false;
    if(key == cv::TOGGLE_SCOREBOARD.getVal<KEYCODE>()) {
        this->bScoreboardToggleCheck = false;
        this->bUIToggleCheck = false;
    }
    if(key == cv::QUICK_RETRY.getVal<KEYCODE>() || key == KEY_R) this->bQuickRetryDown = false;
    if(key == cv::SEEK_TIME.getVal<KEYCODE>()) this->bSeekKey = false;

    // handle fposu key handling
    this->fposu->onKeyUp(key);
}

void Osu::stealFocus() { this->forEachScreen<&OsuScreen::stealFocus>(); }

void Osu::onChar(KeyboardEvent &e) {
    this->forEachScreenWhile<&OsuScreen::onChar>([&e]() -> bool { return !e.isConsumed(); }, e);
}

void Osu::onButtonChange(ButtonIndex button, bool down) {
    if(cv::disable_mousebuttons.getBool()) return;

    using enum ButtonIndex;
    switch(button) {
        case BUTTON_LEFT: {
            if(down != this->bMouseKey1Down) {
                this->bMouseKey1Down = down;
                this->onKey1Change(down, true);
            }
            break;
        }
        case BUTTON_RIGHT: {
            if(down != this->bMouseKey2Down) {
                this->bMouseKey2Down = down;
                this->onKey2Change(down, true);
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

    const bool nextVisible = !this->songBrowser->isVisible();

    // disable mainmenu visibility BEFORE songbrowser and potentially loading beatmaps
    // otherwise during the next update/draw tick we might try to draw images in MainMenu::draw() from stale/deleted beatmaps
    // since clearPreloadedMaps only runs after it's finished (1 frame later)
    // TODO: don't store potentially rugpull-able pointers to beatmaps in MainMenu
    if(!BanchoState::is_in_a_multi_room()) {
        this->mainMenu->setVisible(!nextVisible);
    }

    this->songBrowser->setVisible(nextVisible);

    // try refreshing if we have no beatmaps and are not already refreshing
    if(nextVisible && this->songBrowser->beatmapsets.size() == 0 && !this->songBrowser->bBeatmapRefreshScheduled) {
        this->songBrowser->refreshBeatmaps();
    }

    if(BanchoState::is_in_a_multi_room()) {
        // We didn't select a map; revert to previously selected one
        auto map = this->songBrowser->lastSelectedBeatmap;
        if(map != nullptr) {
            BanchoState::room.map_name = UString::format("%s - %s [%s]", map->getArtist().c_str(),
                                                         map->getTitle().c_str(), map->getDifficultyName().c_str());
            BanchoState::room.map_md5 = map->getMD5();
            BanchoState::room.map_id = map->getID();

            Packet packet;
            packet.id = MATCH_CHANGE_SETTINGS;
            BanchoState::room.pack(packet);
            BANCHO::Net::send_packet(packet);

            this->room->on_map_change();
        }
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

    if(!env->directoryExists(NEOSU_SCREENSHOTS_PATH) && !env->createDirectory(NEOSU_SCREENSHOTS_PATH)) {
        this->notificationOverlay->addNotification("Error: Couldn't create screenshots folder.", 0xffff0000, false,
                                                   3.0f);
        return;
    }

    while(env->fileExists(fmt::format(NEOSU_SCREENSHOTS_PATH "/screenshot{}.png", screenshotNumber)))
        screenshotNumber++;

    const auto screenshotFilename = fmt::format(NEOSU_SCREENSHOTS_PATH "/screenshot{}.png", screenshotNumber);

    constexpr u8 screenshotChannels{3};
    std::vector<u8> pixels = g->getScreenshot(screenshotChannels > 3 /* withAlpha = false */);

    if(pixels.empty()) {
        static uint8_t once = 0;
        if(!once++)
            this->notificationOverlay->addNotification("Error: Couldn't grab a screenshot :(", 0xffff0000, false, 3.0f);
        debugLog("failed to get pixel data for screenshot");
        return;
    }

    const f32 outerWidth = g->getResolution().x;
    const f32 outerHeight = g->getResolution().y;
    const f32 innerWidth = this->internalRect.getWidth();
    const f32 innerHeight = this->internalRect.getHeight();

    soundEngine->play(this->skin->getShutter());
    this->notificationOverlay->addToast(UString::format("Saved screenshot to %s", screenshotFilename.c_str()),
                                        CHAT_TOAST, [screenshotFilename] { env->openFileBrowser(screenshotFilename); });

    // don't need cropping
    if(!cv::crop_screenshots.getBool() || (g->getResolution() == this->getVirtScreenSize())) {
        Image::saveToImage(pixels.data(), static_cast<i32>(outerWidth), static_cast<i32>(outerHeight),
                           screenshotChannels, screenshotFilename);
        return;
    }

    // need cropping
    f32 offsetXpct = 0, offsetYpct = 0;
    if((g->getResolution() != this->getVirtScreenSize()) && cv::letterboxing.getBool()) {
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

void Osu::onPlayEnd(const FinishedScore &score, bool quit, bool /*aborted*/) {
    cv::snd_change_check_interval.setValue(cv::snd_change_check_interval.getDefaultFloat());

    if(!quit) {
        if(!cv::mod_endless.getBool()) {
            // NOTE: the order of these two calls matters
            this->rankingScreen->setScore(score);
            this->rankingScreen->setBeatmapInfo(this->map_iface->getBeatmap());

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
        if(this->internalRect.getWidth() < this->vInternalResolution2.x)
            this->internalRect.setWidth(this->vInternalResolution2.x);
        if(this->internalRect.getHeight() < this->vInternalResolution2.y)
            this->internalRect.setHeight(this->vInternalResolution2.y);

        // clamp downwards to engine resolution
        if(newResolution.x < this->internalRect.getWidth()) this->internalRect.setWidth(newResolution.x);
        if(newResolution.y < this->internalRect.getHeight()) this->internalRect.setHeight(newResolution.y);
    } else {
        this->internalRect = {vec2{}, newResolution};
    }

    // update dpi specific engine globals
    cv::ui_scrollview_scrollbarwidth.setValue(15.0f * Osu::getUIScale());  // not happy with this as a convar

    // interfaces
    this->forEachScreen<&OsuScreen::onResolutionChange>(this->getVirtScreenSize());

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
    debugLog("{}x{}", this->internalRect.getWidth(), this->internalRect.getHeight());

    this->backBuffer->rebuild(0, 0, this->internalRect.getWidth(), this->internalRect.getHeight());

    if(cv::mod_fposu.getBool())
        this->playfieldBuffer->rebuild(0, 0, this->internalRect.getWidth(), this->internalRect.getHeight());
    else
        this->playfieldBuffer->rebuild(0, 0, 64, 64);

    this->sliderFrameBuffer->rebuild(0, 0, this->internalRect.getWidth(), this->internalRect.getHeight(),
                                     Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);

    this->AAFrameBuffer->rebuild(0, 0, this->internalRect.getWidth(), this->internalRect.getHeight());

    if(cv::mod_mafham.getBool()) {
        this->frameBuffer->rebuild(0, 0, this->internalRect.getWidth(), this->internalRect.getHeight());
        this->frameBuffer2->rebuild(0, 0, this->internalRect.getWidth(), this->internalRect.getHeight());
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
    if((g->getResolution() != this->getVirtScreenSize()) && cv::letterboxing.getBool()) {
        offset = -vec2((engine->getScreenWidth() / 2.f - this->internalRect.getWidth() / 2.f) *
                           (1.0f + cv::letterboxing_offset_x.getFloat()),
                       (engine->getScreenHeight() / 2.f - this->internalRect.getHeight() / 2.f) *
                           (1.0f + cv::letterboxing_offset_y.getFloat()));

        scale = vec2(this->internalRect.getWidth() / engine->getScreenWidth(),
                     this->internalRect.getHeight() / engine->getScreenHeight());
    }

    mouse->setOffset(offset);
    mouse->setScale(scale);
}

void Osu::updateWindowsKeyDisable() {
    const bool isPlayerPlaying = engine->hasFocus() && this->isInPlayMode() &&
                                 (!(this->map_iface->isPaused() || this->map_iface->isContinueScheduled()) ||
                                  this->map_iface->isRestartScheduled()) &&
                                 !cv::mod_autoplay.getBool();
    bool grab = false;
    if(cv::win_disable_windows_key_while_playing.getBool()) {
        grab = isPlayerPlaying;
    }
    logIfCV(debug_osu, "{} keyboard, {} to text input", grab ? "grabbed" : "ungrabbed",
            isPlayerPlaying ? "not listening" : "listening");

    env->grabKeyboard(grab);

    // this is kind of a weird place to put this, but we don't care about text input when in gameplay
    // on some platforms, text input being enabled might result in an on-screen keyboard showing up
    // TODO: check if this breaks chat while playing
    env->listenToTextInput(!isPlayerPlaying);
}

void Osu::fireResolutionChanged() { this->onResolutionChanged(this->getVirtScreenSize()); }

void Osu::onWindowedResolutionChanged(std::string_view args) {
    if(env->isFullscreen()) return;

    auto parsed = Parsing::parse_resolution(args);
    if(!parsed.has_value()) {
        debugLog("Error: Invalid arguments {} for command 'osu_resolution'! (Usage: e.g. \"osu_resolution 1280x720\")",
                 args);
        return;
    }

    i32 width{parsed->x}, height{parsed->y};
    debugLog("{}x{}", width, height);

    env->setWindowSize(width, height);
    env->center();
}

void Osu::onInternalResolutionChanged(std::string_view args) {
    if(!env->isFullscreen()) return;

    auto parsed = Parsing::parse_resolution(args);
    if(!parsed.has_value()) {
        debugLog("Error: Invalid arguments {} for command 'osu_resolution'! (Usage: e.g. \"osu_resolution 1280x720\")",
                 args);
        return;
    }
    debugLog("{}x{}", parsed->x, parsed->y);

    const float prevUIScale = getUIScale();
    vec2 newInternalResolution = parsed.value();

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
    this->internalRect = {vec2{}, newInternalResolution};
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
    this->onSkinChange(cv::skin.getString());
}

void Osu::onSkinChange(std::string_view newSkinName) {
    if(this->skin) {
        if(this->bSkinLoadScheduled || this->skinScheduledToLoad != nullptr) return;
        if(newSkinName.length() < 1) return;
    }

    if(newSkinName == "default") {
        this->skinScheduledToLoad = new Skin(newSkinName, MCENGINE_IMAGES_PATH "/default/", true);
        if(!this->skin) this->skin.reset(this->skinScheduledToLoad);
        this->bSkinLoadScheduled = true;
        return;
    }

    std::string neosuSkinFolder = fmt::format(NEOSU_SKINS_PATH "/{}/", newSkinName);
    if(env->directoryExists(neosuSkinFolder)) {
        this->skinScheduledToLoad = new Skin(newSkinName, neosuSkinFolder, false);
    } else {
        std::string ppySkinFolder{cv::osu_folder.getString()};
        ppySkinFolder.append("/");
        ppySkinFolder.append(cv::osu_folder_sub_skins.getString());
        ppySkinFolder.append(newSkinName);
        ppySkinFolder.append("/");
        std::string sf = ppySkinFolder;
        this->skinScheduledToLoad = new Skin(newSkinName, sf, false);
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

void Osu::onSpeedChange(float speed) {
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

void Osu::onUIScaleChange(float oldValue, float newValue) {
    if(oldValue != newValue) {
        // delay
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }
}

void Osu::onUIScaleToDPIChange(float oldValue, float newValue) {
    if((oldValue > 0) != (newValue > 0)) {
        // delay
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }
}

void Osu::onLetterboxingChange(float oldValue, float newValue) {
    if((oldValue > 0) != (newValue > 0)) this->bFireResolutionChangedScheduled = true;  // delay
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
        const bool internal_contains_mouse = this->internalRect.contains(mouse->getPos());
        if(internal_contains_mouse) {
            desired_vis = false;
        } else {
            desired_vis = true;
        }
    }

    // only change if it's different from the current mouse state
    if(desired_vis != currently_visible) {
        logIfCV(debug_mouse, "current: {} desired: {}", currently_visible, desired_vis);
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
        if((g->getResolution() != this->getVirtScreenSize()) && cv::letterboxing.getBool()) {
            clip = McRect{-mouse->getOffset(), this->getVirtScreenSize()};
        } else {
            clip = McRect{vec2{}, engine->getScreenSize()};
        }
    }

    logIfCV(debug_mouse, "confined: {}, cliprect: {}", confine_cursor, clip);

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
    const bool canPress = cv::mod_no_keylock.getBool() || (this->bKeyboardKey1Down != this->bMouseKey1Down);

    // NOTE: allow keyup even while beatmap is paused, to correctly not-continue immediately due to pressed keys
    if(this->isInPlayMode()) {
        if(pressed && canPress && !this->map_iface->isPaused())
            this->map_iface->keyPressed1(isMouse);
        else if(!pressed)
            this->map_iface->keyReleased1(isMouse);
    }

    // cursor anim + ripples
    // TODO: wtf is this correct???
    const bool doAnimate = !this->isInPlayMode() || this->map_iface->isPaused();
    if(doAnimate) {
        if(pressed && canPress) {
            this->hud->animateCursorExpand();
            this->hud->addCursorRipple(mouse->getPos());
        } else if(!this->bKeyboardKey1Down && !this->bMouseKey1Down && !this->bKeyboardKey2Down &&
                  !this->bMouseKey2Down)
            this->hud->animateCursorShrink();
    }
}

void Osu::onKey2Change(bool pressed, bool isMouse) {
    const bool canPress = cv::mod_no_keylock.getBool() || (this->bKeyboardKey2Down != this->bMouseKey2Down);

    // NOTE: allow keyup even while beatmap is paused, to correctly not-continue immediately due to pressed keys
    if(this->isInPlayMode()) {
        if(pressed && canPress && !this->map_iface->isPaused())
            this->map_iface->keyPressed2(isMouse);
        else if(!pressed)
            this->map_iface->keyReleased2(isMouse);
    }

    // cursor anim + ripples
    // TODO: wtf is this correct???
    const bool doAnimate = !this->isInPlayMode() || this->map_iface->isPaused();
    if(doAnimate) {
        if(pressed && canPress) {
            this->hud->animateCursorExpand();
            this->hud->addCursorRipple(mouse->getPos());
        } else if(!this->bKeyboardKey2Down && !this->bMouseKey2Down && !this->bKeyboardKey1Down &&
                  !this->bMouseKey1Down)
            this->hud->animateCursorShrink();
    }
}

void Osu::onLetterboxingOffsetChange() {
    this->updateMouseSettings();
    this->updateConfineCursor();
}

void Osu::onUserCardChange(std::string_view new_username) {
    // NOTE: force update options textbox to avoid shutdown inconsistency
    this->getOptionsMenu()->setUsername(UString{new_username.data(), static_cast<int>(new_username.length())});
    this->userButton->setID(BanchoState::get_uid());
}

float Osu::getImageScaleToFitResolution(vec2 size, vec2 resolution) {
    if(resolution.x / size.x > resolution.y / size.y) {
        return resolution.y / size.y;
    } else {
        return resolution.x / size.x;
    }
}

float Osu::getImageScaleToFitResolution(const Image *img, vec2 resolution) {
    return getImageScaleToFitResolution(vec2(img->getWidth(), img->getHeight()), resolution);
}

float Osu::getImageScaleToFillResolution(vec2 size, vec2 resolution) {
    if(resolution.x / size.x < resolution.y / size.y) {
        return resolution.y / size.y;
    } else {
        return resolution.x / size.x;
    }
}

float Osu::getImageScaleToFillResolution(const Image *img, vec2 resolution) {
    return getImageScaleToFillResolution(vec2(img->getWidth(), img->getHeight()), resolution);
}

float Osu::getImageScale(vec2 size, float osuSize) {
    auto screen = osu ? osu->getVirtScreenSize() : engine->getScreenSize();
    if(screen.x * 3 > screen.y * 4) {
        // Reduce width to fit 4:3
        screen.x = screen.y * 4 / 3;
    } else {
        // Reduce height to fit 4:3
        screen.y = screen.x * 3 / 4;
    }

    f32 x = screen.x / Osu::osuBaseResolution.x / size.x;
    f32 y = screen.y / Osu::osuBaseResolution.y / size.y;
    return osuSize * std::max(x, y);
}

float Osu::getImageScale(const Image *img, float osuSize) {
    return getImageScale(vec2(img->getWidth(), img->getHeight()), osuSize);
}

float Osu::getUIScale(float osuSize) {
    // return osuSize * Osu::getImageScaleToFitResolution(Osu::osuBaseResolution, osu->getVirtScreenSize());

    auto screen = osu ? osu->getVirtScreenSize() : engine->getScreenSize();
    if(screen.x * 3 > screen.y * 4) {
        return osuSize * screen.y / Osu::osuBaseResolution.y;
    } else {
        return osuSize * screen.x / Osu::osuBaseResolution.x;
    }
}

float Osu::getUIScale() {
    f32 scale = cv::ui_scale.getFloat();

    if(cv::ui_scale_to_dpi.getBool()) {
        f32 w = osu ? osu->getVirtScreenWidth() : engine->getScreenWidth();
        f32 h = osu ? osu->getVirtScreenHeight() : engine->getScreenHeight();
        if(w >= cv::ui_scale_to_dpi_minimum_width.getInt() && h >= cv::ui_scale_to_dpi_minimum_height.getInt()) {
            scale *= env->getDPIScale();
        }
    }

    return scale;
}

bool Osu::getModAuto() const { return cv::mod_autoplay.getBool(); }
bool Osu::getModAutopilot() const { return cv::mod_autopilot.getBool(); }
bool Osu::getModRelax() const { return cv::mod_relax.getBool(); }
bool Osu::getModSpunout() const { return cv::mod_spunout.getBool(); }
bool Osu::getModTarget() const { return cv::mod_target.getBool(); }
bool Osu::getModScorev2() const { return cv::mod_scorev2.getBool(); }
bool Osu::getModFlashlight() const { return cv::mod_flashlight.getBool(); }
bool Osu::getModNF() const { return cv::mod_nofail.getBool(); }
bool Osu::getModHD() const { return cv::mod_hidden.getBool(); }
bool Osu::getModHR() const { return cv::mod_hardrock.getBool(); }
bool Osu::getModEZ() const { return cv::mod_easy.getBool(); }
bool Osu::getModSD() const { return cv::mod_suddendeath.getBool(); }
bool Osu::getModSS() const { return cv::mod_perfect.getBool(); }
bool Osu::getModNightmare() const { return cv::mod_nightmare.getBool(); }
bool Osu::getModTD() const { return cv::mod_touchdevice.getBool() || cv::mod_touchdevice_always.getBool(); }

void Osu::setupSoloud() {
    // need to save this state somewhere to share data between callback stages
    static bool was_playing = false;
    static u32 prev_position_ms = 0;

    static auto output_changed_before_cb = []() -> void {
        Sound *map_music = nullptr;
        if(osu && osu->getMapInterface() && (map_music = osu->getMapInterface()->getMusic())) {
            was_playing = map_music->isPlaying();
            prev_position_ms = map_music->getPositionMS();
        } else {
            was_playing = false;
            prev_position_ms = 0;
        }
    };
    // the actual reset will be sandwiched between these during restart
    static auto output_changed_after_cb = []() -> void {
        // part 2 of callback
        if(osu && osu->getOptionsMenu() && osu->getOptionsMenu()->outputDeviceLabel && osu->getSkin()) {
            osu->getOptionsMenu()->outputDeviceLabel->setText(soundEngine->getOutputDeviceName());
            osu->getSkin()->reloadSounds();
            osu->getOptionsMenu()->onOutputDeviceResetUpdate();

            // start playing music again after audio device changed
            Sound *map_music = nullptr;
            const auto &map_iface = osu->getMapInterface();
            if(map_iface && (map_music = map_iface->getMusic())) {
                if(osu->isInPlayMode()) {
                    map_iface->unloadMusic();
                    map_iface->loadMusic();
                    if((map_music = map_iface->getMusic())) {  // need to get new music after loading
                        map_music->setLoop(false);
                        map_music->setPositionMS(prev_position_ms);
                    }
                } else {
                    map_iface->unloadMusic();
                    map_iface->selectBeatmap();
                    if((map_music = map_iface->getMusic())) {
                        map_music->setPositionMS(prev_position_ms);
                    }
                }
            }

            if(was_playing) {
                osu->music_unpause_scheduled = true;
            }
            osu->getOptionsMenu()->scheduleLayoutUpdate();
        }
    };
    soundEngine->setDeviceChangeBeforeCallback(output_changed_before_cb);
    soundEngine->setDeviceChangeAfterCallback(output_changed_after_cb);

    // this sets convar callbacks for things that require a soundengine reinit, do it
    // only after init so config files don't restart it over and over again
    soundEngine->allowInternalCallbacks();
}
