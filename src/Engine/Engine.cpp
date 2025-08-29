// Copyright (c) 2012, PG, All rights reserved.
#include "Engine.h"

#include <cstdio>

#include "AnimationHandler.h"
#include "CBaseUIContainer.h"
#include "ConVar.h"
#include "Console.h"
#include "ConsoleBox.h"
#include "DiscordInterface.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "NetworkHandler.h"
#include "Osu.h"
#include "Profiler.h"
#include "ResourceManager.h"
#include "SoundEngine.h"
#include "Timing.h"
#include "VisualProfiler.h"

Image *MISSING_TEXTURE = nullptr;

std::unique_ptr<Mouse> mouse = nullptr;
std::unique_ptr<Keyboard> keyboard = nullptr;
std::unique_ptr<App> app = nullptr;
std::unique_ptr<Graphics> g = nullptr;
std::unique_ptr<SoundEngine> soundEngine = nullptr;
std::unique_ptr<ResourceManager> resourceManager = nullptr;
std::unique_ptr<NetworkHandler> networkHandler = nullptr;
std::unique_ptr<AnimationHandler> animationHandler = nullptr;

Engine *engine = nullptr;

ConsoleBox *Engine::consoleBox = nullptr;

Engine::Engine() {
    engine = this;

    this->guiContainer = nullptr;
    this->visualProfiler = nullptr;

    // disable output buffering (else we get multithreading issues due to blocking)
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // print debug information
    debugLog("-= Engine Startup =-\n");
    debugLog("cmdline: {:s}\n", UString::join(env->getCommandLine()));

    // timing
    this->iFrameCount = 0;
    this->iVsyncFrameCount = 0;
    this->fVsyncFrameCounterTime = 0.0f;
    this->dFrameTime = 0.016f;

    cv::engine_throttle.setCallback(SA::MakeDelegate<&Engine::onEngineThrottleChanged>(this));

    // window
    this->bBlackout = false;
    this->bHasFocus = false;
    this->bIsMinimized = false;

    // screen
    this->bResolutionChange = false;
    this->vScreenSize = env->getWindowSize();
    this->vNewScreenSize = this->vScreenSize;
    this->screenRect = {vec2{}, this->vScreenSize};

    debugLog("Engine: ScreenSize = ({}x{})\n", (int)this->vScreenSize.x, (int)this->vScreenSize.y);

    // custom
    this->bDrawing = false;

    // initialize all engine subsystems (the order does matter!)
    debugLog("Engine: Initializing subsystems ...\n");
    {
        // input devices
        mouse = std::make_unique<Mouse>();
        runtime_assert(mouse.get(), "Mouse failed to initialize!");
        this->inputDevices.push_back(mouse.get());
        this->mice.push_back(mouse.get());

        keyboard = std::make_unique<Keyboard>();
        runtime_assert(keyboard.get(), "Keyboard failed to initialize!");
        this->inputDevices.push_back(keyboard.get());
        this->keyboards.push_back(keyboard.get());

        // create graphics through environment
        g.reset(env->createRenderer());
        runtime_assert(g.get(), "Graphics failed to initialize!");
        g->init();  // needs init() separation due to potential graphics access

        // make unique_ptrs for the rest
        networkHandler = std::make_unique<NetworkHandler>();
        runtime_assert(networkHandler.get(), "Network handler failed to initialize!");

        resourceManager = std::make_unique<ResourceManager>();
        runtime_assert(resourceManager.get(), "Resource manager menu failed to initialize!");
        resourceManager->setSyncLoadMaxBatchSize(512);

        SoundEngine::SndEngineType type = Env::cfg(AUD::BASS)     ? SoundEngine::BASS
                                          : Env::cfg(AUD::SOLOUD) ? SoundEngine::SOLOUD
                                                                  : SoundEngine::BASS;
        {
            auto args = env->getLaunchArgs();
            auto soundString = args["-sound"].value_or("bass").trim();
            soundString.lowerCase();
            if(Env::cfg(AUD::BASS) && soundString == "bass")
                type = SoundEngine::BASS;
            else if(Env::cfg(AUD::SOLOUD) && soundString == "soloud")
                type = SoundEngine::SOLOUD;
        }
        soundEngine.reset(SoundEngine::createSoundEngine(type));
        runtime_assert(soundEngine.get(), "Sound engine failed to initialize!");

        animationHandler = std::make_unique<AnimationHandler>();
        runtime_assert(animationHandler.get(), "Animation handler failed to initialize!");

        init_discord_sdk();

        // default launch overrides
        g->setVSync(false);

        // engine time starts now
        this->dTime = Timing::getTimeReal();
    }
    debugLog("Engine: Initializing subsystems done.\n");
}

Engine::~Engine() {
    debugLog("-= Engine Shutdown =-\n");

    // reset() all static unique_ptrs
    debugLog("Engine: Freeing app...\n");
    app.reset();

    debugLog("Engine: Freeing engine GUI...\n");
    this->consoleBox = nullptr;
    SAFE_DELETE(this->guiContainer);

    destroy_discord_sdk();

    debugLog("Engine: Freeing animation handler...\n");
    animationHandler.reset();

    debugLog("Engine: Freeing resource manager...\n");
    resourceManager.reset();

    debugLog("Engine: Freeing Sound...\n");
    soundEngine.reset();

    debugLog("Engine: Freeing network handler...\n");
    networkHandler.reset();

    debugLog("Engine: Freeing graphics...\n");
    g.reset();

    debugLog("Engine: Freeing input devices...\n");
    // first remove the mouse and keyboard from the input devices
    std::erase_if(this->inputDevices,
                  [](InputDevice *device) { return device == mouse.get() || device == keyboard.get(); });

    // delete remaining input devices (if any)
    for(auto *device : this->inputDevices) {
        delete device;
    }
    this->inputDevices.clear();
    this->mice.clear();
    this->keyboards.clear();

    // reset the static unique_ptrs
    mouse.reset();
    keyboard.reset();

    debugLog("Engine: Freeing fonts...\n");
    McFont::cleanupSharedResources();

    debugLog("Engine: Goodbye.\n");

    engine = nullptr;
}

void Engine::loadApp() {
    // create directories we will assume already exist later on
    if(!env->directoryExists(MCENGINE_DATA_DIR "avatars")) {
        env->createDirectory(MCENGINE_DATA_DIR "avatars");
    }
    if(!env->directoryExists(MCENGINE_DATA_DIR "cfg")) {
        env->createDirectory(MCENGINE_DATA_DIR "cfg");
    }
    if(!env->directoryExists(MCENGINE_DATA_DIR "maps")) {
        env->createDirectory(MCENGINE_DATA_DIR "maps");
    }
    if(!env->directoryExists(MCENGINE_DATA_DIR "replays")) {
        env->createDirectory(MCENGINE_DATA_DIR "replays");
    }
    if(!env->directoryExists(MCENGINE_DATA_DIR "screenshots")) {
        env->createDirectory(MCENGINE_DATA_DIR "screenshots");
    }
    if(!env->directoryExists(MCENGINE_DATA_DIR "skins")) {
        env->createDirectory(MCENGINE_DATA_DIR "skins");
    }

    // load core default resources
    debugLog("Engine: Loading default resources ...\n");
    resourceManager->loadFont("weblysleekuisb.ttf", "FONT_DEFAULT", 15, true, env->getDPI());
    resourceManager->loadFont("tahoma.ttf", "FONT_CONSOLE", 8, false, 96);
    debugLog("Engine: Loading default resources done.\n");

    // load other default resources and things which are not strictly necessary
    {
        MISSING_TEXTURE = resourceManager->createImage(512, 512);
        resourceManager->setResourceName(MISSING_TEXTURE, "MISSING_TEXTURE");
        for(int x = 0; x < 512; x++) {
            for(int y = 0; y < 512; y++) {
                int rowCounter = (x / 64);
                int columnCounter = (y / 64);
                Color color = (((rowCounter + columnCounter) % 2 == 0) ? rgb(255, 0, 221) : rgb(0, 0, 0));
                MISSING_TEXTURE->setPixel(x, y, color);
            }
        }
        MISSING_TEXTURE->load();

        // create engine gui
        this->guiContainer = new CBaseUIContainer(0, 0, engine->getScreenWidth(), engine->getScreenHeight(), "");
        Engine::consoleBox = new ConsoleBox();
        this->guiContainer->addBaseUIElement(this->consoleBox);
        this->visualProfiler = new VisualProfiler();
        this->guiContainer->addBaseUIElement(this->visualProfiler);

        // (engine hardcoded hotkeys come first, then engine gui)
        keyboard->addListener(this->guiContainer, true);
        keyboard->addListener(this, true);
    }

    debugLog("Engine: Loading app ...\n");
    {
        //*****************//
        //	Load App here  //
        //*****************//

        app = std::make_unique<App>();
        runtime_assert(app.get(), "App failed to initialize!");
        resourceManager->resetSyncLoadMaxBatchSize();
        // start listening to the default keyboard input
        keyboard->addListener(app.get());
    }
    debugLog("Engine: Loading app done.\n");
}

void Engine::onPaint() {
    VPROF_BUDGET("Engine::onPaint", VPROF_BUDGETGROUP_DRAW);
    if(this->bBlackout || this->bIsMinimized) return;

    this->bDrawing = true;
    {
        // begin
        {
            VPROF_BUDGET("Graphics::beginScene", VPROF_BUDGETGROUP_DRAW);
            g->beginScene();
        }

        // middle
        {
            if(app != nullptr) {
                VPROF_BUDGET("App::draw", VPROF_BUDGETGROUP_DRAW);
                app->draw();
            }

            if(this->guiContainer != nullptr) this->guiContainer->draw();

            // debug input devices
            for(const auto &inputDevice : this->inputDevices) {
                inputDevice->draw();
            }
        }

        // end
        {
            VPROF_BUDGET("Graphics::endScene", VPROF_BUDGETGROUP_DRAW_SWAPBUFFERS);
            g->endScene();
        }
    }
    this->bDrawing = false;

    this->iFrameCount++;
}

void Engine::onUpdate() {
    VPROF_BUDGET("Engine::onUpdate", VPROF_BUDGETGROUP_UPDATE);

    if(this->bBlackout) return;

    {
        VPROF_BUDGET("Timer::update", VPROF_BUDGETGROUP_UPDATE);
        // update time
        {
            // frame time
            this->dFrameTime = std::max<double>(Timing::getTimeReal() - this->dTime, 0.00005);
            // total engine runtime
            this->dTime = Timing::getTimeReal();
            if(cv::engine_throttle.getBool()) {
                // it's more like a crude estimate but it gets the job done for use as a throttle
                if((this->fVsyncFrameCounterTime += static_cast<float>(this->dFrameTime)) >
                   env->getDisplayRefreshTime()) {
                    this->fVsyncFrameCounterTime = 0.0f;
                    ++this->iVsyncFrameCount;
                }
            }
        }
    }

    // handle pending queued resolution changes
    if(this->bResolutionChange) {
        this->bResolutionChange = false;

        if(cv::debug_engine.getBool())
            debugLog("Engine: executing pending queued resolution change to ({:d}, {:d})\n",
                     (int)this->vNewScreenSize.x, (int)this->vNewScreenSize.y);

        this->onResolutionChange(this->vNewScreenSize);
    }

    // update miscellaneous engine subsystems
    {
        {
            VPROF_BUDGET("InputDevices::update", VPROF_BUDGETGROUP_UPDATE);
            for(auto &inputDevice : this->inputDevices) {
                inputDevice->update();
            }
        }

        {
            VPROF_BUDGET("AnimationHandler::update", VPROF_BUDGETGROUP_UPDATE);
            animationHandler->update();
        }

        {
            // VPROF_BUDGET("SoundEngine::update", VPROF_BUDGETGROUP_UPDATE);
            soundEngine->update();  // currently does nothing anyways
        }

        {
            VPROF_BUDGET("ResourceManager::update", VPROF_BUDGETGROUP_UPDATE);
            resourceManager->update();
        }

        {
            VPROF_BUDGET("GUI::update", VPROF_BUDGETGROUP_UPDATE);
            // update gui
            bool propagate_clicks = true;
            if(this->guiContainer != nullptr) this->guiContainer->mouse_update(&propagate_clicks);

            // execute queued commands
            // TODO: this is shit
            if(Console::g_commandQueue.size() > 0) {
                for(const auto &i : Console::g_commandQueue) {
                    Console::processCommand(i);
                }
                Console::g_commandQueue.clear();  // reset
            }
        }
    }

    // update app
    if(app != nullptr) {
        VPROF_BUDGET("App::update", VPROF_BUDGETGROUP_UPDATE);
        app->update();
    }

    // update discord presence
    tick_discord_sdk();

    // update environment (after app, at the end here)
    {
        VPROF_BUDGET("Environment::update", VPROF_BUDGETGROUP_UPDATE);
        env->update();
    }
}

void Engine::onFocusGained() {
    this->bHasFocus = true;

    if(cv::debug_engine.getBool()) debugLog("Engine: got focus\n");

    if(app != nullptr) app->onFocusGained();
}

void Engine::onFocusLost() {
    this->bHasFocus = false;

    if(cv::debug_engine.getBool()) debugLog("Engine: lost focus\n");

    for(auto &keyboard : this->keyboards) {
        keyboard->reset();
    }

    if(app != nullptr) app->onFocusLost();

    // auto minimize on certain conditions
    if(env->isFullscreen() || env->isFullscreenWindowedBorderless()) {
        if((!env->isFullscreenWindowedBorderless() && cv::minimize_on_focus_lost_if_fullscreen.getBool()) ||
           (env->isFullscreenWindowedBorderless() &&
            cv::minimize_on_focus_lost_if_borderless_windowed_fullscreen.getBool())) {
            env->minimize();
        }
    }
}

void Engine::onMinimized() {
    this->bIsMinimized = true;
    this->bHasFocus = false;

    if(cv::debug_engine.getBool()) debugLog("Engine: window minimized\n");

    if(app != nullptr) app->onMinimized();
}

void Engine::onMaximized() {
    this->bIsMinimized = false;

    if(cv::debug_engine.getBool()) debugLog("Engine: window maximized\n");
}

void Engine::onRestored() {
    this->bIsMinimized = false;

    if(cv::debug_engine.getBool()) debugLog("Engine: window restored\n");

    if(app != nullptr) app->onRestored();
}

void Engine::onResolutionChange(vec2 newResolution) {
    debugLog(0xff00ff00, "Engine: onResolutionChange() ({:d}, {:d}) -> ({:d}, {:d})\n", (int)this->vScreenSize.x,
             (int)this->vScreenSize.y, (int)newResolution.x, (int)newResolution.y);

    // NOTE: Windows [Show Desktop] button in the superbar causes (0,0)
    if(newResolution.x < 2 || newResolution.y < 2) {
        this->bIsMinimized = true;
        newResolution = vec2(2, 2);
    }

    this->screenRect = {vec2{}, newResolution};

    // to avoid double resolutionChange
    this->bResolutionChange = false;
    this->vNewScreenSize = newResolution;

    if(this->guiContainer != nullptr) this->guiContainer->setSize(newResolution.x, newResolution.y);
    if(this->consoleBox != nullptr) Engine::consoleBox->onResolutionChange(newResolution);

    // update everything
    this->vScreenSize = newResolution;
    if(g != nullptr) g->onResolutionChange(newResolution);
    if(app != nullptr) app->onResolutionChanged(newResolution);
}

void Engine::onDPIChange() {
    debugLog(0xff00ff00, "Engine: DPI changed to {:d}\n", env->getDPI());

    if(app != nullptr) app->onDPIChanged();
}

void Engine::onShutdown() {
    if(this->bBlackout || (app != nullptr && !app->onShutdown())) return;

    this->bBlackout = true;
    soundEngine->shutdown();
    env->shutdown();
}

// hardcoded engine hotkeys
void Engine::onKeyDown(KeyboardEvent &e) {
    auto keyCode = e.getKeyCode();
    // handle ALT+F4 quit
    if(keyboard->isAltDown() && keyCode == KEY_F4) {
        this->shutdown();
        e.consume();
        return;
    }

    // handle ALT+ENTER fullscreen toggle
    if(keyboard->isAltDown() && (keyCode == KEY_ENTER || keyCode == KEY_NUMPAD_ENTER)) {
        this->toggleFullscreen();
        e.consume();
        return;
    }

    // handle CTRL+F11 profiler toggle
    if(keyboard->isControlDown() && keyCode == KEY_F11) {
        cv::vprof.setValue(cv::vprof.getBool() ? 0.0f : 1.0f);
        e.consume();
        return;
    }

    // handle profiler display mode change
    if(keyboard->isControlDown() && keyCode == KEY_TAB) {
        if(cv::vprof.getBool()) {
            if(keyboard->isShiftDown())
                this->visualProfiler->decrementInfoBladeDisplayMode();
            else
                this->visualProfiler->incrementInfoBladeDisplayMode();
            e.consume();
            return;
        }
    }
}

void Engine::shutdown() { this->onShutdown(); }

void Engine::restart() {
    this->onShutdown();
    env->restart();
}

void Engine::focus() { env->focus(); }

void Engine::center() { env->center(); }

void Engine::toggleFullscreen() {
    if(env->isFullscreen())
        env->disableFullscreen();
    else
        env->enableFullscreen();
}

void Engine::disableFullscreen() { env->disableFullscreen(); }

void Engine::showMessageInfo(const UString &title, const UString &message) {
    debugLog("INFO: [{:s}] | {:s}\n", title.toUtf8(), message.toUtf8());
    env->showMessageInfo(title, message);
}

void Engine::showMessageWarning(const UString &title, const UString &message) {
    debugLog("WARNING: [{:s}] | {:s}\n", title.toUtf8(), message.toUtf8());
    env->showMessageWarning(title, message);
}

void Engine::showMessageError(const UString &title, const UString &message) {
    debugLog("ERROR: [{:s}] | {:s}\n", title.toUtf8(), message.toUtf8());
    env->showMessageError(title, message);
}

void Engine::showMessageErrorFatal(const UString &title, const UString &message) {
    debugLog("FATAL ERROR: [{:s}] | {:s}\n", title.toUtf8(), message.toUtf8());
    env->showMessageErrorFatal(title, message);
}

void Engine::requestResolutionChange(vec2 newResolution) {
    if(newResolution == this->vNewScreenSize) return;

    this->vNewScreenSize = newResolution;
    this->bResolutionChange = true;
}

void Engine::onEngineThrottleChanged(float newVal) {
    const bool enable = !!static_cast<int>(newVal);
    if(!enable) {
        this->fVsyncFrameCounterTime = 0.0f;
        this->iVsyncFrameCount = 0;
    }
}

void Engine::logToConsole(std::optional<Color> color, const UString &msg) {
    if(Engine::consoleBox != nullptr) {
        if(color.has_value())
            Engine::consoleBox->log(msg, color.value());
        else
            Engine::consoleBox->log(msg);
    }
}

//**********************//
//	Engine ConCommands	//
//**********************//

void _restart(void) { engine->restart(); }

void _printsize(void) {
    vec2 s = engine->getScreenSize();
    debugLog("Engine: screenSize = ({:f}, {:f})\n", s.x, s.y);
}

void _borderless(void) {
    if(cv::fullscreen_windowed_borderless.getBool()) {
        cv::fullscreen_windowed_borderless.setValue(0.0f);
        if(env->isFullscreen()) env->disableFullscreen();
    } else {
        cv::fullscreen_windowed_borderless.setValue(1.0f);
        if(!env->isFullscreen()) env->enableFullscreen();
    }
}

void _minimize(void) { env->minimize(); }

void _maximize(void) { env->maximize(); }

void _toggleresizable(void) { env->setWindowResizable(!env->isWindowResizable()); }

void _focus(void) { engine->focus(); }

void _center(void) { engine->center(); }

void _errortest(void) {
    engine->showMessageError(
        "Error Test",
        "This is an error message, fullscreen mode should be disabled and you should be able to read this");
}

void _dpiinfo(void) {
    debugLog("env->getDPI() = {:d}, env->getDPIScale() = {:f}\n", env->getDPI(), env->getDPIScale());
}
