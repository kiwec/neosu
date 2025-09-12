// Copyright (c) 2018, PG & 2025, WH, All rights reserved.

#include "Environment.h"

#include "Engine.h"
#include "Mouse.h"
#include "File.h"
#include "SString.h"

// #include "DirectX11Interface.h" TODO
#include "SDLGLInterface.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>
#include <string>
#include <sstream>
#include <iomanip>

#if defined(MCENGINE_PLATFORM_WINDOWS)
#include "WinDebloatDefs.h"
#include <io.h>
#include <lmcons.h>
#include <windows.h>
#elif defined(__APPLE__) || defined(MCENGINE_PLATFORM_LINUX)
#include <pwd.h>
#include <unistd.h>
#ifdef MCENGINE_PLATFORM_LINUX
#include <X11/Xlib.h>
#endif
#elif defined(__EMSCRIPTEN__)
// TODO
#endif

#include <curl/curl.h>
#include <SDL3/SDL.h>

Environment *env = nullptr;
bool Environment::s_bIsWine = false;

SDL_Environment *Environment::s_sdlenv = SDL_GetEnvironment();
bool Environment::s_bIsATTY = (isatty(fileno(stdout)) != 0);

Environment::Environment(int argc, char *argv[]) : m_interop(this) {
    env = this;

    // parse args
    m_mArgMap = [&]() -> std::unordered_map<std::string, std::optional<std::string>> {
        // example usages:
        // args.contains("-file")
        // auto filename = args["-file"].value_or("default.txt");
        // if (args["-output"].has_value())
        // 	auto outfile = args["-output"].value();
        std::unordered_map<std::string, std::optional<std::string>> args;
        for(int i = 1; i < argc; ++i) {
            std::string_view arg{argv[i]};
            if(arg.starts_with('-'))
                if(i + 1 < argc && !(argv[i + 1][0] == '-')) {
                    args[std::string(arg)] = argv[i + 1];
                    ++i;
                } else
                    args[std::string(arg)] = std::nullopt;
            else
                args[std::string(arg)] = std::nullopt;
        }
        return args;
    }();

    // simple vector representation of the whole cmdline including the program name (as the first element)
    m_vCmdLine = std::vector<std::string>(argv, argv + argc);

#ifdef MCENGINE_PLATFORM_WINDOWS
    s_bIsWine = !!GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "wine_get_version");
#endif

    m_engine = nullptr;  // will be initialized by the mainloop once setup is complete
    m_window = nullptr;

    m_bRunning = true;
    m_bDrawing = false;
    m_bIsRestartScheduled = false;

    m_bMinimized = false;  // for fps_max_background
    m_bHasFocus = true;    // for fps_max_background
    m_bFullscreenWindowedBorderless = false;

    m_fDisplayHz = 360.0f;
    m_fDisplayHzSecs = 1.0f / m_fDisplayHz;

    m_bEnvDebug = false;

    m_bResizable = false;  // window is created non-resizable
    m_bFullscreen = false;

    m_sUsername = {};
    m_sProgDataPath = {};  // local data for McEngine files
    m_sAppDataPath = {};
    m_hwnd = nullptr;

    m_bIsCursorInsideWindow = true;
    m_bActualRawInputState = false;
    m_bCursorClipped = false;
    m_bCursorVisible = false;
    m_cursorType = CURSORTYPE::CURSOR_NORMAL;

    // lazy init
    m_mCursorIcons = {};

    m_vLastAbsMousePos = vec2{};

    m_sCurrClipboardText = {};
    // lazy init
    m_mMonitors = {};
    // lazy init (with initMonitors)
    m_fullDesktopBoundingBox = McRect{};

    m_vLastKnownWindowPos = vec2{};
    m_vLastKnownWindowSize = vec2{320, 240};

    m_sdldriver = SDL_GetCurrentVideoDriver();
    m_bIsX11 = (m_sdldriver == "x11");

    if(m_bIsX11) {
        // workaround alt tab bug (cursor getting stuck confined for 5 seconds)
        SDL_SetX11EventHook(Environment::sdl_x11eventhook, this);
    }

    // setup callbacks
    cv::debug_env.setCallback(SA::MakeDelegate<&Environment::onLogLevelChange>(this));
    cv::fullscreen_windowed_borderless.setCallback(
        SA::MakeDelegate<&Environment::onFullscreenWindowBorderlessChange>(this));
    cv::monitor.setCallback(SA::MakeDelegate<&Environment::onMonitorChange>(this));

    // set high priority right away
    Environment::setProcessPriority(cv::win_processpriority.getFloat());
}

Environment::~Environment() {
    for(auto &[_, sdl_cur] : m_mCursorIcons) {
        SDL_DestroyCursor(sdl_cur);
    }
    env = nullptr;
}

// well this doesn't do much atm... called at the end of engine->onUpdate
void Environment::update() {
    m_bIsCursorInsideWindow = m_bHasFocus && m_engine->getScreenRect().contains(getMousePos());
}

Graphics *Environment::createRenderer() {
#ifndef MCENGINE_FEATURE_DIRECTX11
    // need to load stuff dynamically before the base class constructors
    SDLGLInterface::load();
    return new SDLGLInterface(m_window);
#else
    return new DirectX11Interface(Env::cfg(OS::WINDOWS) ? getHwnd() : reinterpret_cast<HWND>(m_window));
#endif
}

void Environment::shutdown() {
    setRawInput(false);

    SDL_Event event;
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

void Environment::restart() {
    m_bIsRestartScheduled = true;
    shutdown();
}

const std::string &Environment::getExeFolder() {
    static std::string pathStr{};
    if(!pathStr.empty()) return pathStr;
    // sdl caches this internally, but we'll cache a std::string representation of it
    const char *path = SDL_GetBasePath();
    if(path) {
        pathStr = path;
    } else {
        pathStr = "./";
    }
    return pathStr;
}

void Environment::openURLInDefaultBrowser(const std::string &url) noexcept {
    if(!SDL_OpenURL(url.c_str())) {
        debugLog("Failed to open URL: {:s}\n", SDL_GetError());
    }
}

// returns at least 1
int Environment::getLogicalCPUCount() { return SDL_GetNumLogicalCPUCores(); }

const UString &Environment::getUsername() {
    if(!m_sUsername.isEmpty()) return m_sUsername;
#if defined(MCENGINE_PLATFORM_WINDOWS)
    DWORD username_len = UNLEN + 1;
    wchar_t username[UNLEN + 1];

    if(GetUserNameW(username, &username_len)) m_sUsername = username;
#elif defined(__APPLE__) || defined(MCENGINE_PLATFORM_LINUX) || defined(MCENGINE_PLATFORM_WASM)
    const char *user = getenv("USER");
    if(user != nullptr) m_sUsername = {user};
#ifndef MCENGINE_PLATFORM_WASM
    else {
        struct passwd *pwd = getpwuid(getuid());
        if(pwd != nullptr) m_sUsername = {pwd->pw_name};
    }
#endif
#endif
    // fallback
    if(m_sUsername.isEmpty()) m_sUsername = {PACKAGE_NAME "-user"};
    return m_sUsername;
}

// i.e. toplevel appdata path
const std::string &Environment::getUserDataPath() {
    if(!m_sAppDataPath.empty()) return m_sAppDataPath;

    m_sAppDataPath = ".";  // set it to non-empty to avoid endlessly failing if SDL_GetPrefPath fails once

    char *path = SDL_GetPrefPath("", "");
    if(path != nullptr) {
        m_sAppDataPath = path;
        // since this is kind of an abuse of SDL_GetPrefPath, we remove the extra slashes at the end
        while(!m_sAppDataPath.empty() && (m_sAppDataPath.ends_with('\\') || m_sAppDataPath.ends_with('/'))) {
            m_sAppDataPath.pop_back();
        }
        m_sAppDataPath.append(PREF_PATHSEP);
        SDL_free(path);
    }

    return m_sAppDataPath;
}

// i.e. ~/.local/share/PACKAGE_NAME
const std::string &Environment::getLocalDataPath() {
    if(!m_sProgDataPath.empty()) return m_sProgDataPath;

    char *path = SDL_GetPrefPath("McEngine", PACKAGE_NAME);
    if(path != nullptr) m_sProgDataPath = path;

    SDL_free(path);

    if(m_sProgDataPath.empty())  // fallback to exe dir
        m_sProgDataPath = getExeFolder();

    return m_sProgDataPath;
}

// modifies the input filename! (checks case insensitively past the last slash)
bool Environment::fileExists(std::string &filename) {
    return File::existsCaseInsensitive(filename) == File::FILETYPE::FILE;
}

// modifies the input directoryName! (checks case insensitively past the last slash)
bool Environment::directoryExists(std::string &directoryName) {
    return File::existsCaseInsensitive(directoryName) == File::FILETYPE::FOLDER;
}

// same as the above, but for string literals (so we can't check insensitively and modify the input)
bool Environment::fileExists(const std::string &filename) { return File::exists(filename) == File::FILETYPE::FILE; }

bool Environment::directoryExists(const std::string &directoryName) {
    return File::exists(directoryName) == File::FILETYPE::FOLDER;
}

bool Environment::createDirectory(const std::string &directoryName) {
    return SDL_CreateDirectory(directoryName.c_str());  // returns true if it already exists
}

bool Environment::renameFile(const std::string &oldFileName, const std::string &newFileName) {
    if(oldFileName == newFileName) {
        return true;
    }
    if(!SDL_RenamePath(oldFileName.c_str(), newFileName.c_str())) {
        std::string tempFile{newFileName + ".tmp"};
        if(!SDL_CopyFile(oldFileName.c_str(), tempFile.c_str())) {
            return false;
        }
        if(!SDL_RenamePath(tempFile.c_str(), newFileName.c_str())) {
            return false;
        }
        // return true if we were able to copy the path (and the file exists in the end), even if removing the old file
        // didn't work
        SDL_RemovePath(oldFileName.c_str());
    }
    return Environment::fileExists(newFileName);
}

bool Environment::deleteFile(const std::string &filePath) { return SDL_RemovePath(filePath.c_str()); }

std::vector<std::string> Environment::getFilesInFolder(const std::string &folder) noexcept {
    return enumerateDirectory(folder, SDL_PATHTYPE_FILE);
}

std::vector<std::string> Environment::getFoldersInFolder(const std::string &folder) noexcept {
    return enumerateDirectory(folder, SDL_PATHTYPE_DIRECTORY);
}

std::string Environment::getFileNameFromFilePath(const std::string &filepath) noexcept {
    return getThingFromPathHelper(filepath, false);
}

std::string Environment::normalizeDirectory(std::string dirPath) noexcept {
    SString::trim(&dirPath);
    if(dirPath.empty()) return dirPath;

    // remove drive letter prefix if switching to linux
    if constexpr(!Env::cfg(OS::WINDOWS)) {
        if(dirPath.find(':') == 1) {
            dirPath.erase(0, 2);
        }
    }

    while(dirPath.ends_with("\\") || dirPath.ends_with("/")) {
        dirPath.pop_back();
    }
    dirPath.append(PREF_PATHSEP);

    // use std::filesystem lexically_normal to clean up the path (it doesn't make sure it exists, purely transforms it)
    std::filesystem::path fspath{dirPath};
    dirPath = fspath.lexically_normal().generic_string();

    if(dirPath == "." PREF_PATHSEP) {
        return "";
    } else {
        return dirPath;
    }
}

bool Environment::isAbsolutePath(const std::string &filePath) noexcept {
    bool is_absolute_path = filePath.starts_with('/');

    if constexpr(Env::cfg(OS::WINDOWS)) {
        // On Wine, linux paths are also valid, hence the OR
        is_absolute_path |= ((filePath.find(':') == 1) || (filePath.starts_with(R"(\\?\)")));
    }

    return is_absolute_path;
}

std::string Environment::getFolderFromFilePath(const std::string &filepath) noexcept {
    return getThingFromPathHelper(filepath, true);
}

std::string Environment::getFileExtensionFromFilePath(const std::string &filepath, bool /*includeDot*/) noexcept {
    const auto extIdx = filepath.find_last_of('.');
    if(extIdx != std::string::npos) {
        return filepath.substr(extIdx + 1);
    } else
        return {""};
}

// sadly, sdl doesn't give a way to do this
std::vector<UString> Environment::getLogicalDrives() {
    std::vector<UString> drives{};

    if constexpr(Env::cfg(OS::LINUX)) {
        drives.emplace_back("/");
    } else if constexpr(Env::cfg(OS::WINDOWS)) {
#if defined(MCENGINE_PLATFORM_WINDOWS)
        DWORD dwDrives = GetLogicalDrives();
        for(int i = 0; i < 26; i++)  // A-Z
        {
            if(dwDrives & (1 << i)) {
                char driveLetter = 'A' + i;
                UString drivePath = UString::format("%c:/", driveLetter);

                SDL_PathInfo info;
                UString testPath = UString::format("%c:\\", driveLetter);

                if(SDL_GetPathInfo(testPath.toUtf8(), &info)) drives.emplace_back(drivePath);
            }
        }
#endif
    } else if constexpr(Env::cfg(OS::WASM)) {
        // TODO: VFS
        drives.emplace_back("/");
    }

    return drives;
}

// cached on startup from main.cpp with argv[0] passed, after that argv0 can be null
const std::string &Environment::getPathToSelf(const char *argv0) {
    static std::string pathStr{};
    if(!pathStr.empty()) return pathStr;
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::path exe_path{};

    if constexpr(Env::cfg(OS::LINUX))
        exe_path = fs::canonical("/proc/self/exe", ec);
    else {
        UString uPath{argv0};
        exe_path = fs::canonical(fs::path(uPath.plat_str()), ec);
    }

    if(!ec && !exe_path.empty())  // canonical path found
    {
        UString uPath{exe_path.string().c_str()};
        pathStr = uPath.toUtf8();
    } else {
#if defined(MCENGINE_PLATFORM_WINDOWS)  // fallback to GetModuleFileNameW
        std::array<wchar_t, MAX_PATH> buf;
        GetModuleFileNameW(nullptr, buf.data(), MAX_PATH);

        auto size = WideCharToMultiByte(CP_UTF8, 0, buf.data(), buf.size(), NULL, 0, NULL, NULL);
        std::string utf8path(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, buf.data(), size, (LPSTR)utf8path.c_str(), size, NULL, NULL);
        pathStr = utf8path;
#else
#ifndef MCENGINE_PLATFORM_LINUX
        printf("WARNING: unsupported platform for " __FUNCTION__ "\n");
#endif
        std::string sp;
        std::ifstream("/proc/self/comm") >> sp;
        if(!sp.empty()) {  // fallback to data dir + self
            pathStr = MCENGINE_DATA_DIR + sp;
        } else {  // fallback to data dir + package name
            pathStr = std::string{MCENGINE_DATA_DIR} + PACKAGE_NAME;
        }
#endif
    }
    return pathStr;
}

std::string Environment::getEnvVariable(const std::string &varToQuery) noexcept {
    const char *varVal = nullptr;
    if(s_sdlenv && !varToQuery.empty()) {
        varVal = SDL_GetEnvironmentVariable(s_sdlenv, varToQuery.c_str());
        if(varVal) {
            return std::string{varVal};
        }
    }
    return {""};
}

bool Environment::setEnvVariable(const std::string &varToSet, const std::string &varValue, bool overwrite) noexcept {
    if(s_sdlenv && !varToSet.empty()) {
        return SDL_SetEnvironmentVariable(s_sdlenv, varToSet.c_str(), varValue.empty() ? "" : varValue.c_str(),
                                          overwrite);
    }
    return false;
}

bool Environment::unsetEnvVariable(const std::string &varToUnset) noexcept {
    if(s_sdlenv && !varToUnset.empty()) {
        return SDL_UnsetEnvironmentVariable(s_sdlenv, varToUnset.c_str());
    }
    return false;
}

std::string Environment::encodeStringToURI(const std::string &unencodedString) noexcept {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for(const char c : unencodedString) {
        // keep alphanumerics and other accepted characters intact
        if(std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            escaped << c;
        } else {
            // any other characters are percent-encoded
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

std::string Environment::urlEncode(const std::string &unencodedString) noexcept {
    CURL* curl = curl_easy_init();
    if(!curl) {
        return "";
    }

    char* encoded = curl_easy_escape(curl, unencodedString.c_str(), unencodedString.length());
    if(!encoded) {
        curl_easy_cleanup(curl);
        return "";
    }

    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

std::string Environment::filesystemPathToURI(const std::filesystem::path &path) noexcept {
    namespace fs = std::filesystem;
    // convert to absolute path and normalize
    auto abs_path = fs::absolute(path);
    // convert to path with forward slashes
    const UString path_str = UString{abs_path.generic_string()};
    // URI encode the path
    std::string uri = encodeStringToURI(path_str.toUtf8());

    // prepend with file:///
    if(uri[0] == '/')
        uri = fmt::format("file://{}", uri);
    else
        uri = fmt::format("file:///{}", uri);

    // add trailing slash if it's a directory
    if(fs::is_directory(abs_path) && !uri.ends_with('/')) {
        uri += '/';
    }
    return uri;
}

const UString &Environment::getClipBoardText() {
    char *newClip = SDL_GetClipboardText();
    if(newClip) m_sCurrClipboardText = newClip;

    SDL_free(newClip);

    return m_sCurrClipboardText;
}

void Environment::setClipBoardText(const UString &text) {
    m_sCurrClipboardText = text;
    SDL_SetClipboardText(text.toUtf8());
}

// static helper for class methods below (defaults to flags = error, modalWindow = null)
void Environment::showDialog(const char *title, const char *message, unsigned int flags, void *modalWindow) {
    auto *actualWin{static_cast<SDL_Window *>(modalWindow)};

    // message does not show up for hidden windows
    if(actualWin && ((SDL_GetWindowFlags(actualWin) & SDL_WINDOW_HIDDEN) == SDL_WINDOW_HIDDEN)) {
        actualWin = nullptr;
    }

    SDL_ShowSimpleMessageBox(flags, title, message, actualWin);
}

void Environment::showMessageInfo(const UString &title, const UString &message) const {
    showDialog(title.toUtf8(), message.toUtf8(), SDL_MESSAGEBOX_INFORMATION, m_window);
}

void Environment::showMessageWarning(const UString &title, const UString &message) const {
    showDialog(title.toUtf8(), message.toUtf8(), SDL_MESSAGEBOX_WARNING, m_window);
}

void Environment::showMessageError(const UString &title, const UString &message) const {
    showDialog(title.toUtf8(), message.toUtf8(), SDL_MESSAGEBOX_ERROR, m_window);
}

// what is the point of this exactly?
void Environment::showMessageErrorFatal(const UString &title, const UString &message) const {
    showMessageError(title, message);
}

void Environment::openFileWindow(FileDialogCallback callback, const char *filetypefilters, const UString & /*title*/,
                                 const UString &initialpath) const {
    // convert filetypefilters (Windows-style)
    std::vector<std::string> filterNames;
    std::vector<std::string> filterPatterns;
    std::vector<SDL_DialogFileFilter> sdlFilters;

    if(filetypefilters && *filetypefilters) {
        const char *curr = filetypefilters;
        // add the filetype filters to the SDL dialog filter
        while(*curr) {
            const char *name = curr;
            curr += strlen(name) + 1;

            if(!*curr) break;

            const char *pattern = curr;
            curr += strlen(pattern) + 1;

            filterNames.emplace_back(name);
            filterPatterns.emplace_back(pattern);

            SDL_DialogFileFilter filter = {filterNames.back().c_str(), filterPatterns.back().c_str()};
            sdlFilters.push_back(filter);
        }
    }

    // callback data to be passed to SDL
    auto *callbackData = new FileDialogCallbackData{std::move(callback)};

    // show it
    SDL_ShowOpenFileDialog(sdlFileDialogCallback, callbackData, m_window,
                           sdlFilters.empty() ? nullptr : sdlFilters.data(), static_cast<int>(sdlFilters.size()),
                           initialpath.length() > 0 ? initialpath.toUtf8() : nullptr, false);
}

void Environment::openFolderWindow(FileDialogCallback callback, const UString &initialpath) const {
    // callback data to be passed to SDL
    auto *callbackData = new FileDialogCallbackData{std::move(callback)};

    // show it
    SDL_ShowOpenFolderDialog(sdlFileDialogCallback, callbackData, m_window,
                             initialpath.length() > 0 ? initialpath.toUtf8() : nullptr, false);
}

// just open the file manager in a certain folder, but not do anything with it
void Environment::openFileBrowser(const std::string &initialpath) noexcept {
    std::string pathToOpen{initialpath};
    if(pathToOpen.empty())
        pathToOpen = getExeFolder();
    else
        // XXX: On windows you can also open a folder while having a file selected
        //      Would be useful for screenshots, for example
        pathToOpen = getFolderFromFilePath(pathToOpen);

    namespace fs = std::filesystem;
    std::string encodedPath =
        Env::cfg(OS::WINDOWS) ? fmt::format("file:///{}", pathToOpen) : filesystemPathToURI(fs::path{pathToOpen});

    if(!SDL_OpenURL(encodedPath.c_str())) debugLog("Failed to open file URI {:s}: {:s}\n", encodedPath, SDL_GetError());
}

void Environment::focus() {
    if(!SDL_RaiseWindow(m_window)) {
        debugLog("Failed to focus window: {:s}\n", SDL_GetError());
        return;
    }
    m_bHasFocus = true;
}

void Environment::center() {
    syncWindow();
    const SDL_DisplayID di = SDL_GetDisplayForWindow(m_window);
    if(!di) {
        debugLog("Failed to obtain SDL_DisplayID for window: {:s}\n", SDL_GetError());
        return;
    }
    setWindowPos(SDL_WINDOWPOS_CENTERED_DISPLAY(di), SDL_WINDOWPOS_CENTERED_DISPLAY(di));
}

void Environment::minimize() {
    if(!SDL_MinimizeWindow(m_window)) {
        debugLog("Failed to minimize window: {:s}\n", SDL_GetError());
        return;
    }
    m_bHasFocus = false;
}

void Environment::maximize() {
    if(!SDL_MaximizeWindow(m_window)) {
        debugLog("Failed to maximize window: {:s}\n", SDL_GetError());
        return;
    }
    m_bHasFocus = true;
}

// TODO: implement exclusive fullscreen for dx11 backend
void Environment::enableFullscreen() {
    // NOTE: "fake" fullscreen since we don't want a videomode change
    // XXX: (doesn't this make fullscreen_windowed_borderless irrelevant?)
    if(!SDL_SetWindowFullscreen(m_window, true)) {
        debugLog("Failed to enable fullscreen: {:s}\n", SDL_GetError());
        return;
    }

    m_bFullscreen = true;
    cv::fullscreen.setValue(m_bFullscreen, false);

    syncWindow();

    // TODO: fix/remove the need to have these here
    if(osu) {
        auto res = cv::resolution.getString().c_str();
        osu->onInternalResolutionChanged(res, res);
    }
}

void Environment::disableFullscreen() {
    if(!SDL_SetWindowFullscreen(m_window, false)) {
        debugLog("Failed to disable fullscreen: {:s}\n", SDL_GetError());
        return;
    }

    m_bFullscreen = false;
    cv::fullscreen.setValue(m_bFullscreen, false);

    syncWindow();

    if(osu) {
        auto res = cv::windowed_resolution.getString().c_str();
        osu->onWindowedResolutionChanged(res, res);
    }
}

void Environment::setFullscreenWindowedBorderless(bool fullscreenWindowedBorderless) {
    m_bFullscreenWindowedBorderless = fullscreenWindowedBorderless;

    if(isFullscreen()) {
        disableFullscreen();
        enableFullscreen();
    }
}

void Environment::setWindowTitle(const UString &title) { SDL_SetWindowTitle(m_window, title.toUtf8()); }

void Environment::syncWindow() {
    if(m_window) SDL_SyncWindow(m_window);
}

bool Environment::setWindowPos(int x, int y) { return SDL_SetWindowPosition(m_window, x, y); }

bool Environment::setWindowSize(int width, int height) { return SDL_SetWindowSize(m_window, width, height); }

// NOTE: the SDL header states:
// "You can't change the resizable state of a fullscreen window."
void Environment::setWindowResizable(bool resizable) {
    if(!SDL_SetWindowResizable(m_window, resizable)) {
        debugLog("Failed to set window {:s} (currently {:s}): {:s}\n", resizable ? "resizable" : "non-resizable",
                 m_bResizable ? "resizable" : "non-resizable", SDL_GetError());
        return;
    }
    m_bResizable = resizable;
}

void Environment::setMonitor(int monitor) {
    if(monitor == 0 || monitor == getMonitor()) return center();

    bool success = false;

    if(!m_mMonitors.contains(monitor))  // try force reinit to check for new monitors
        initMonitors(true);
    if(m_mMonitors.contains(monitor)) {
        // SDL: "If the window is in an exclusive fullscreen or maximized state, this request has no effect."
        if(m_bFullscreen || m_bFullscreenWindowedBorderless) {
            disableFullscreen();
            syncWindow();
            success = setWindowPos(SDL_WINDOWPOS_CENTERED_DISPLAY(monitor), SDL_WINDOWPOS_CENTERED_DISPLAY(monitor));
            syncWindow();
            enableFullscreen();
        } else
            success = setWindowPos(SDL_WINDOWPOS_CENTERED_DISPLAY(monitor), SDL_WINDOWPOS_CENTERED_DISPLAY(monitor));

        if(!success)
            debugLog("WARNING: failed to setMonitor({:d}), centering instead. SDL error: {:s}\n", monitor,
                     SDL_GetError());
        else if(!(success = (monitor == getMonitor())))
            debugLog("WARNING: setMonitor({:d}) didn't actually change the monitor, centering instead.\n", monitor);
    } else
        debugLog("WARNING: tried to setMonitor({:d}) to invalid monitor, centering instead\n", monitor);

    if(!success) center();

    cv::monitor.setValue(getMonitor(), false);
}

HWND Environment::getHwnd() const {
    HWND hwnd = nullptr;
#if defined(MCENGINE_PLATFORM_WINDOWS)
    hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if(!hwnd) debugLog("(Windows) hwnd is null! SDL: {:s}\n", SDL_GetError());
#elif defined(__APPLE__)
    NSWindow *nswindow = (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(m_window),
                                                                     SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if(nswindow) {
#warning "getHwnd() TODO"
    }
#elif defined(MCENGINE_PLATFORM_LINUX)
    if(SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        auto *xdisplay = (Display *)SDL_GetPointerProperty(SDL_GetWindowProperties(m_window),
                                                           SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        auto xwindow =
            (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if(xdisplay && xwindow)
            hwnd = (HWND)xwindow;
        else
            debugLog("(X11) no display/no surface! SDL: {:s}\n", SDL_GetError());
    } else if(SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        struct wl_display *display = (struct wl_display *)SDL_GetPointerProperty(
            SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        struct wl_surface *surface = (struct wl_surface *)SDL_GetPointerProperty(
            SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        if(display && surface)
            hwnd = (HWND)surface;
        else
            debugLog("(Wayland) no display/no surface! SDL: {:s}\n", SDL_GetError());
    }
#endif

    return hwnd;
}

vec2 Environment::getWindowPos() const {
    int x{0}, y{0};
    if(!SDL_GetWindowPosition(m_window, &x, &y)) {
        debugLog("Failed to get window position (returning cached {},{}): {:s}\n", m_vLastKnownWindowPos.x,
                 m_vLastKnownWindowPos.y, SDL_GetError());
    } else {
        m_vLastKnownWindowPos = vec2{static_cast<float>(x), static_cast<float>(y)};
    }
    return m_vLastKnownWindowPos;
}

vec2 Environment::getWindowSize() const {
    int width{320}, height{240};
    if(!SDL_GetWindowSize(m_window, &width, &height)) {
        debugLog("Failed to get window size (returning cached {},{}): {:s}\n", m_vLastKnownWindowSize.x,
                 m_vLastKnownWindowSize.y, SDL_GetError());
    } else {
        m_vLastKnownWindowSize = vec2{static_cast<float>(width), static_cast<float>(height)};
    }
    return m_vLastKnownWindowSize;
}

const std::map<unsigned int, McRect> &Environment::getMonitors() {
    if(m_mMonitors.size() < 1)  // lazy init
        initMonitors();
    return m_mMonitors;
}

int Environment::getMonitor() const {
    const int display = static_cast<int>(SDL_GetDisplayForWindow(m_window));
    return display == 0 ? -1 : display;  // 0 == invalid, according to SDL
}

vec2 Environment::getNativeScreenSize() const {
    SDL_DisplayID di = SDL_GetDisplayForWindow(m_window);
    return {static_cast<float>(SDL_GetDesktopDisplayMode(di)->w), static_cast<float>(SDL_GetDesktopDisplayMode(di)->h)};
}

McRect Environment::getDesktopRect() const { return {{}, getNativeScreenSize()}; }

McRect Environment::getWindowRect() const { return {getWindowPos(), getWindowSize()}; }

bool Environment::isPointValid(vec2 point) const {  // whether an x,y coordinate lands on an actual display
    if(m_mMonitors.size() < 1) initMonitors();
    // check for the trivial case first
    const bool withinMinMaxBounds = m_fullDesktopBoundingBox.contains(point);
    if(!withinMinMaxBounds) {
        return false;
    }
    // if it's within the full min/max bounds, make sure it actually lands inside of a display rect within the
    // coordinate space (not in some empty space between/around, like with different monitor orientations)
    for(const auto &[_, dp] : m_mMonitors) {
        if(dp.contains(point)) return true;
    }
    return false;
}

int Environment::getDPI() const {
    float dpi = SDL_GetWindowDisplayScale(m_window) * 96;

    return std::clamp<int>((int)dpi, 96, 96 * 2);  // sanity clamp
}

void Environment::setCursor(CURSORTYPE cur) {
    if(m_mCursorIcons.empty()) initCursors();
    if(m_cursorType != cur) {
        m_cursorType = cur;
        SDL_SetCursor(m_mCursorIcons.at(m_cursorType));  // does not make visible if the cursor isn't visible
    }
}

void Environment::setRawInput(bool raw) {
    m_bActualRawInputState = raw;

    if(raw == SDL_GetWindowRelativeMouseMode(m_window)) {
        // nothing to do
        return;
    }

    if(!raw && mouse) {
        // need to manually set the cursor position if we're disabling raw input
        setOSMousePos(mouse->getRealPos());
    }

    if(!SDL_SetWindowRelativeMouseMode(m_window, raw)) {
        debugLog("FIXME (handle error): SDL_SetWindowRelativeMouseMode failed: {:s}\n", SDL_GetError());
        m_bActualRawInputState = !raw;
    }
}

void Environment::setCursorVisible(bool visible) {
    m_bCursorVisible = visible;

    if(visible) {
        // disable rawinput (allow regular mouse movement)
        setRawInput(false);
        SDL_SetWindowMouseGrab(m_window, false);  // release grab
        SDL_ShowCursor();
    } else {
        setCursor(CURSORTYPE::CURSOR_NORMAL);
        SDL_HideCursor();

        if(mouse && mouse->isRawInputWanted()) {  // re-enable rawinput
            setRawInput(true);
        }
        if(isCursorClipped()) {
            SDL_SetWindowMouseGrab(m_window, true);  // re-enable grab
        }
    }

    // sanity
    m_bCursorVisible = SDL_CursorVisible();
}

void Environment::setCursorClip(bool clip, McRect rect) {
    m_cursorClipRect = rect;
    if(clip) {
        const SDL_Rect sdlClip = McRectToSDLRect(rect);
        SDL_SetWindowMouseRect(m_window, &sdlClip);
        SDL_SetWindowMouseGrab(m_window, true);
        m_bCursorClipped = true;
    } else {
        m_bCursorClipped = false;
        SDL_SetWindowMouseRect(m_window, nullptr);
        SDL_SetWindowMouseGrab(m_window, false);
    }
}

void Environment::setOSMousePos(vec2 pos) {
    SDL_WarpMouseInWindow(m_window, pos.x, pos.y);
    m_vLastAbsMousePos = pos;
}

UString Environment::keyCodeToString(KEYCODE keyCode) {
    const char *name = SDL_GetScancodeName((SDL_Scancode)keyCode);
    if(name == nullptr)
        return UString::format("%lu", keyCode);
    else {
        UString uName = UString(name);
        if(uName.length() < 1)
            return UString::format("%lu", keyCode);
        else
            return uName;
    }
}

bool Environment::grabKeyboard(bool grab) { return SDL_SetWindowKeyboardGrab(m_window, grab); }

void Environment::listenToTextInput(bool listen) {
    listen ? SDL_StartTextInput(m_window) : SDL_StopTextInput(m_window);
}

//******************************//
//	internal helpers/callbacks  //
//******************************//

// convar callback
void Environment::setProcessPriority(float newPrio) {
    SDL_SetCurrentThreadPriority(!!static_cast<int>(newPrio) ? SDL_THREAD_PRIORITY_HIGH : SDL_THREAD_PRIORITY_NORMAL);
}

// convar callback
void Environment::onLogLevelChange(float newval) {
    const bool enable = !!static_cast<int>(newval);
    if(enable && !m_bEnvDebug) {
        envDebug(true);
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_TRACE);
    } else if(!enable && m_bEnvDebug) {
        envDebug(false);
        SDL_ResetLogPriorities();
    }
}

SDL_Rect Environment::McRectToSDLRect(const McRect &mcrect) noexcept {
    return {.x = static_cast<int>(mcrect.getX()),
            .y = static_cast<int>(mcrect.getY()),
            .w = static_cast<int>(mcrect.getWidth()),
            .h = static_cast<int>(mcrect.getHeight())};
}

McRect Environment::SDLRectToMcRect(const SDL_Rect &sdlrect) noexcept {
    return {static_cast<float>(sdlrect.x), static_cast<float>(sdlrect.y), static_cast<float>(sdlrect.w),
            static_cast<float>(sdlrect.h)};
}

std::pair<vec2, vec2> Environment::consumeMousePositionCache() {
    float xRel{0.f}, yRel{0.f};
    float x{m_vLastAbsMousePos.x}, y{m_vLastAbsMousePos.y};

    // this gets zeroed on every call to it, which is why this function "consumes" data
    // both of these calls are only updated with the last SDL_PumpEvents call
    SDL_GetRelativeMouseState(&xRel, &yRel);
    SDL_GetMouseState(&x, &y);

    // <rel, abs>
    return {{xRel, yRel}, {x, y}};
}

void Environment::initCursors() {
    m_mCursorIcons = {
        {CURSORTYPE::CURSOR_NORMAL, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT)},
        {CURSORTYPE::CURSOR_WAIT, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT)},
        {CURSORTYPE::CURSOR_SIZE_H, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE)},
        {CURSORTYPE::CURSOR_SIZE_V, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE)},
        {CURSORTYPE::CURSOR_SIZE_HV, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE)},
        {CURSORTYPE::CURSOR_SIZE_VH, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE)},
        {CURSORTYPE::CURSOR_TEXT, SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT)},
    };
}

void Environment::initMonitors(bool force) const {
    if(!force && !m_mMonitors.empty())
        return;
    else if(force)  // refresh
        m_mMonitors.clear();

    int count = -1;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);

    m_fullDesktopBoundingBox = {};  // the min/max coordinates, for "valid point" lookups (checked first before
                                    // iterating through actual monitor rects)
    for(int i = 0; i < count; i++) {
        const SDL_DisplayID di = displays[i];

        McRect displayRect{};
        vec2 size{0.f};
        SDL_Rect sdlDisplayRect{};

        if(!SDL_GetDisplayBounds(di, &sdlDisplayRect)) {
            // fallback
            size = vec2{static_cast<float>(SDL_GetDesktopDisplayMode(di)->w),
                        static_cast<float>(SDL_GetDesktopDisplayMode(di)->h)};
            displayRect = McRect{{}, size};
            // expand the display bounds, we just have to assume that the displays are placed left-to-right, with Y
            // coordinates at 0 (in this fallback path)
            if(size.y > m_fullDesktopBoundingBox.getHeight()) {
                m_fullDesktopBoundingBox.setHeight(size.y);
            }
            m_fullDesktopBoundingBox.setWidth(m_fullDesktopBoundingBox.getWidth() + size.x);
        } else {
            displayRect = SDLRectToMcRect(sdlDisplayRect);
            size = displayRect.getSize();
            // otherwise we can get the min/max bounding box accurately
            m_fullDesktopBoundingBox = m_fullDesktopBoundingBox.Union(displayRect);
        }
        m_mMonitors.try_emplace(di, displayRect);
    }

    if(count < 1) {
        debugLog("WARNING: No monitors found! Adding default monitor ...\n");
        const vec2 windowSize = getWindowSize();
        m_mMonitors.try_emplace(1, McRect{{}, windowSize});
    }

    // make sure this is also valid
    if(m_fullDesktopBoundingBox.getSize() == vec2{0, 0}) {
        m_fullDesktopBoundingBox = getWindowRect();
    }

    SDL_free(displays);
}

// TODO: filter?
void Environment::sdlFileDialogCallback(void *userdata, const char *const *filelist, int /*filter*/) {
    auto *callbackData = static_cast<FileDialogCallbackData *>(userdata);
    if(!callbackData) return;

    std::vector<UString> results;

    if(filelist) {
        for(const char *const *curr = filelist; *curr; curr++) {
            results.emplace_back(*curr);
        }
    }

    // call the callback
    callbackData->callback(results);

    // data is no longer needed
    delete callbackData;
}

// folder = true means return the canonical filesystem path to the folder containing the given path
//			if the path is already a folder, just return it directly
// folder = false means to strip away the file path separators from the given path and return just the filename itself
std::string Environment::getThingFromPathHelper(const std::string &path, bool folder) noexcept {
    if(path.empty()) return path;
    namespace fs = std::filesystem;

    std::string retPath{path};

    // find the last path separator (either / or \)
    auto lastSlash = retPath.find_last_of('/');
    if(lastSlash == std::string::npos) lastSlash = retPath.find_last_of('\\');

    if(folder) {
        // if path ends with separator, it's already a directory
        bool endsWithSeparator = retPath.back() == '/' || retPath.back() == '\\';

        UString ustrPath{retPath};
        std::error_code ec;
        auto abs_path = fs::canonical(ustrPath.plat_str(), ec);

        if(!ec)  // canonical path found
        {
            auto status = fs::status(abs_path, ec);
            // if it's already a directory or it doesn't have a parent path then just return it directly
            if(ec || status.type() == fs::file_type::directory || !abs_path.has_parent_path())
                ustrPath = abs_path.c_str();
            // else return the parent directory for the file
            else if(abs_path.has_parent_path() && !abs_path.parent_path().empty())
                ustrPath = abs_path.parent_path().c_str();
        } else if(!endsWithSeparator)  // canonical failed, handle manually (if it's not already a directory)
        {
            if(lastSlash != std::string::npos)  // return parent
                ustrPath = ustrPath.substr(0, lastSlash);
            else  // no separators found, just use ./
                ustrPath = UString::fmt("." PREF_PATHSEP "{}", ustrPath);
        }
        retPath = ustrPath.utf8View();
        // make sure whatever we got now ends with a slash
        if(retPath.back() != '/' && retPath.back() != '\\') retPath = retPath + PREF_PATHSEP;
    } else if(lastSlash != std::string::npos)  // just return the file
    {
        retPath = retPath.substr(lastSlash + 1);
    }
    // else: no separators found, entire path is the filename

    return retPath;
}

#ifdef MCENGINE_PLATFORM_WINDOWS  // the win32 api is just WAY faster for this

std::vector<std::string> Environment::enumerateDirectory(const std::string &pathToEnum,
                                                         /* enum SDL_PathType */ unsigned int type) noexcept {
    // Since we want to avoid wide strings in the codebase as much as possible,
    // we convert wide paths to UTF-8 (as they fucking should be).
    // We can't just use FindFirstFileA, because then any path with unicode
    // characters will fail to open!
    // Keep in mind that windows can't handle the way too modern 1993 UTF-8, so
    // you have to use std::filesystem::u8path() or convert it back to a wstring
    // before using the windows API.

    const bool wantDirs = (type == SDL_PATHTYPE_DIRECTORY);

    std::string folder{pathToEnum};
    folder.append("*.*");
    WIN32_FIND_DATAW data;
    std::wstring buffer;
    std::vector<std::string> entries;

    int size = MultiByteToWideChar(CP_UTF8, 0, folder.c_str(), folder.length(), NULL, 0);
    std::wstring wentry(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, folder.c_str(), folder.length(), (LPWSTR)wentry.c_str(), wentry.length());

    HANDLE handle = FindFirstFileW(wentry.c_str(), &data);

    while(true) {
        std::wstring filename(data.cFileName);
        if(filename == buffer) break;

        buffer = filename;

        if(filename.length() > 0 &&
           (!wantDirs || (wantDirs && (filename.compare(L".") != 0 && filename.compare(L"..") != 0)))) {
            if(!!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == wantDirs) {
                int size = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), filename.length(), NULL, 0, NULL, NULL);
                std::string utf8filename(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), size, (LPSTR)utf8filename.c_str(), size, NULL, NULL);
                entries.push_back(utf8filename);
            }
        }

        FindNextFileW(handle, &data);
    }

    FindClose(handle);

    return entries;
}

#else

// for getting files in folder/ folders in folder
std::vector<std::string> Environment::enumerateDirectory(const std::string &pathToEnum,
                                                         /* enum SDL_PathType */ unsigned int type) noexcept {
    namespace fs = std::filesystem;

    std::vector<std::string> contents;
    contents.reserve(512);

    std::error_code ec;
    const bool wantFiles = (type == SDL_PATHTYPE_FILE);
    const bool wantDirs = (type == SDL_PATHTYPE_DIRECTORY);

    for(const auto &entry : fs::directory_iterator(pathToEnum, ec)) {
        if(ec) continue;
        auto fileType = entry.status(ec).type();

        if((wantFiles && fileType == fs::file_type::regular) || (wantDirs && fileType == fs::file_type::directory)) {
            const auto &filename = entry.path().filename();
            if constexpr(Env::cfg(OS::WINDOWS)) {  // std::filesystem BS
                contents.emplace_back(UString{filename.generic_string()}.utf8View());
            } else {
                contents.emplace_back(filename.generic_string());
            }
        }
    }

    if(ec && contents.empty()) {
        debugLog("Failed to enumerate directory: {}\n", ec.message());
    }

    return contents;
}

#endif  // MCENGINE_PLATFORM_WINDOWS

// hotfix/workaround cursor confinement issue (maybe just with i3wm?)
bool Environment::sdl_x11eventhook(void *thisptr, XEvent *xev) {
    (void)thisptr, (void)xev;
#ifdef MCENGINE_PLATFORM_LINUX
    if(thisptr && xev && xev->type == 10 /* FocusOut */) {
        auto *envptr = static_cast<Environment *>(thisptr);
        if(envptr->m_window) {
            // unconfine mouse manually
            SDL_SetWindowMouseRect(envptr->m_window, nullptr);
        }
    }
#endif
    return true;
}
