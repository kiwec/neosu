#pragma once
#include <source_location>

#include "KeyboardListener.h"
#include "Timing.h"
#include "cbase.h"
#include "fmt/color.h"
#include "fmt/printf.h"

class App;
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
#define debugLogF(...) Engine::ContextLogger::log(std::source_location::current(), __FUNCTION__, __VA_ARGS__)
#define debugLog(...) \
    Engine::ContextLogger::logPrintf(std::source_location::current(), __FUNCTION__, __VA_ARGS__)  // deprecated
#else
#define debugLogF(...) Engine::ContextLogger::log(__FUNCTION__, __VA_ARGS__)
#define debugLog(...) Engine::ContextLogger::logPrintf(__FUNCTION__, __VA_ARGS__)  // deprecated
#endif

class Engine final : public KeyboardListener {
   public:
    Engine(i32 argc, char **argv);
    ~Engine();

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
    void onResolutionChange(Vector2 newResolution);
    void onDPIChange();
    void onShutdown();

    // primary keyboard messages
    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &) override { ; }
    void onChar(KeyboardEvent &) override { ; }

    // convenience functions (passthroughs)
    void shutdown();
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
    void requestResolutionChange(Vector2 newResolution);
    [[nodiscard]] constexpr McRect getScreenRect() const { return this->screenRect; }
    [[nodiscard]] constexpr Vector2 getScreenSize() const { return this->vScreenSize; }
    [[nodiscard]] constexpr int getScreenWidth() const { return (int)this->vScreenSize.x; }
    [[nodiscard]] constexpr int getScreenHeight() const { return (int)this->vScreenSize.y; }

    // vars
    void setFrameTime(double delta);
    [[nodiscard]] constexpr double getTime() const { return this->dTime; }
    double getTimeReal();
    [[nodiscard]] constexpr double getTimeRunning() const { return this->dRunTime; }
    [[nodiscard]] constexpr double getFrameTime() const { return this->dFrameTime; }
    [[nodiscard]] constexpr unsigned long getFrameCount() const { return this->iFrameCount; }

    [[nodiscard]] constexpr bool hasFocus() const { return this->bHasFocus; }
    [[nodiscard]] constexpr bool isDrawing() const { return this->bDrawing; }
    [[nodiscard]] constexpr bool isMinimized() const { return this->bIsMinimized; }

    // debugging/console
    void setConsole(Console *console) { this->console = console; }
    [[nodiscard]] constexpr ConsoleBox *getConsoleBox() const { return this->consoleBox; }
    [[nodiscard]] constexpr Console *getConsole() const { return this->console; }
    [[nodiscard]] constexpr CBaseUIContainer *getGUI() const { return this->guiContainer; }

    // input devices
    std::vector<Mouse *> mice;
    std::vector<Keyboard *> keyboards;
    std::vector<InputDevice *> inputDevices;

    // timing
    Timer *timer;
    double dTime;
    double dRunTime;
    unsigned long iFrameCount;
    double dFrameTime;

    // primary screen
    McRect screenRect;
    Vector2 vScreenSize;
    Vector2 vNewScreenSize;
    bool bResolutionChange;

    // window
    bool bHasFocus;
    bool bIsMinimized;

    // engine gui, mostly for debugging
    CBaseUIContainer *guiContainer;
    VisualProfiler *visualProfiler;
    static ConsoleBox *consoleBox;
    static Console *console;

    // custom
    i32 iArgc;
    char **sArgv;
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

        // debug build shows full source location
        template <typename... Args>
        //[[deprecated("use compile-time format string checked debugLogF instead")]]
        static void logPrintf(const std::source_location &loc, const char *func, const std::string_view &fmt,
                              Args &&...args) {
            auto contextPrefix = fmt::format("[{}:{}:{}] [{}]: ", Environment::getFileNameFromFilePath(loc.file_name()),
                                             loc.line(), loc.column(), func);

            std::string message = fmt::sprintf(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message);
        }

        template <typename... Args>
        //[[deprecated("use compile-time format string checked debugLogF instead")]]
        static void logPrintf(const std::source_location &loc, const char *func, Color color,
                              const std::string_view &fmt, Args &&...args) {
            auto contextPrefix = fmt::format("[{}:{}:{}] [{}]: ", Environment::getFileNameFromFilePath(loc.file_name()),
                                             loc.line(), loc.column(), func);

            std::string message = fmt::sprintf(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message, color);
        }

        // release build only shows function name
        template <typename... Args>
        //[[deprecated("use compile-time format string checked debugLogF instead")]]
        static void logPrintf(const char *func, const std::string_view &fmt, Args &&...args) {
            auto contextPrefix = fmt::format("[{}] ", func);
            trim_to_last_scope(contextPrefix);
            std::string message = fmt::sprintf(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message);
        }

        template <typename... Args>
        //[[deprecated("use compile-time format string checked debugLogF instead")]]
        static void logPrintf(const char *func, Color color, const std::string_view &fmt, Args &&...args) {
            auto contextPrefix = fmt::format("[{}] ", func);
            trim_to_last_scope(contextPrefix);
            std::string message = fmt::sprintf(fmt, std::forward<Args>(args)...);
            Engine::logImpl(contextPrefix + message, color);
        }
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
        if constexpr(Env::cfg(OS::WINDOWS))  // hmm... odd bug with fmt::print (or mingw?), when the stdout isn't
                                             // redirected to a file
        {
            if(color == rgb(255, 255, 255) || !Environment::isaTTY())
                printf("%s", message.c_str());
            else
                printf("%s", fmt::format(fmt::fg(fmt::rgb(color.R(), color.G(), color.B())), "{}", message).c_str());
        } else {
            if(color == rgb(255, 255, 255) || !Environment::isaTTY())
                fmt::print("{}", message);
            else
                fmt::print(fmt::fg(fmt::rgb(color.R(), color.G(), color.B())), "{}", message);
        }
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
