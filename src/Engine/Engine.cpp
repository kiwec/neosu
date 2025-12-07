// Copyright (c) 2012, PG, All rights reserved.
#include "Engine.h"

#include "Environment.h"

#include "App.h"
#include "MakeDelegateWrapper.h"

#include "AsyncIOHandler.h"
#include "AnimationHandler.h"
#include "CBaseUIContainer.h"
#include "ConVar.h"
#include "ConsoleBox.h"
#include "DirectoryWatcher.h"
#include "DiscordInterface.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "NetworkHandler.h"
#include "Profiler.h"
#include "ResourceManager.h"
#include "SoundEngine.h"
#include "Timing.h"
#include "Logging.h"
#include "VisualProfiler.h"
#include "SString.h"
#include "crypto.h"
#include "Font.h"
#include "Image.h"

Image *MISSING_TEXTURE{nullptr};

std::unique_ptr<Mouse> mouse{nullptr};
std::unique_ptr<Keyboard> keyboard{nullptr};
std::unique_ptr<App> app{nullptr};
std::unique_ptr<Graphics> g{nullptr};
std::unique_ptr<SoundEngine> soundEngine{nullptr};
std::unique_ptr<ResourceManager> resourceManager{nullptr};
std::unique_ptr<NetworkHandler> networkHandler{nullptr};
std::unique_ptr<AnimationHandler> anim{nullptr};
std::unique_ptr<AsyncIOHandler> io{nullptr};
std::unique_ptr<DirectoryWatcher> directoryWatcher{nullptr};

mcatomic_shptr<ConsoleBox> Engine::consoleBox{nullptr};

Engine *engine{nullptr};
Engine::Engine() {
    engine = this;

    // init rng seeding for C rand()
    srand(crypto::rng::get_rand<u32>());

    // always keep a dummy App() alive so we don't have to null-check for "app" inside engine code
    app.reset(App::create(true));

    this->guiContainer = nullptr;
    this->visualProfiler = nullptr;

    // print debug information
    debugLog("-= Engine Startup =-");
    debugLog("cmdline: {:s}", SString::join(env->getCommandLine()));

    // timing
    this->iFrameCount = 0;
    this->iVsyncFrameCount = 0;
    this->fVsyncFrameCounterTime = 0.0f;
    this->dFrameTime = 0.016f;

    cv::engine_throttle.setCallback(SA::MakeDelegate<&Engine::onEngineThrottleChanged>(this));

    // screen
    this->bResolutionChange = false;
    this->screenRect = {{}, env->getWindowSize()};
    this->vNewScreenSize = this->screenRect.getSize();

    debugLog("Engine: ScreenSize = ({}x{})", (int)this->screenRect.getWidth(), (int)this->screenRect.getHeight());

    // custom
    this->bDrawing = false;
    this->bShuttingDown = false;

    // initialize all engine subsystems (the order does matter!)
    debugLog("Engine: Initializing subsystems ...");
    {
        // async io
        io = std::make_unique<AsyncIOHandler>();
        directoryWatcher = std::make_unique<DirectoryWatcher>();
        this->runtime_assert(!!io && io->succeeded() && !!directoryWatcher, "I/O subsystem failed to initialize!");

        // shared freetype init
        this->runtime_assert(McFont::initSharedResources(), "FreeType failed to initialize!");

        // input devices
        mouse = std::make_unique<Mouse>();
        this->runtime_assert(!!mouse, "Mouse failed to initialize!");
        this->inputDevices.push_back(mouse.get());
        this->mice.push_back(mouse.get());

        keyboard = std::make_unique<Keyboard>();
        this->runtime_assert(!!keyboard, "Keyboard failed to initialize!");
        this->inputDevices.push_back(keyboard.get());
        this->keyboards.push_back(keyboard.get());

        // create graphics through environment
        g.reset(env->createRenderer());
        // needs init() separation due to potential graphics access
        this->runtime_assert(!!g && g->init(), "Graphics failed to initialize!");

        // make unique_ptrs for the rest
        networkHandler = std::make_unique<NetworkHandler>();
        this->runtime_assert(!!networkHandler, "Network handler failed to initialize!");

        resourceManager = std::make_unique<ResourceManager>();
        this->runtime_assert(!!resourceManager, "Resource manager menu failed to initialize!");
        resourceManager->setSyncLoadMaxBatchSize(512);

        soundEngine.reset(SoundEngine::initialize());
        this->runtime_assert(!!soundEngine && soundEngine->succeeded(), "Sound engine failed to initialize!");

        anim = std::make_unique<AnimationHandler>();
        this->runtime_assert(!!anim, "Animation handler failed to initialize!");

        DiscRPC::init();

        // default launch overrides
        g->setVSync(false);

        // engine time starts now
        this->dTime = Timing::getTimeReal();
    }
    debugLog("Engine: Initializing subsystems done.");
}

Engine::~Engine() {
    debugLog("-= Engine Shutdown =-");

    // reset() all global unique_ptrs
    debugLog("Engine: Freeing app...");
    app.reset(App::create(true));  // re-create a dummy app and delete it again at the end

    debugLog("Engine: Freeing engine GUI...");
    if(const auto &cbox = Engine::consoleBox.load(std::memory_order_acquire); cbox != nullptr) {
        // don't allow CBaseUI to delete it, it might still be in use (being flushed) by Logger
        this->guiContainer->removeBaseUIElement(cbox.get());
    }
    Engine::consoleBox.store(nullptr, std::memory_order_release);
    SAFE_DELETE(this->guiContainer);

    DiscRPC::destroy();

    debugLog("Engine: Freeing animation handler...");
    anim.reset();

    debugLog("Engine: Freeing resource manager...");
    resourceManager.reset();

    debugLog("Engine: Freeing Sound...");
    soundEngine.reset();

    debugLog("Engine: Freeing network handler...");
    networkHandler.reset();

    debugLog("Engine: Freeing graphics...");
    g.reset();

    debugLog("Engine: Freeing input devices...");
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

    debugLog("Engine: Freeing fonts...");
    McFont::cleanupSharedResources();

    debugLog("Engine: Stopping I/O subsystem...");
    directoryWatcher.reset();

    io->cleanup();
    io.reset();

    debugLog("Engine: Goodbye.");

    app.reset();  // delete the dummy App() for real
    engine = nullptr;
}

void Engine::loadApp() {
    if(this->bShuttingDown) return;
    // load core default resources
    debugLog("Engine: Loading default resources ...");
    this->defaultFont = resourceManager->loadFont("weblysleekuisb.ttf", "FONT_DEFAULT", 15, true, env->getDPI());
    this->consoleFont = resourceManager->loadFont("tahoma.ttf", "FONT_CONSOLE", 8, false, 96);

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
        Engine::consoleBox.store(std::make_shared<ConsoleBox>(), std::memory_order_release);
        this->guiContainer->addBaseUIElement(Engine::consoleBox.load(std::memory_order_acquire).get());
        this->visualProfiler = new VisualProfiler();
        this->guiContainer->addBaseUIElement(this->visualProfiler);

        // (engine hardcoded hotkeys come first, then engine gui)
        keyboard->addListener(this->guiContainer, true);
        keyboard->addListener(this, true);
    }

    debugLog("Engine: Loading app ...");
    {
        //*****************//
        //	Load App here  //
        //*****************//

#ifndef BUILD_TOOLS_ONLY
        app.reset(App::create(false));
        this->runtime_assert(!!app, "App failed to initialize!");

        resourceManager->resetSyncLoadMaxBatchSize();
#endif

        // start listening to the default keyboard input
        keyboard->addListener(app.get());
    }
    debugLog("Engine: Loading app done.");
}

void Engine::onPaint() {
    VPROF_BUDGET("Engine::onPaint", VPROF_BUDGETGROUP_DRAW);
    if(this->bShuttingDown) return;

    this->bDrawing = true;
    {
        // begin
        {
            VPROF_BUDGET("Graphics::beginScene", VPROF_BUDGETGROUP_DRAW);
            g->beginScene();
        }

        // middle
        {
            {
                VPROF_BUDGET("App::draw", VPROF_BUDGETGROUP_DRAW);
                app->draw();
            }

            if(this->guiContainer) this->guiContainer->draw();

            // debug input devices
            for(auto *inputDevice : this->inputDevices) {
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

    if(this->bShuttingDown) return;

    {
        VPROF_BUDGET("Timer::update", VPROF_BUDGETGROUP_UPDATE);
        // update time
        {
            // frame time
            double now = Timing::getTimeReal();
            this->dFrameTime = std::max<double>(now - this->dTime, 0.00005);
            // total engine runtime
            this->dTime = now;
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

        logIfCV(debug_engine, "executing pending queued resolution change to ({})", this->vNewScreenSize);

        this->onResolutionChange(this->vNewScreenSize);
    }

    // update miscellaneous engine subsystems
    {
        {
            VPROF_BUDGET("AsyncIO::update", VPROF_BUDGETGROUP_UPDATE);
            io->update();
        }

        {
            VPROF_BUDGET("DirectoryWatcher::update", VPROF_BUDGETGROUP_UPDATE);
            directoryWatcher->update();
        }

        {
            VPROF_BUDGET("InputDevices::update", VPROF_BUDGETGROUP_UPDATE);
            for(auto *inputDevice : this->inputDevices) {
                inputDevice->update();
            }
        }

        {
            VPROF_BUDGET("AnimationHandler::update", VPROF_BUDGETGROUP_UPDATE);
            anim->update();
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
            if(this->guiContainer) this->guiContainer->mouse_update(&propagate_clicks);
        }

        {
            VPROF_BUDGET("NetworkHandler::update", VPROF_BUDGETGROUP_UPDATE);
            // run networking response callbacks, if any
            networkHandler->update();
        }
    }

    // update app
    {
        VPROF_BUDGET("App::update", VPROF_BUDGETGROUP_UPDATE);
        app->update();
    }

    // update discord presence
    DiscRPC::tick();

    // update environment (after app, at the end here)
    {
        VPROF_BUDGET("Environment::update", VPROF_BUDGETGROUP_UPDATE);
        env->update();
    }
}

void Engine::onFocusGained() {
    logIfCV(debug_engine, "got focus");

    if(soundEngine) soundEngine->onFocusGained();  // switch shared->exclusive if applicable
    app->onFocusGained();
}

void Engine::onFocusLost() {
    logIfCV(debug_engine, "lost focus");

    for(auto *keyboard : this->keyboards) {
        keyboard->reset();
    }

    if(soundEngine) soundEngine->onFocusLost();  // switch exclusive->shared if applicable
    app->onFocusLost();

    // auto minimize on certain conditions
    if(env->winFullscreened() && (cv::minimize_on_focus_lost_if_borderless_windowed_fullscreen.getBool() ||
                                  cv::minimize_on_focus_lost_if_fullscreen.getBool())) {
        env->minimize();
    }
}

void Engine::onMinimized() {
    logIfCV(debug_engine, "window minimized");

    app->onMinimized();
}

void Engine::onMaximized() { logIfCV(debug_engine, "window maximized"); }

void Engine::onRestored() {
    logIfCV(debug_engine, "window restored");

    if(g) g->onRestored();
    app->onRestored();
}

void Engine::onResolutionChange(vec2 newResolution) {
    debugLog("Engine: onResolutionChange() ({:d}, {:d}) -> ({:d}, {:d})", (int)this->screenRect.getWidth(),
             (int)this->screenRect.getHeight(), (int)newResolution.x, (int)newResolution.y);

    // NOTE: Windows [Show Desktop] button in the superbar causes (0,0)
    if(newResolution.x < 2 || newResolution.y < 2) {
        newResolution = vec2(2, 2);
    }

    // to avoid double resolutionChange
    this->bResolutionChange = false;
    this->vNewScreenSize = newResolution;
    this->screenRect = {vec2{}, newResolution};

    if(this->guiContainer) this->guiContainer->setSize(newResolution.x, newResolution.y);
    if(const auto &cbox = Engine::consoleBox.load(std::memory_order_relaxed); cbox != nullptr) {
        cbox->onResolutionChange(newResolution);
    }

    // update everything
    if(g) g->onResolutionChange(newResolution);
    app->onResolutionChanged(newResolution);
}

void Engine::onDPIChange() {
    debugLog("Engine: DPI changed to {:d}", env->getDPI());

    app->onDPIChanged();
}

void Engine::onShutdown() {
    if(this->bShuttingDown || !app->onShutdown()) return;

    this->bShuttingDown = true;
    if(!!soundEngine) soundEngine->shutdown();
    env->shutdown();
}

// hardcoded engine hotkeys
void Engine::onKeyDown(KeyboardEvent &e) {
    auto keyCode = e.getScanCode();
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

void Engine::restart() {
    this->onShutdown();
    env->restart();
}

void Engine::focus() { env->focus(); }

void Engine::center() { env->center(); }

void Engine::toggleFullscreen() {
    if(env->winFullscreened())
        env->disableFullscreen();
    else
        env->enableFullscreen();
}

void Engine::disableFullscreen() { env->disableFullscreen(); }

void Engine::showMessageInfo(const UString &title, const UString &message) {
    debugLog("INFO: [{:s}] | {:s}", title, message);
    env->showMessageInfo(title, message);
}

void Engine::showMessageWarning(const UString &title, const UString &message) {
    debugLog("WARNING: [{:s}] | {:s}", title, message);
    env->showMessageWarning(title, message);
}

void Engine::showMessageError(const UString &title, const UString &message) {
    debugLog("ERROR: [{:s}] | {:s}", title, message);
    Logger::flush();
    env->showMessageError(title, message);
}

void Engine::showMessageErrorFatal(const UString &title, const UString &message) {
    debugLog("FATAL ERROR: [{:s}] | {:s}", title, message);
    Logger::flush();
    env->showMessageErrorFatal(title, message);
}

void Engine::runtime_assert(bool cond, const char *reason) {
    if(cond) return;
    this->showMessageErrorFatal("Engine Error", reason);
    fubar_abort();
}

void Engine::requestResolutionChange(vec2 newResolution) {
    if(env->winMinimized()) return;
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

//**********************//
//	Engine ConCommands	//
//**********************//

void _printsize() {
    vec2 s = engine->getScreenSize();
    debugLog("Engine: screenSize = ({:f}, {:f})", s.x, s.y);
}

void _borderless() {
    if(cv::fullscreen_windowed_borderless.getBool()) {
        cv::fullscreen_windowed_borderless.setValue(0.0f);
        if(env->winFullscreened()) env->disableFullscreen();
    } else {
        cv::fullscreen_windowed_borderless.setValue(1.0f);
        if(!env->winFullscreened()) env->enableFullscreen();
    }
}

void _errortest() {
    engine->showMessageError(
        "Error Test",
        "This is an error message, fullscreen mode should be disabled and you should be able to read this");
}

void _restart() { engine->restart(); }
void _minimize() { env->minimize(); }
void _maximize() { env->maximize(); }
void _toggleresizable() { env->setWindowResizable(!env->winResizable()); }
void _focus() { engine->focus(); }
void _center() { engine->center(); }
void _dpiinfo() { debugLog("env->getDPI() = {:d}, env->getDPIScale() = {:f}", env->getDPI(), env->getDPIScale()); }
