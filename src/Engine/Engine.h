#pragma once
// Copyright (c) 2012, PG, All rights reserved.

#include "EngineConfig.h"

#include "noinclude.h"
#include "types.h"

#include "Rect.h"
#include "KeyboardListener.h"
#include "CompatShims.h"

#include <vector>
#include <memory>

#ifndef APP_H
class App;
#endif

class Graphics;
class Mouse;
class ConVar;
class Keyboard;
class InputDevice;
class SoundEngine;
class NetworkHandler;
class ResourceManager;
class AnimationHandler;
class AsyncIOHandler;

class CBaseUIContainer;
class VisualProfiler;
class ConsoleBox;
class Console;
class UString;
class Image;

class Engine final : public KeyboardListener {
    NOCOPY_NOMOVE(Engine)
   public:
    Engine();
    ~Engine() override;

    // app
    void loadApp();

    // render/update
    void onPaint();
    void onUpdate();

    // window messages
    void onFocusGained();
    void onFocusLost();
    void onMinimized();
    void onMaximized();
    void onRestored();
    void onResolutionChange(vec2 newResolution);
    void onDPIChange();
    void onShutdown();

    // primary keyboard messages
    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &) override { ; }
    void onChar(KeyboardEvent &) override { ; }

    // convenience functions (passthroughs)
    inline void shutdown() { this->onShutdown(); }
    void restart();
    void focus();
    void center();
    void toggleFullscreen();
    void disableFullscreen();

    void showMessageInfo(const UString &title, const UString &message);
    void showMessageWarning(const UString &title, const UString &message);
    void showMessageError(const UString &title, const UString &message);
    void showMessageErrorFatal(const UString &title, const UString &message);

    // engine specifics
    [[nodiscard]] inline bool isShuttingDown() const { return this->bShuttingDown; }

    // interfaces
   public:
    [[nodiscard]] inline const std::vector<Mouse *> &getMice() const { return this->mice; }
    [[nodiscard]] inline const std::vector<Keyboard *> &getKeyboards() const { return this->keyboards; }

    // screen
    void requestResolutionChange(vec2 newResolution);
    [[nodiscard]] constexpr McRect getScreenRect() const { return this->screenRect; }
    [[nodiscard]] constexpr vec2 getScreenSize() const { return this->vScreenSize; }
    [[nodiscard]] constexpr int getScreenWidth() const { return (int)this->vScreenSize.x; }
    [[nodiscard]] constexpr int getScreenHeight() const { return (int)this->vScreenSize.y; }

    // vars
    [[nodiscard]] constexpr double getTime() const { return this->dTime; }
    [[nodiscard]] constexpr double getFrameTime() const { return this->dFrameTime; }
    [[nodiscard]] constexpr u64 getFrameCount() const { return this->iFrameCount; }

    // clang-format off
    // NOTE: if engine_throttle cvar is off, this will always return true
    [[nodiscard]] inline bool throttledShouldRun(unsigned int howManyVsyncFramesToWaitBetweenExecutions) {
        return howManyVsyncFramesToWaitBetweenExecutions == 0 ||
               ((this->fVsyncFrameCounterTime == 0.0f) && !(this->iVsyncFrameCount % howManyVsyncFramesToWaitBetweenExecutions));
    }
    // clang-format on

    [[nodiscard]] constexpr bool hasFocus() const { return this->bHasFocus; }
    [[nodiscard]] constexpr bool isDrawing() const { return this->bDrawing; }
    [[nodiscard]] constexpr bool isMinimized() const { return this->bIsMinimized; }

    // debugging/console
    [[nodiscard]] inline std::shared_ptr<ConsoleBox> getConsoleBox() const {
        return Engine::consoleBox.load(std::memory_order_relaxed);
    }
    [[nodiscard]] constexpr CBaseUIContainer *getGUI() const { return this->guiContainer; }

   private:
    void runtime_assert(bool cond, const char *reason);

    // input devices
    std::vector<Mouse *> mice;
    std::vector<Keyboard *> keyboards;
    std::vector<InputDevice *> inputDevices;

    // timing
    f64 dTime;
    u64 iFrameCount;
    double dFrameTime;
    // this will wrap quickly, and that's fine, it should be used as a dividend in a modular expression anyways
    uint8_t iVsyncFrameCount;
    float fVsyncFrameCounterTime;
    void onEngineThrottleChanged(float newVal);

    // primary screen
    McRect screenRect;
    vec2 vScreenSize{0.f};
    vec2 vNewScreenSize{0.f};
    bool bResolutionChange;

    // window
    bool bHasFocus;
    bool bIsMinimized;

    // engine gui, mostly for debugging
    CBaseUIContainer *guiContainer;
    VisualProfiler *visualProfiler;
    friend class Logger;
    static mcatomic_shptr<ConsoleBox> consoleBox;

    // custom
    bool bShuttingDown;
    bool bDrawing;
};

extern std::unique_ptr<Mouse> mouse;
extern std::unique_ptr<Keyboard> keyboard;
extern std::unique_ptr<App> app;
extern std::unique_ptr<Graphics> g;
extern std::unique_ptr<SoundEngine> soundEngine;
extern std::unique_ptr<ResourceManager> resourceManager;
extern std::unique_ptr<NetworkHandler> networkHandler;
extern std::unique_ptr<AnimationHandler> anim;
extern std::unique_ptr<AsyncIOHandler> io;

extern Engine *engine;

void _restart(void);
void _printsize(void);
void _fullscreen(void);
void _borderless(void);
void _minimize(void);
void _maximize(void);
void _toggleresizable(void);
void _focus(void);
void _center(void);
void _errortest(void);
void _dpiinfo(void);

// black and purple placeholder texture, valid from engine startup to shutdown
extern Image *MISSING_TEXTURE;
