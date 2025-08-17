#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"  // for extract_beatmapset
#include "File.h"
#include "MainMenu.h"
#include "NeosuUrl.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"

namespace {  // static namespace

// drag-drop/file associations/registry stuff below
void handle_osk(const char *osk_path) {
    Skin::unpack(osk_path);

    auto folder_name = env->getFileNameFromFilePath(osk_path);
    folder_name.erase(folder_name.size() - 4);  // remove .osk extension

    cv::skin.setValue(env->getFileNameFromFilePath(folder_name).c_str());
    osu->optionsMenu->updateSkinNameLabel();
}

void handle_osz(const char *osz_path) {
    File osz(osz_path);
    i32 set_id = Downloader::extract_beatmapset_id(osz.readFile(), osz.getFileSize());
    if(set_id < 0) {
        // special case: legacy fallback behavior for invalid beatmapSetID, try to parse the ID from the
        // path
        auto mapset_name = UString(env->getFileNameFromFilePath(osz_path).c_str());
        const std::vector<UString> tokens = mapset_name.split(" ");
        for(const auto &token : tokens) {
            i32 id = token.toInt();
            if(id > 0) set_id = id;
        }
    }
    if(set_id == -1) {
        osu->notificationOverlay->addToast("Beatmapset doesn't have a valid ID.", ERROR_TOAST);
        return;
    }

    std::string mapset_dir = MCENGINE_DATA_DIR "maps" PREF_PATHSEP;
    mapset_dir.append(std::to_string(set_id));
    mapset_dir.append(PREF_PATHSEP);
    if(!Environment::directoryExists(mapset_dir)) {
        env->createDirectory(mapset_dir);
    }
    if(!Downloader::extract_beatmapset(osz.readFile(), osz.getFileSize(), mapset_dir)) {
        osu->notificationOverlay->addToast("Failed to extract beatmapset", ERROR_TOAST);
        return;
    }

    db->addBeatmapSet(mapset_dir);
    if(!osu->getSongBrowser()->selectBeatmapset(set_id)) {
        osu->notificationOverlay->addToast("Failed to import beatmapset", ERROR_TOAST);
        return;
    }

    // prevent song browser from picking main menu song after database loads
    // (we just loaded and selected another song, so previous no longer applies)
    SAFE_DELETE(osu->mainMenu->preloaded_beatmapset);
}
}  // namespace

void Environment::Interop::handle_cmdline_args(const std::vector<UString> &args) {
    bool need_to_reload_database = false;

    for(auto &arg : args) {
        if(arg[0] == '-') return;
        if(arg.length() < 4) return;

        if(strstr(arg.toUtf8(), "neosu://") == arg.toUtf8()) {
            handle_neosu_url(arg.toUtf8());
        } else {
            auto extension = env->getFileExtensionFromFilePath(arg.toUtf8());
            if(!extension.compare("osz")) {
                // NOTE: we're assuming db is loaded here?
                handle_osz(arg.toUtf8());
            } else if(!extension.compare("osk") || !extension.compare("zip")) {
                handle_osk(arg.toUtf8());
            } else if(!extension.compare("db") && !db->isLoading()) {
                db->dbPathsToImport.emplace_back(arg.toUtf8());
                need_to_reload_database = true;
            }
        }
    }

    if(need_to_reload_database) {
        osu->getSongBrowser()->refreshBeatmaps();
    }
}

// these methods need to be factored out somehow to not be 100% tied to the windows main loop, putting them here to
// avoid bloating main.cpp
#ifdef _WIN32

#include "Engine.h"

#include "WinDebloatDefs.h"
#include <objbase.h>

void Environment::Interop::register_file_associations() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Register neosu as an application
    HKEY neosu_key;
    i32 err = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\neosu", 0, NULL, REG_OPTION_NON_VOLATILE,
                              KEY_WRITE, NULL, &neosu_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: {:d} (root)\n", err);
        return;
    }
    RegSetValueExW(neosu_key, L"", 0, REG_SZ, (BYTE *)L"neosu", 12);
    RegSetValueExW(neosu_key, L"URL Protocol", 0, REG_SZ, (BYTE *)L"", 2);

    HKEY app_key;
    err = RegCreateKeyExW(neosu_key, L"Application", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &app_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: {:d} (app)\n", err);
        RegCloseKey(neosu_key);
        return;
    }
    RegSetValueExW(app_key, L"ApplicationName", 0, REG_SZ, (BYTE *)L"neosu", 12);
    RegCloseKey(app_key);

    HKEY cmd_key;
    wchar_t command[MAX_PATH + 10];
    swprintf_s(command, _countof(command), L"\"%s\" \"%%1\"", exePath);
    err = RegCreateKeyExW(neosu_key, L"shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                          &cmd_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: {:d} (command)\n", err);
        RegCloseKey(neosu_key);
        return;
    }
    RegSetValueExW(cmd_key, L"", 0, REG_SZ, (BYTE *)command, (wcslen(command) + 1) * sizeof(wchar_t));
    RegCloseKey(cmd_key);

    RegCloseKey(neosu_key);

    // Register neosu as .osk handler
    HKEY osk_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\.osk\\OpenWithProgids"), 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &osk_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osk format handler. Error: {:d}\n", err);
        return;
    }
    RegSetValueEx(osk_key, TEXT("neosu"), 0, REG_SZ, (BYTE *)L"", 2);
    RegCloseKey(osk_key);

    // Register neosu as .osr handler
    HKEY osr_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\.osr\\OpenWithProgids"), 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &osr_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osr format handler. Error: {:d}\n", err);
        return;
    }
    RegSetValueEx(osr_key, TEXT("neosu"), 0, REG_SZ, (BYTE *)L"", 2);
    RegCloseKey(osr_key);

    // Register neosu as .osz handler
    HKEY osz_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\.osz\\OpenWithProgids"), 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &osz_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osz format handler. Error: {:d}\n", err);
        return;
    }
    RegSetValueEx(osz_key, TEXT("neosu"), 0, REG_SZ, (BYTE *)L"", 2);
    RegCloseKey(osz_key);
}

#define WM_NEOSU_PROTOCOL (WM_USER + 1)
// TODO SDL: there's no way this works properly
void Environment::Interop::handle_existing_window(int argc, char *argv[]) {
    // if a neosu instance is already running, send it a message then quit
    HWND existing_window = FindWindowW(L"neosu", NULL);
    if(existing_window) {
        for(i32 i = 0; i < argc; i++) {
            COPYDATASTRUCT cds;
            cds.dwData = WM_NEOSU_PROTOCOL;
            cds.cbData = strlen(argv[i]) + 1;
            cds.lpData = argv[i];
            SendMessage(existing_window, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        }

        SetForegroundWindow(existing_window);
        std::exit(0);
    }
}

#else // not implemented
void Environment::Interop::register_file_associations() { return; }
void Environment::Interop::handle_existing_window(int /*argc*/, char **/*argv*/) { return; }
#endif
