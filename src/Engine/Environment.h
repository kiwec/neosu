// Copyright (c) 2015-2018, PG & 2025, WH, All rights reserved.
#pragma once

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "BaseEnvironment.h"

#include "UString.h"
#include "Cursors.h"
#include "KeyboardEvent.h"
#include "Rect.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <functional>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Cursor SDL_Cursor;
typedef struct SDL_Environment SDL_Environment;
typedef struct SDL_Rect SDL_Rect;
typedef union _XEvent XEvent;

class Graphics;
class UString;
class Engine;

class Environment;
extern Environment *env;

class Environment {
    NOCOPY_NOMOVE(Environment)
   public:
    struct Interop {
        NOCOPY_NOMOVE(Interop)
       public:
        Interop() = delete;
        Interop(Environment *env_ptr) : m_env(env_ptr) {}
        ~Interop() { m_env = nullptr; }
        bool handle_cmdline_args(const std::vector<std::string> &args);
        bool handle_cmdline_args() { return handle_cmdline_args(m_env->getCommandLine()); }
        void setup_system_integrations();
        static void handle_existing_window([[maybe_unused]] int argc,
                                           [[maybe_unused]] char *argv[]);  // only impl. for windows ATM
       private:
        Environment *m_env;
    };

   private:
    friend struct Interop;
    Interop m_interop;

   public:
    Environment(const std::unordered_map<std::string, std::optional<std::string>> &argMap,
                const std::vector<std::string> &cmdlineVec);
    virtual ~Environment();

    void update();

    // engine/factory
    Graphics *createRenderer();
#ifdef MCENGINE_FEATURE_DIRECTX11
    [[nodiscard]] inline bool usingDX11() const { return m_bUsingDX11; }
#else
    [[nodiscard]] constexpr forceinline bool usingDX11() const { return false; }
#endif

    // system
    void shutdown();
    void restart();
    [[nodiscard]] inline bool isRunning() const { return m_bRunning; }
    [[nodiscard]] inline bool isRestartScheduled() const { return m_bIsRestartScheduled; }
    [[nodiscard]] inline Interop &getEnvInterop() { return m_interop; }

    // resolved and cached at early startup with argv[0]
    // contains the full canonical path to the current exe
    static const std::string &getPathToSelf(const char *argv0 = nullptr);

    // i.e. getenv()
    static std::string getEnvVariable(std::string_view varToQuery) noexcept;
    // i.e. setenv()
    static bool setEnvVariable(std::string_view varToSet, std::string_view varValue, bool overwrite = true) noexcept;
    // i.e. unsetenv()
    static bool unsetEnvVariable(std::string_view varToUnset) noexcept;

    static const std::string &getExeFolder();
    static void setThreadPriority(float newVal);  // != 0.0 : high

    static void openURLInDefaultBrowser(std::string_view url) noexcept;

    [[nodiscard]] inline const std::unordered_map<std::string, std::optional<std::string>> &getLaunchArgs() const {
        return m_mArgMap;
    }
    [[nodiscard]] inline const std::vector<std::string> &getCommandLine() const { return m_vCmdLine; }

    // returns at least 1
    static int getLogicalCPUCount() noexcept;

    // user
    [[nodiscard]] const UString &getUsername() const noexcept;
    [[nodiscard]] const std::string &getUserDataPath() const noexcept;
    [[nodiscard]] const std::string &getLocalDataPath() const noexcept;

    // file IO
    [[nodiscard]] static bool fileExists(std::string &filename) noexcept;  // passthroughs to McFile
    [[nodiscard]] static bool directoryExists(std::string &directoryName) noexcept;
    [[nodiscard]] static bool fileExists(std::string_view filename) noexcept;
    [[nodiscard]] static bool directoryExists(std::string_view directoryName) noexcept;

    static bool createDirectory(std::string_view directoryName) noexcept;
    static bool renameFile(const std::string &oldFileName, const std::string &newFileName) noexcept;
    static bool deleteFile(std::string_view filePath) noexcept;
    [[nodiscard]] static std::vector<std::string> getFilesInFolder(std::string_view folder) noexcept;
    [[nodiscard]] static std::vector<std::string> getFoldersInFolder(std::string_view folder) noexcept;
    [[nodiscard]] static std::vector<UString> getLogicalDrives();
    // returns an absolute (i.e. fully-qualified) filesystem path
    [[nodiscard]] static std::string getFolderFromFilePath(std::string_view filepath) noexcept;
    [[nodiscard]] static std::string getFileExtensionFromFilePath(std::string_view filepath) noexcept;
    [[nodiscard]] static std::string getFileNameFromFilePath(std::string_view filePath) noexcept;
    [[nodiscard]] static std::string normalizeDirectory(std::string dirPath) noexcept;
    [[nodiscard]] static bool isAbsolutePath(std::string_view filePath) noexcept;

    // URL-encodes a string, but keeps slashes intact
    [[nodiscard]] static std::string encodeStringToURI(std::string_view unencodedString) noexcept;

    // Fully URL-encodes a string, including slashes
    [[nodiscard]] static std::string urlEncode(std::string_view unencodedString) noexcept;

    // clipboard
    [[nodiscard]] const UString &getClipBoardText();
    void setClipBoardText(const UString &text);

    // dialogs & message boxes
    static void showDialog(const char *title, const char *message,
                           unsigned int flags = 0x00000010u /* SDL_MESSAGEBOX_ERROR */,
                           void /*SDL_Window*/ *modalWindow = nullptr);
    void showMessageInfo(const UString &title, const UString &message) const;
    void showMessageWarning(const UString &title, const UString &message) const;
    void showMessageError(const UString &title, const UString &message) const;
    void showMessageErrorFatal(const UString &title, const UString &message) const;

    using FileDialogCallback = std::function<void(const std::vector<UString> &paths)>;
    void openFileWindow(FileDialogCallback callback, const char *filetypefilters, std::string_view title,
                        std::string_view initialpath = "") const noexcept;
    void openFolderWindow(FileDialogCallback callback, std::string_view initialpath = "") const noexcept;
    void openFileBrowser(std::string_view initialpath) const noexcept;

    // window
    void focus();
    void center();
    void minimize();
    void maximize();
    void enableFullscreen();
    void disableFullscreen();
    void syncWindow();
    void setWindowTitle(const UString &title);
    bool setWindowPos(int x, int y);
    bool setWindowSize(int width, int height);
    void setWindowResizable(bool resizable);
    void setFullscreenWindowedBorderless(bool fullscreenWindowedBorderless);
    void setMonitor(int monitor);
    [[nodiscard]] inline const float &getDisplayRefreshRate() const { return m_fDisplayHz; }
    [[nodiscard]] inline const float &getDisplayRefreshTime() const { return m_fDisplayHzSecs; }
    [[nodiscard]] HWND getHwnd() const;
    [[nodiscard]] vec2 getWindowPos() const;
    [[nodiscard]] vec2 getWindowSize() const;
    [[nodiscard]] int getMonitor() const;
    [[nodiscard]] const std::map<unsigned int, McRect> &getMonitors();
    [[nodiscard]] vec2 getNativeScreenSize() const;
    [[nodiscard]] McRect getDesktopRect() const;
    [[nodiscard]] McRect getWindowRect() const;
    [[nodiscard]] inline bool isFullscreenWindowedBorderless() const { return m_bFullscreenWindowedBorderless; }
    [[nodiscard]] int getDPI() const;
    [[nodiscard]] inline float getDPIScale() const { return (float)getDPI() / 96.0f; }
    [[nodiscard]] inline bool isFullscreen() const { return m_bFullscreen; }
    [[nodiscard]] inline bool isWindowResizable() const { return m_bResizable; }
    [[nodiscard]] inline bool hasFocus() const { return m_bHasFocus; }

    [[nodiscard]] bool isPointValid(vec2 point) const;  // whether an x,y coordinate lands on an actual display

    // mouse
    [[nodiscard]] inline bool isCursorInWindow() const { return m_bIsCursorInsideWindow; }
    [[nodiscard]] inline bool isCursorVisible() const { return m_bCursorVisible; }
    [[nodiscard]] inline bool isCursorClipped() const { return m_bCursorClipped; }
    [[nodiscard]] inline vec2 getMousePos() const { return m_vLastAbsMousePos; }
    [[nodiscard]] inline const McRect &getCursorClip() const { return m_cursorClipRect; }
    [[nodiscard]] inline CURSORTYPE getCursor() const { return m_cursorType; }
    [[nodiscard]] inline bool isOSMouseInputRaw() const { return m_bActualRawInputState; }

    void setCursor(CURSORTYPE cur);
    void setCursorVisible(bool visible);
    void setCursorClip(bool clip, McRect rect);
    void setRawInput(bool raw);  // enable/disable OS-level rawinput

    void setOSMousePos(vec2 pos);
    inline void setMousePos(float x, float y) { setOSMousePos(vec2{x, y}); }

    // keyboard
    UString keyCodeToString(KEYCODE keyCode);
    void listenToTextInput(bool listen);
    bool grabKeyboard(bool grab);

    // debug
    [[nodiscard]] inline bool envDebug() const { return m_bEnvDebug; }

    // platform
    [[nodiscard]] constexpr bool isWine() const { return m_bIsWine; }
    [[nodiscard]] constexpr bool isX11() const { return m_bIsX11; }
    [[nodiscard]] constexpr bool isKMSDRM() const { return m_bIsKMSDRM; }
    [[nodiscard]] constexpr bool isWayland() const { return m_bIsWayland; }

   protected:
    std::unordered_map<std::string, std::optional<std::string>> m_mArgMap;
    std::vector<std::string> m_vCmdLine;
    std::unique_ptr<Engine> m_engine;

    SDL_Window *m_window;
    std::string m_sdldriver;
    static SDL_Environment *s_sdlenv;
    bool m_bUsingDX11;

    bool m_bRunning;
    bool m_bDrawing;
    bool m_bIsRestartScheduled;

    bool m_bMinimized;
    bool m_bHasFocus;
    bool m_bRestoreFullscreen;

    // cache
    mutable UString m_sUsername;
    mutable std::string m_sProgDataPath;
    mutable std::string m_sAppDataPath;
    HWND m_hwnd;

    // logging
    inline bool envDebug(bool enable) {
        m_bEnvDebug = enable;
        return m_bEnvDebug;
    }
    void onLogLevelChange(float newval);
    bool m_bEnvDebug;

    // monitors
    void initMonitors(bool force = false) const;
    // mutable due to lazy init
    mutable std::map<unsigned int, McRect> m_mMonitors;
    mutable McRect m_fullDesktopBoundingBox;

    float m_fDisplayHz;
    float m_fDisplayHzSecs;

    // window
    bool m_bDPIOverride;
    bool m_bResizable;
    bool m_bFullscreen;
    bool m_bFullscreenWindowedBorderless;
    inline void onFullscreenWindowBorderlessChange(float newValue) {
        setFullscreenWindowedBorderless(!!static_cast<int>(newValue));
    }
    inline void onMonitorChange(float oldValue, float newValue) {
        if(oldValue != newValue) setMonitor(static_cast<int>(newValue));
    }

    // save the last position obtained from SDL so that we can return something sensible if the SDL API fails
    mutable vec2 m_vLastKnownWindowSize{0.f};
    mutable vec2 m_vLastKnownWindowPos{0.f};

    // mouse
    friend class Mouse;
    // <rel, abs>
    std::pair<vec2, vec2> consumeMousePositionCache();
    // allow Mouse to update the cached environment position post-sensitivity/clipping
    // the difference between setMousePos and this is that it doesn't actually warp the OS cursor
    inline void updateCachedMousePos(const vec2 &pos) { m_vLastAbsMousePos = pos; }

    vec2 m_vLastAbsMousePos{0.f};
    bool m_bIsCursorInsideWindow;
    bool m_bCursorClipped;
    bool m_bCursorVisible;
    bool m_bActualRawInputState;
    McRect m_cursorClipRect;
    CURSORTYPE m_cursorType;
    std::map<CURSORTYPE, SDL_Cursor *> m_mCursorIcons;

    // clipboard
    UString m_sCurrClipboardText;

    // misc platform
    bool m_bIsWine{false};
    bool m_bIsX11;
    bool m_bIsKMSDRM;
    bool m_bIsWayland;

   private:
    // lazy inits
    void initCursors();

    // static callbacks/helpers
    static void sdlFileDialogCallback(void *userdata, const char *const *filelist, int filter) noexcept;

    // for getting files in folder/ folders in folder
    static std::vector<std::string> enumerateDirectory(std::string_view pathToEnum,
                                                       /* enum SDL_PathType */ unsigned int type) noexcept;
    static std::string getThingFromPathHelper(
        std::string_view path,
        bool folder) noexcept;  // code sharing for getFolderFromFilePath/getFileNameFromFilePath

    // internal path conversion helper, SDL_URLOpen needs a URL-encoded URI on Unix (because it goes to xdg-open)
    [[nodiscard]] static std::string filesystemPathToURI(const std::filesystem::path &path) noexcept;

    static SDL_Rect McRectToSDLRect(const McRect &mcrect) noexcept;
    static McRect SDLRectToMcRect(const SDL_Rect &sdlrect) noexcept;

    static bool sdl_x11eventhook(void *thisptr, XEvent *xev);
};

#endif
