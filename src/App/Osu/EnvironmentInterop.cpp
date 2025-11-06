#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"  // for extract_beatmapset
#include "File.h"
#include "MainMenu.h"
#include "NeosuUrl.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Parsing.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"
#include "Logging.h"

namespace {  // static namespace

// drag-drop/file associations/registry stuff below
void handle_osk(const char *osk_path) {
    Skin::unpack(osk_path);

    auto folder_name = Environment::getFileNameFromFilePath(osk_path);
    folder_name.erase(folder_name.size() - 4);  // remove .osk extension

    cv::skin.setValue(Environment::getFileNameFromFilePath(folder_name).c_str());
    osu->getOptionsMenu()->updateSkinNameLabel();
}

void handle_osz(const char *osz_path) {
    uSz osz_filesize = 0;
    std::unique_ptr<u8[]> osz_data = nullptr;
    {
        File osz(osz_path);
        osz_filesize = osz.getFileSize();
        osz_data = osz.takeFileBuffer();
        if(!osz.canRead() || !osz_filesize || !osz_data) {
            osu->getNotificationOverlay()->addToast(fmt::format("Failed to import {}", osz_path), ERROR_TOAST);
            return;
        }
    }

    i32 set_id = Downloader::extract_beatmapset_id(osz_data.get(), osz_filesize);
    if(set_id < 0) {
        // special case: legacy fallback behavior for invalid beatmapSetID, try to parse the ID from the
        // path
        auto mapset_name = Environment::getFileNameFromFilePath(osz_path);
        if(!mapset_name.empty() && std::isdigit(static_cast<unsigned char>(mapset_name[0]))) {
            if(!Parsing::parse(mapset_name.c_str(), &set_id)) {
                set_id = -1;
            }
        }
    }
    if(set_id == -1) {
        osu->getNotificationOverlay()->addToast(u"Beatmapset doesn't have a valid ID.", ERROR_TOAST);
        return;
    }

    std::string mapset_dir = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);
    Environment::createDirectory(mapset_dir);
    if(!Downloader::extract_beatmapset(osz_data.get(), osz_filesize, mapset_dir)) {
        osu->getNotificationOverlay()->addToast(u"Failed to extract beatmapset", ERROR_TOAST);
        return;
    }

    db->addBeatmapSet(mapset_dir, set_id);
    if(!osu->getSongBrowser()->selectBeatmapset(set_id)) {
        osu->getNotificationOverlay()->addToast(u"Failed to import beatmapset", ERROR_TOAST);
        return;
    }
}
}  // namespace

bool Environment::Interop::handle_cmdline_args(const std::vector<std::string> &args) {
    bool need_to_reload_database = false;

    for(const auto &arg : args) {
        // XXX: naive way of ignoring '-sound soloud' type params, might break in the future
        if(arg[0] == '-') continue;
        if(arg.length() < 4) continue;

        if(arg.starts_with("neosu://")) {
            handle_neosu_url(arg.c_str());
        } else {
            auto extension = Environment::getFileExtensionFromFilePath(arg);
            if(!extension.compare("osz")) {
                // NOTE: we're assuming db is loaded here?
                handle_osz(arg.c_str());
            } else if(!extension.compare("osk") || !extension.compare("zip")) {
                handle_osk(arg.c_str());
            } else if(!extension.compare("db") && !db->isLoading()) {
                db->dbPathsToImport.emplace_back(arg.c_str());
                need_to_reload_database = true;
            }
        }
    }

    if(need_to_reload_database) {
        osu->getSongBrowser()->refreshBeatmaps();
    }

    return need_to_reload_database;
}

#ifdef _WIN32

#include "Engine.h"
#include "SString.h"

#include "WinDebloatDefs.h"
#include <objbase.h>

#include <SDL3/SDL_system.h>

#define NEOSU_WINDOW_MESSAGE_ID TEXT("NEOSU_CMDLINE")
namespace {  // static
bool sdl_windows_message_hook(void *userdata, MSG *msg) {
    static UINT neosu_msg = RegisterWindowMessage(NEOSU_WINDOW_MESSAGE_ID);

    // true == continue processing
    if(!userdata || !msg || msg->message != neosu_msg) {
        return true;
    }
    Environment *env_ptr{static_cast<Environment *>(userdata)};

    // check the custom registered message

    // reconstruct the mapping/event names from the identifier passed in lParam
    auto sender_pid = static_cast<DWORD>(msg->wParam);
    auto identifier = static_cast<DWORD>(msg->lParam);

    const std::string mapping_name = fmt::format("neosu_cmdline_{}_{}", sender_pid, identifier);
    const std::string event_name = fmt::format("neosu_event_{}_{}", sender_pid, identifier);

    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapping_name.c_str());
    if(hMapFile) {
        bool signaled = false;

        auto signal_completion = [&signaled, &event_name]() -> void {
            if(signaled) return;
            signaled = true;
            HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, event_name.c_str());
            if(hEvent) {
                SetEvent(hEvent);
                CloseHandle(hEvent);
            }
        };

        LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if(pBuf) {
            // first 4 bytes contain the data size
            DWORD data_size = *((DWORD *)pBuf);
            const char *data = ((const char *)pBuf) + sizeof(DWORD);

            // parse null-separated arguments
            std::vector<std::string> args;
            const char *current = data;
            const char *end = data + data_size;

            while(current < end) {
                size_t len = strnlen(current, end - current);
                if(len > 0) {
                    args.emplace_back(current, len);
                }
                current += len + 1;  // split on null, skip null
            }

            UnmapViewOfFile(pBuf);

            // we're done with the mapped file, signal the event
            signal_completion();

            // handle the arguments
            if(!args.empty()) {
                debugLog("handling external arguments: {}", SString::join(args));
                env_ptr->getEnvInterop().handle_cmdline_args(args);
            }
        }
        CloseHandle(hMapFile);
        signal_completion();
    }

    // focus current window
    env_ptr->focus();

    return false;  // we already processed everything, don't fallthrough to sdl
}
}  // namespace

void Environment::Interop::handle_existing_window(int argc, char *argv[]) {
    // if a neosu instance is already running, send it a message then quit
    HWND existing_window = FindWindow(TEXT(PACKAGE_NAME), nullptr);
    if(existing_window && argc > 1) {  // only send if we have more than just the exe name as args

        size_t total_size = 0;
        for(int i = 1; i < argc; i++) {         // skip exe name
            total_size += strlen(argv[i]) + 1;  // +1 for null terminator
        }

        if(total_size > 0 && total_size < 4096) {  // reasonable size limit...?
            // need to create a unique identifier for this message
            DWORD sender_pid = GetCurrentProcessId();
            DWORD identifier = GetTickCount();

            // create unique names for mapping and event
            const std::string mapping_name = fmt::format("neosu_cmdline_{}_{}", sender_pid, identifier);
            const std::string event_name = fmt::format("neosu_event_{}_{}", sender_pid, identifier);

            // create completion event first, so we know when we can exit this process
            HANDLE hEvent = CreateEventA(nullptr, FALSE, FALSE, event_name.c_str());

            // create named shared memory for the data
            // for some reason, WM_COPYDATA hooks don't work with the SDL message loop... this is the next best solution
            HANDLE hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                                 total_size + sizeof(DWORD),  // extra space for size header
                                                 mapping_name.c_str());

            if(hMapFile && hEvent) {
                LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                if(pBuf) {
                    // store the size first
                    *((DWORD *)pBuf) = (DWORD)total_size;
                    char *data = ((char *)pBuf) + sizeof(DWORD);
                    char *current = data;

                    // pack arguments with null separators, so we can split them easily
                    for(int i = 1; i < argc; i++) {
                        size_t len = strlen(argv[i]);
                        memcpy(current, argv[i], len);
                        current[len] = '\0';
                        current += len + 1;
                    }

                    UnmapViewOfFile(pBuf);

                    // post the identifier message
                    UINT neosu_msg = RegisterWindowMessage(NEOSU_WINDOW_MESSAGE_ID);

                    if(PostMessage(existing_window, neosu_msg, (WPARAM)sender_pid, (LPARAM)identifier)) {
                        // wait for the receiver to signal completion (with 5 second timeout)
                        DWORD wait_result = WaitForSingleObject(hEvent, 5000);
                        switch(wait_result) {
                            case WAIT_OBJECT_0:
                                // success
                                break;
                            case WAIT_TIMEOUT:
                                debugLog("timeout waiting for message processing completion");
                                break;
                            case WAIT_FAILED:
                                debugLog("failed to wait for completion event, error: {}", GetLastError());
                                break;
                            default:
                                debugLog("unexpected wait result: {}", wait_result);
                                break;
                        }
                    } else {
                        debugLog("failed to post message to existing HWND {}, error: {}",
                                 static_cast<const void *>(existing_window), GetLastError());
                    }
                }
            }
            if(hMapFile) CloseHandle(hMapFile);
            if(hEvent) CloseHandle(hEvent);
        }

        SetForegroundWindow(existing_window);
        std::exit(0);
    }
}

void Environment::Interop::setup_system_integrations() {
    SDL_SetWindowsMessageHook(sdl_windows_message_hook, (void *)this->m_env);

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Register neosu as an application
    HKEY neosu_key;
    i32 err = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\neosu", 0, nullptr, REG_OPTION_NON_VOLATILE,
                              KEY_WRITE, nullptr, &neosu_key, nullptr);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: {} (root)", err);
        return;
    }
    RegSetValueExW(neosu_key, L"", 0, REG_SZ, (BYTE *)L"neosu", 12);
    RegSetValueExW(neosu_key, L"URL Protocol", 0, REG_SZ, (BYTE *)L"", 2);

    HKEY app_key;
    err = RegCreateKeyExW(neosu_key, L"Application", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &app_key,
                          nullptr);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: {} (app)", err);
        RegCloseKey(neosu_key);
        return;
    }
    RegSetValueExW(app_key, L"ApplicationName", 0, REG_SZ, (BYTE *)L"neosu", 12);
    RegCloseKey(app_key);

    HKEY cmd_key;
    err = RegCreateKeyExW(neosu_key, L"shell\\open\\command", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                          &cmd_key, nullptr);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: {} (command)", err);
        RegCloseKey(neosu_key);
        return;
    }

    // Add current launch arguments, so doing "Open with -> neosu"
    // will always use the last launch options the player used.
    auto cmdline = env->getCommandLine();
    assert(!cmdline.empty());
    cmdline.erase(cmdline.begin());  // remove program name
    const UString launch_args = SString::join(cmdline);

    wchar_t command[MAX_PATH + 10];
    swprintf_s(command, _countof(command), L"\"%s\"%s \"%%1\"", exePath, launch_args.wchar_str());
    RegSetValueExW(cmd_key, L"", 0, REG_SZ, (BYTE *)command, (wcslen(command) + 1) * sizeof(wchar_t));
    RegCloseKey(cmd_key);

    RegCloseKey(neosu_key);

    // Register neosu as .osk handler
    HKEY osk_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\.osk\\OpenWithProgids"), 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &osk_key, nullptr);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osk format handler. Error: {}", err);
        return;
    }
    RegSetValueEx(osk_key, TEXT("neosu"), 0, REG_SZ, (BYTE *)TEXT(""), sizeof(TEXT("")));
    RegCloseKey(osk_key);

    // Register neosu as .osr handler
    HKEY osr_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\.osr\\OpenWithProgids"), 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &osr_key, nullptr);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osr format handler. Error: {}", err);
        return;
    }
    RegSetValueExW(osr_key, TEXT("neosu"), 0, REG_SZ, (BYTE *)TEXT(""), sizeof(TEXT("")));
    RegCloseKey(osr_key);

    // Register neosu as .osz handler
    HKEY osz_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\.osz\\OpenWithProgids"), 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &osz_key, nullptr);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osz format handler. Error: {}", err);
        return;
    }
    RegSetValueEx(osz_key, TEXT("neosu"), 0, REG_SZ, (BYTE *)TEXT(""), sizeof(TEXT("")));
    RegCloseKey(osz_key);
}

#else  // not implemented
void Environment::Interop::setup_system_integrations() { return; }
void Environment::Interop::handle_existing_window(int /*argc*/, char ** /*argv*/) { return; }
#endif
