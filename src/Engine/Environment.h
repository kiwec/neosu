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
   public:
    struct Interop {
        void handle_osz(const char *path);
        void handle_cmdline_args(const std::vector<UString> &args);
        void handle_cmdline_args() { env ? handle_cmdline_args(env->getCommandLine()) : (void)0; }
        void register_file_associations();
        static void handle_existing_window([[maybe_unused]] int argc,
                                           [[maybe_unused]] char *argv[]);  // this is likely broken ATM
    };

   private:
    Interop m_interop{};

   public:
    Environment(int argc, char *argv[]);
    virtual ~Environment();

    void update();

    // engine/factory
    Graphics *createRenderer();

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
    static std::string getEnvVariable(const std::string &varToQuery) noexcept;
    // i.e. setenv()
    static bool setEnvVariable(const std::string &varToSet, const std::string &varValue,
                               bool overwrite = true) noexcept;
    // i.e. unsetenv()
    static bool unsetEnvVariable(const std::string &varToUnset) noexcept;

    static const std::string &getExeFolder();
    static void setProcessPriority(float newVal);  // != 0.0 : high

    static void openURLInDefaultBrowser(const std::string &url) noexcept;

    [[nodiscard]] inline const std::unordered_map<UString, std::optional<UString>> &getLaunchArgs() const {
        return m_mArgMap;
    }
    [[nodiscard]] inline const std::vector<UString> &getCommandLine() const { return m_vCmdLine; }

    // returns at least 1
    static int getLogicalCPUCount();

    // user
    [[nodiscard]] const UString &getUsername();
    [[nodiscard]] const std::string &getUserDataPath();
    [[nodiscard]] const std::string &getLocalDataPath();

    // file IO
    [[nodiscard]] static bool fileExists(std::string &filename);  // passthroughs to McFile
    [[nodiscard]] static bool directoryExists(std::string &directoryName);
    [[nodiscard]] static bool fileExists(const std::string &filename);
    [[nodiscard]] static bool directoryExists(const std::string &directoryName);
    [[nodiscard]] static inline bool isaTTY() { return s_bIsATTY; }  // is stdout a terminal

    static bool createDirectory(const std::string &directoryName);
    static bool renameFile(const std::string &oldFileName, const std::string &newFileName);
    static bool deleteFile(const std::string &filePath);
    [[nodiscard]] static std::vector<std::string> getFilesInFolder(const std::string &folder) noexcept;
    [[nodiscard]] static std::vector<std::string> getFoldersInFolder(const std::string &folder) noexcept;
    [[nodiscard]] static std::vector<UString> getLogicalDrives();
    // returns an absolute (i.e. fully-qualified) filesystem path
    [[nodiscard]] static std::string getFolderFromFilePath(const std::string &filepath) noexcept;
    [[nodiscard]] static std::string getFileExtensionFromFilePath(const std::string &filepath,
                                                                  bool includeDot = false) noexcept;
    [[nodiscard]] static std::string getFileNameFromFilePath(const std::string &filePath) noexcept;
    [[nodiscard]] static std::string normalizeDirectory(std::string dirPath) noexcept;
    [[nodiscard]] static bool isAbsolutePath(const std::string &filePath) noexcept;

    // URL-encodes a string, but keeps slashes intact
    [[nodiscard]] static std::string encodeStringToURI(const std::string &unencodedString) noexcept;

    // Fully URL-encodes a string, including slashes
    [[nodiscard]] static std::string urlEncode(const std::string &unencodedString) noexcept;

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
    void openFileWindow(FileDialogCallback callback, const char *filetypefilters, const UString &title,
                        const UString &initialpath = "") const;
    void openFolderWindow(FileDialogCallback callback, const UString &initialpath = "") const;
    void openFileBrowser(const std::string &initialpath) noexcept;

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
    [[nodiscard]] bool isFullscreenWindowedBorderless() const { return m_bFullscreenWindowedBorderless; }
    [[nodiscard]] int getDPI() const;
    [[nodiscard]] float getDPIScale() const { return (float)getDPI() / 96.0f; }
    [[nodiscard]] inline const bool &isFullscreen() const { return m_bFullscreen; }
    [[nodiscard]] inline const bool &isWindowResizable() const { return m_bResizable; }
    [[nodiscard]] inline bool hasFocus() const { return m_bHasFocus; }

    [[nodiscard]] bool isPointValid(vec2 point) const;  // whether an x,y coordinate lands on an actual display

    // mouse
    [[nodiscard]] inline const bool &isCursorInWindow() const { return m_bIsCursorInsideWindow; }
    [[nodiscard]] inline const bool &isCursorVisible() const { return m_bCursorVisible; }
    [[nodiscard]] inline const bool &isCursorClipped() const { return m_bCursorClipped; }
    [[nodiscard]] inline const vec2 &getMousePos() const { return m_vLastAbsMousePos; }
    [[nodiscard]] inline const McRect &getCursorClip() const { return m_cursorClipRect; }
    [[nodiscard]] inline const CURSORTYPE &getCursor() const { return m_cursorType; }
    [[nodiscard]] inline const bool &isOSMouseInputRaw() const { return m_bActualRawInputState; }

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
    [[nodiscard]] inline bool isWine() const { return s_bIsWine; }

#ifdef _DEBUG
    [[nodiscard]] inline bool isDebugBuild() const { return true; }
#else
    [[nodiscard]] inline bool isDebugBuild() const { return false; }
#endif

   protected:
    std::unordered_map<UString, std::optional<UString>> m_mArgMap;
    std::vector<UString> m_vCmdLine;
    std::unique_ptr<Engine> m_engine;

    SDL_Window *m_window;
    std::string m_sdldriver;
    bool m_bIsX11;
    static SDL_Environment *s_sdlenv;

    bool m_bRunning;
    bool m_bDrawing;
    bool m_bIsRestartScheduled;

    bool m_bMinimized;  // for fps_max_background
    bool m_bHasFocus;   // for fps_max_background

    // cache
    UString m_sUsername;
    std::string m_sProgDataPath;
    std::string m_sAppDataPath;
    HWND m_hwnd;

    // logging
    inline bool envDebug(bool enable) {
        m_bEnvDebug = enable;
        return m_bEnvDebug;
    }
    void onLogLevelChange(float newval);
    bool m_bEnvDebug;

    static bool s_bIsATTY;  // to determine whether we should log with colours to stdout/stderr

    // monitors
    void initMonitors(bool force = false) const;
    // mutable due to lazy init
    mutable std::map<unsigned int, McRect> m_mMonitors;
    mutable McRect m_fullDesktopBoundingBox;

    float m_fDisplayHz;
    float m_fDisplayHzSecs;

    // window
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

    // misc
    void initCursors();
    static bool s_bIsWine;

   private:
    // static callbacks/helpers
    struct FileDialogCallbackData {
        FileDialogCallback callback;
    };
    static void sdlFileDialogCallback(void *userdata, const char *const *filelist, int filter);

    // for getting files in folder/ folders in folder
    static std::vector<std::string> enumerateDirectory(const std::string &pathToEnum,
                                                       /* enum SDL_PathType */ unsigned int type) noexcept;
    static std::string getThingFromPathHelper(
        const std::string &path,
        bool folder) noexcept;  // code sharing for getFolderFromFilePath/getFileNameFromFilePath

    // internal path conversion helper, SDL_URLOpen needs a URL-encoded URI on Unix (because it goes to xdg-open)
    [[nodiscard]] static std::string filesystemPathToURI(const std::filesystem::path &path) noexcept;

    static SDL_Rect McRectToSDLRect(const McRect &mcrect) noexcept;
    static McRect SDLRectToMcRect(const SDL_Rect &sdlrect) noexcept;

    static bool sdl_x11eventhook(void *thisptr, XEvent *xev);
};

#endif
