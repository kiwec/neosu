#pragma once
// Copyright (c) 2012, PG, All rights reserved.
#include <source_location>

#include "KeyboardListener.h"
#include "Timing.h"
#include "App.h"
#include "fmt/color.h"

class Mouse;
class ConVar;
class Keyboard;
class InputDevice;
class SoundEngine;
class NetworkHandler;
class ResourceManager;
class AnimationHandler;

class CBaseUIContainer;
class VisualProfiler;
class ConsoleBox;
class Console;

#ifdef _DEBUG
#define debugLog(...) Engine::ContextLogger::log(std::source_location::current(), __FUNCTION__, __VA_ARGS__)
#else
#define debugLog(...) Engine::ContextLogger::log(__FUNCTION__, __VA_ARGS__)
#endif

class Engine final : public KeyboardListener {
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
    void blackout() { this->bBlackout = true; }

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
    [[nodiscard]] constexpr unsigned long getFrameCount() const { return this->iFrameCount; }

    // clang-format off
    // NOTE: if engine_throttle cvar is off, this will always return true
    [[nodiscard]] inline bool throttledShouldRun(unsigned int howManyVsyncFramesToWaitBetweenExecutions) { return (this->fVsyncFrameCounterTime == 0.0f) && !(this->iVsyncFrameCount % howManyVsyncFramesToWaitBetweenExecutions);}
    // clang-format on

    [[nodiscard]] constexpr bool hasFocus() const { return this->bHasFocus; }
    [[nodiscard]] constexpr bool isDrawing() const { return this->bDrawing; }
    [[nodiscard]] constexpr bool isMinimized() const { return this->bIsMinimized; }

    // debugging/console
    [[nodiscard]] constexpr ConsoleBox *getConsoleBox() const { return this->consoleBox; }
    [[nodiscard]] constexpr CBaseUIContainer *getGUI() const { return this->guiContainer; }

   private:
    // input devices
    std::vector<Mouse *> mice;
    std::vector<Keyboard *> keyboards;
    std::vector<InputDevice *> inputDevices;

    // timing
    f64 dTime;
    unsigned long iFrameCount;
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
    static ConsoleBox *consoleBox;

    // custom
    bool bBlackout;
    bool bDrawing;

   public:
    class ContextLogger {
       private:
        static forceinline void trim_to_last_scope([[maybe_unused]] std::string &str) {
#ifdef _MSC_VER  // msvc always adds the full scope to __FUNCTION__, which we don't want for non-debug builds
            auto pos = str.rfind("::");
            if(pos != std::string::npos) {
                str.erase(1, pos + 1);
            }
#endif
        }

       public:
        // debug build shows full source location
#ifdef _DEBUG
        template <typename... Args>
        static void log(const std::source_location &loc, const char *func, fmt::format_string<Args...> fmt,
                        Args &&...args) {
            auto contextPrefix = fmt::format("[{}:{}:{}] [{}]: ", Environment::getFileNameFromFilePath(loc.file_name()),
                                             loc.line(), loc.column(), func);

            auto message = fmt::format(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message);
        }

        template <typename... Args>
        static void log(const std::source_location &loc, const char *func, Color color, fmt::format_string<Args...> fmt,
                        Args &&...args) {
            auto contextPrefix = fmt::format("[{}:{}:{}] [{}]: ", Environment::getFileNameFromFilePath(loc.file_name()),
                                             loc.line(), loc.column(), func);

            auto message = fmt::format(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message, color);
        }
#else
        // release build only shows function name
        template <typename... Args>
        static void log(const char *func, fmt::format_string<Args...> fmt, Args &&...args) {
            auto contextPrefix = fmt::format("[{}] ", func);
            trim_to_last_scope(contextPrefix);
            auto message = fmt::format(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message);
        }

        template <typename... Args>
        static void log(const char *func, Color color, fmt::format_string<Args...> fmt, Args &&...args) {
            auto contextPrefix = fmt::format("[{}] ", func);
            trim_to_last_scope(contextPrefix);
            auto message = fmt::format(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message, color);
        }
#endif
    };
    template <typename... Args>
    static void logRaw(fmt::format_string<Args...> fmt, Args &&...args) {
        auto message = fmt::format(fmt, std::forward<Args>(args)...);
        Engine::logImpl(message);
    }

   private:
    // logging stuff (implementation)
    static void logToConsole(std::optional<Color> color, const UString &message);

    static void logImpl(const std::string &message, Color color = rgb(255, 255, 255)) {
        if(color == rgb(255, 255, 255) || !Environment::isaTTY())
            FMT_PRINT("{}", message);
        else
            FMT_PRINT(fmt::fg(fmt::rgb(color.R(), color.G(), color.B())), "{}", message);
        logToConsole(color, UString(message));
    }
};

extern std::unique_ptr<Mouse> mouse;
extern std::unique_ptr<Keyboard> keyboard;
extern std::unique_ptr<App> app;
extern std::unique_ptr<Graphics> g;
extern std::unique_ptr<SoundEngine> soundEngine;
extern std::unique_ptr<ResourceManager> resourceManager;
extern std::unique_ptr<NetworkHandler> networkHandler;
extern std::unique_ptr<AnimationHandler> animationHandler;

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
