// Copyright (c) 2024, kiwec, All rights reserved.
#include "PeppyImporter.h"

#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "KeyBindings.h"
#include "Database.h"
#include "OptionsMenu.h"
#include "Parsing.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"

#include <winbase.h>
#include <winreg.h>
#endif
namespace PeppyImporter {
namespace {  // static namespace
void try_set_key(const std::string& key, ConVar* cvar) {
    if(key == "Noe") {
        cvar->setValue(0);
    } else if(key == "A") {
        cvar->setValue((int)KEY_A);
    } else if(key == "B") {
        cvar->setValue((int)KEY_B);
    } else if(key == "C") {
        cvar->setValue((int)KEY_C);
    } else if(key == "D") {
        cvar->setValue((int)KEY_D);
    } else if(key == "E") {
        cvar->setValue((int)KEY_E);
    } else if(key == "F") {
        cvar->setValue((int)KEY_F);
    } else if(key == "G") {
        cvar->setValue((int)KEY_G);
    } else if(key == "H") {
        cvar->setValue((int)KEY_H);
    } else if(key == "I") {
        cvar->setValue((int)KEY_I);
    } else if(key == "J") {
        cvar->setValue((int)KEY_J);
    } else if(key == "K") {
        cvar->setValue((int)KEY_K);
    } else if(key == "L") {
        cvar->setValue((int)KEY_L);
    } else if(key == "M") {
        cvar->setValue((int)KEY_M);
    } else if(key == "N") {
        cvar->setValue((int)KEY_N);
    } else if(key == "O") {
        cvar->setValue((int)KEY_O);
    } else if(key == "P") {
        cvar->setValue((int)KEY_P);
    } else if(key == "Q") {
        cvar->setValue((int)KEY_Q);
    } else if(key == "R") {
        cvar->setValue((int)KEY_R);
    } else if(key == "S") {
        cvar->setValue((int)KEY_S);
    } else if(key == "T") {
        cvar->setValue((int)KEY_T);
    } else if(key == "U") {
        cvar->setValue((int)KEY_U);
    } else if(key == "V") {
        cvar->setValue((int)KEY_V);
    } else if(key == "W") {
        cvar->setValue((int)KEY_W);
    } else if(key == "X") {
        cvar->setValue((int)KEY_X);
    } else if(key == "Y") {
        cvar->setValue((int)KEY_Y);
    } else if(key == "Z") {
        cvar->setValue((int)KEY_Z);
    } else if(key == "0") {
        cvar->setValue((int)KEY_0);
    } else if(key == "1") {
        cvar->setValue((int)KEY_1);
    } else if(key == "2") {
        cvar->setValue((int)KEY_2);
    } else if(key == "3") {
        cvar->setValue((int)KEY_3);
    } else if(key == "4") {
        cvar->setValue((int)KEY_4);
    } else if(key == "5") {
        cvar->setValue((int)KEY_5);
    } else if(key == "6") {
        cvar->setValue((int)KEY_6);
    } else if(key == "7") {
        cvar->setValue((int)KEY_7);
    } else if(key == "8") {
        cvar->setValue((int)KEY_8);
    } else if(key == "9") {
        cvar->setValue((int)KEY_9);
    } else if(key == "F1") {
        cvar->setValue((int)KEY_F1);
    } else if(key == "F2") {
        cvar->setValue((int)KEY_F2);
    } else if(key == "F3") {
        cvar->setValue((int)KEY_F3);
    } else if(key == "F4") {
        cvar->setValue((int)KEY_F4);
    } else if(key == "F5") {
        cvar->setValue((int)KEY_F5);
    } else if(key == "F6") {
        cvar->setValue((int)KEY_F6);
    } else if(key == "F7") {
        cvar->setValue((int)KEY_F7);
    } else if(key == "F8") {
        cvar->setValue((int)KEY_F8);
    } else if(key == "F9") {
        cvar->setValue((int)KEY_F9);
    } else if(key == "F10") {
        cvar->setValue((int)KEY_F10);
    } else if(key == "F11") {
        cvar->setValue((int)KEY_F11);
    } else if(key == "F12") {
        cvar->setValue((int)KEY_F12);
    } else if(key == "Left") {
        cvar->setValue((int)KEY_LEFT);
    } else if(key == "Right") {
        cvar->setValue((int)KEY_RIGHT);
    } else if(key == "Up") {
        cvar->setValue((int)KEY_UP);
    } else if(key == "Down") {
        cvar->setValue((int)KEY_DOWN);
    } else if(key == "Tab") {
        cvar->setValue((int)KEY_TAB);
    } else if((key == "Return") || (key == "Enter")) {
        cvar->setValue((int)KEY_ENTER);
    } else if(key == "Shift") {
        cvar->setValue((int)KEY_LSHIFT);
    } else if(key == "Control") {
        cvar->setValue((int)KEY_LCONTROL);
    } else if(key == "LeftAlt") {
        cvar->setValue((int)KEY_LALT);
    } else if(key == "RightAlt") {
        cvar->setValue((int)KEY_RALT);
    } else if(key == "Escape") {
        cvar->setValue((int)KEY_ESCAPE);
    } else if(key == "Space") {
        cvar->setValue((int)KEY_SPACE);
    } else if(key == "Back") {
        cvar->setValue((int)KEY_BACKSPACE);
    } else if(key == "End") {
        cvar->setValue((int)KEY_END);
    } else if(key == "Insert") {
        cvar->setValue((int)KEY_INSERT);
    } else if(key == "Delete") {
        cvar->setValue((int)KEY_DELETE);
    } else if(key == "Help") {
        cvar->setValue((int)KEY_HELP);
    } else if(key == "Home") {
        cvar->setValue((int)KEY_HOME);
    } else if(key == "Escape") {
        cvar->setValue((int)KEY_ESCAPE);
    } else if(key == "PageUp") {
        cvar->setValue((int)KEY_PAGEUP);
    } else if(key == "PageDown") {
        cvar->setValue((int)KEY_PAGEDOWN);
    } else {
        debugLog("No key code found for '{}'!\n", key);
    }
}

void update_osu_folder_from_registry() {
#ifdef _WIN32
    i32 err;
    HKEY key;

    auto key_path = L"Software\\Classes\\osustable.File.osk\\Shell\\Open\\Command";
    err = RegOpenKeyExW(HKEY_CURRENT_USER, key_path, 0, KEY_READ, &key);
    if(err != ERROR_SUCCESS) {
        // Older registry key, in case the user isn't on latest osu!stable version
        // See https://osu.ppy.sh/home/changelog/cuttingedge/20250111
        err = RegOpenKeyExW(HKEY_CLASSES_ROOT, L"osu\\shell\\open\\command", 0, KEY_READ, &key);
    }
    if(err != ERROR_SUCCESS) {
        debugLog("osu!stable not found in registry!\n");
        return;
    }

    DWORD dwType = REG_SZ;
    WCHAR szPath[MAX_PATH];
    DWORD dwSize = sizeof(szPath);
    err = RegQueryValueExW(key, NULL, NULL, &dwType, (LPBYTE)szPath, &dwSize);
    RegCloseKey(key);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to get path of osu:// protocol handler.\n");
        return;
    }

    // The path is in format: "C:\Path\To\osu!.exe" "%1"
    WCHAR* endQuote = wcschr(szPath + 1, L'"');
    if(endQuote) {
        *endQuote = L'\0';
    }
    WCHAR* path = szPath + 1;
    WCHAR* lastBackslash = wcsrchr(path, L'\\');
    if(lastBackslash) {
        *lastBackslash = L'\0';
        UString new_osu_folder{path};
        debugLog("Found osu! folder from registry: {:s}\n", new_osu_folder.toUtf8());
        cv::osu_folder.setValue(new_osu_folder.toUtf8());
        return;
    }
#endif
}
}  // namespace

void import_settings_from_osu_stable() {
    if(cv::osu_folder.getString().empty()) {
        update_osu_folder_from_registry();
    }

    auto username = env->getUsername();
    if(username.length() == 0) {
        debugLog("Failed to get username; not going to import settings from osu!stable.\n");
        return;
    }

    std::string cfg_path = cv::osu_folder.getString();
    cfg_path.append(PREF_PATHSEP "osu!.");
    cfg_path.append(username.toUtf8());
    cfg_path.append(".cfg");
    File file(cfg_path);

    while(file.canRead()) {
        std::string curLine = file.readLine();

        bool b;
        f32 flt;
        std::string str;

        if(Parsing::parse_setting(curLine, "BeatmapDirectory", &str)) {
            if(str.length() > 2) cv::songs_folder.setValue(str);
        } else if(Parsing::parse_setting(curLine, "VolumeUniversal", &flt))
            cv::volume_master.setValue(flt / 100.f);
        else if(Parsing::parse_setting(curLine, "VolumeEffect", &flt))
            cv::volume_effects.setValue(flt / 100.f);
        else if(Parsing::parse_setting(curLine, "VolumeMusic", &flt))
            cv::volume_music.setValue(flt / 100.f);
        else if(Parsing::parse_setting(curLine, "AllowPublicInvites", &b))
            cv::allow_mp_invites.setValue(b);
        else if(Parsing::parse_setting(curLine, "AutoChatHide", &b))
            cv::chat_auto_hide.setValue(b);
        else if(Parsing::parse_setting(curLine, "BlockNonFriendPM", &b))
            cv::allow_stranger_dms.setValue(b);
        else if(Parsing::parse_setting(curLine, "ChatAudibleHighlight", &b))
            cv::chat_ping_on_mention.setValue(b);
        else if(Parsing::parse_setting(curLine, "ChatHighlightName", &b))
            cv::chat_notify_on_mention.setValue(b);
        else if(Parsing::parse_setting(curLine, "ChatMessageNotification", &b))
            cv::chat_notify_on_dm.setValue(b);
        else if(Parsing::parse_setting(curLine, "CursorSize", &flt))
            cv::cursor_scale.setValue(flt);
        else if(Parsing::parse_setting(curLine, "AutomaticCursorSizing", &b))
            cv::automatic_cursor_size.setValue(b);
        else if(Parsing::parse_setting(curLine, "DimLevel", &flt))
            cv::background_dim.setValue(flt / 100.f);
        else if(Parsing::parse_setting(curLine, "IHateHavingFun", &b))
            cv::background_dont_fade_during_breaks.setValue(b);
        else if(Parsing::parse_setting(curLine, "DiscordRichPresence", &b))
            cv::rich_presence.setValue(b);
        else if(Parsing::parse_setting(curLine, "FpsCounter", &b))
            cv::draw_fps.setValue(b);
        else if(Parsing::parse_setting(curLine, "CursorRipple", &b))
            cv::draw_cursor_ripples.setValue(b);
        else if(Parsing::parse_setting(curLine, "HighlightWords", &str))
            cv::chat_highlight_words.setValue(str);
        else if(Parsing::parse_setting(curLine, "HighResolution", &b))
            cv::skin_hd.setValue(b);
        else if(Parsing::parse_setting(curLine, "IgnoreBeatmapSamples", &b))
            cv::ignore_beatmap_samples.setValue(b);
        else if(Parsing::parse_setting(curLine, "IgnoreBeatmapSkins", &b))
            cv::ignore_beatmap_skins.setValue(b);
        else if(Parsing::parse_setting(curLine, "IgnoreList", &str))
            cv::chat_ignore_list.setValue(str);
        else if(Parsing::parse_setting(curLine, "KeyOverlay", &b))
            cv::draw_inputoverlay.setValue(b);
        else if(Parsing::parse_setting(curLine, "Language", &str))
            cv::language.setValue(str);
        else if(Parsing::parse_setting(curLine, "ShowInterface", &b))
            cv::draw_hud.setValue(b);
        else if(Parsing::parse_setting(curLine, "LowResolution", &b))
            cv::skin_hd.setValue(!b);  // note the '!' (this is mirror of HighResolution setting)
        else if(Parsing::parse_setting(curLine, "MouseDisableButtons", &b))
            cv::disable_mousebuttons.setValue(b);
        else if(Parsing::parse_setting(curLine, "MouseDisableWheel", &b))
            cv::disable_mousewheel.setValue(b);
        else if(Parsing::parse_setting(curLine, "MouseSpeed", &flt))
            cv::mouse_sensitivity.setValue(flt);
        else if(Parsing::parse_setting(curLine, "Offset", &flt))
            cv::universal_offset.setValue(flt);
        else if(Parsing::parse_setting(curLine, "ScoreMeterScale", &flt))
            cv::hud_hiterrorbar_scale.setValue(flt);
        else if(Parsing::parse_setting(curLine, "NotifyFriends", &b))
            cv::notify_friend_status_change.setValue(b);
        else if(Parsing::parse_setting(curLine, "PopupDuringGameplay", &b))
            cv::notify_during_gameplay.setValue(b);
        else if(Parsing::parse_setting(curLine, "ScoreboardVisible", &b)) {
            cv::draw_scoreboard.setValue(b);
            cv::draw_scoreboard_mp.setValue(b);
        } else if(Parsing::parse_setting(curLine, "SongSelectThumbnails", &b))
            cv::draw_songbrowser_thumbnails.setValue(b);
        else if(Parsing::parse_setting(curLine, "ShowSpectators", &b))
            cv::draw_spectator_list.setValue(b);
        else if(Parsing::parse_setting(curLine, "AutoSendNowPlaying", &b))
            cv::spec_share_map.setValue(b);
        else if(Parsing::parse_setting(curLine, "ShowStoryboard", &b))
            cv::draw_storyboard.setValue(b);
        else if(Parsing::parse_setting(curLine, "Skin", &str))
            cv::skin.setValue(str);
        else if(Parsing::parse_setting(curLine, "SkinSamples", &b))
            cv::skin_use_skin_hitsounds.setValue(b);
        else if(Parsing::parse_setting(curLine, "SnakingSliders", &b))
            cv::snaking_sliders.setValue(b);
        else if(Parsing::parse_setting(curLine, "Video", &b))
            cv::draw_video.setValue(b);
        else if(Parsing::parse_setting(curLine, "RawInput", &b))
            cv::mouse_raw_input.setValue(b);
        else if(Parsing::parse_setting(curLine, "ConfineMouse", &str)) {
            if(str == "Never") {
                cv::confine_cursor_fullscreen.setValue(false);
                cv::confine_cursor_windowed.setValue(false);
            } else if(str == "Fullscreen") {
                cv::confine_cursor_fullscreen.setValue(true);
                cv::confine_cursor_windowed.setValue(false);
            } else if(str == "Always") {
                cv::confine_cursor_fullscreen.setValue(true);
                cv::confine_cursor_windowed.setValue(true);
            }
        } else if(Parsing::parse_setting(curLine, "HiddenShowFirstApproach", &b))
            cv::show_approach_circle_on_first_hidden_object.setValue(b);
        else if(Parsing::parse_setting(curLine, "ComboColourSliderBall", &b))
            cv::slider_ball_tint_combo_color.setValue(b);
        else if(Parsing::parse_setting(curLine, "Username", &str))
            cv::name.setValue(str);
        else if(Parsing::parse_setting(curLine, "Letterboxing", &b))
            cv::letterboxing.setValue(b);
        else if(Parsing::parse_setting(curLine, "LetterboxPositionX", &flt))
            cv::letterboxing_offset_x.setValue(flt / 100.f);
        else if(Parsing::parse_setting(curLine, "LetterboxPositionY", &flt))
            cv::letterboxing_offset_y.setValue(flt / 100.f);
        else if(Parsing::parse_setting(curLine, "ShowUnicode", &b))
            cv::prefer_cjk.setValue(b);
        else if(Parsing::parse_setting(curLine, "Ticker", &b))
            cv::chat_ticker.setValue(b);
        else if(Parsing::parse_setting(curLine, "keyOsuLeft", &str))
            try_set_key(str, &cv::LEFT_CLICK);
        else if(Parsing::parse_setting(curLine, "keyOsuLeft", &str))
            try_set_key(str, &cv::LEFT_CLICK);
        else if(Parsing::parse_setting(curLine, "keyOsuRight", &str))
            try_set_key(str, &cv::RIGHT_CLICK);
        else if(Parsing::parse_setting(curLine, "keyOsuSmoke", &str))
            try_set_key(str, &cv::SMOKE);
        else if(Parsing::parse_setting(curLine, "keyPause", &str))
            try_set_key(str, &cv::GAME_PAUSE);
        else if(Parsing::parse_setting(curLine, "keySkip", &str))
            try_set_key(str, &cv::SKIP_CUTSCENE);
        else if(Parsing::parse_setting(curLine, "keyToggleScoreboard", &str))
            try_set_key(str, &cv::TOGGLE_SCOREBOARD);
        else if(Parsing::parse_setting(curLine, "keyToggleChat", &str))
            try_set_key(str, &cv::TOGGLE_CHAT);
        else if(Parsing::parse_setting(curLine, "keyToggleExtendedChat \n]", &str))
            try_set_key(str, &cv::TOGGLE_EXTENDED_CHAT);
        else if(Parsing::parse_setting(curLine, "keyScreenshot", &str))
            try_set_key(str, &cv::SAVE_SCREENSHOT);
        else if(Parsing::parse_setting(curLine, "keyIncreaseAudioOffset", &str))
            try_set_key(str, &cv::INCREASE_LOCAL_OFFSET);
        else if(Parsing::parse_setting(curLine, "keyDecreaseAudioOffset", &str))
            try_set_key(str, &cv::DECREASE_LOCAL_OFFSET);
        else if(Parsing::parse_setting(curLine, "keyQuickRetry", &str))
            try_set_key(str, &cv::QUICK_RETRY);
        else if(Parsing::parse_setting(curLine, "keyVolumeIncrease", &str))
            try_set_key(str, &cv::INCREASE_VOLUME);
        else if(Parsing::parse_setting(curLine, "keyVolumeDecrease", &str))
            try_set_key(str, &cv::DECREASE_VOLUME);
        else if(Parsing::parse_setting(curLine, "keyDisableMouseButtons", &str))
            try_set_key(str, &cv::DISABLE_MOUSE_BUTTONS);
        else if(Parsing::parse_setting(curLine, "keyBossKey", &str))
            try_set_key(str, &cv::BOSS_KEY);
        else if(Parsing::parse_setting(curLine, "keyEasy", &str))
            try_set_key(str, &cv::MOD_EASY);
        else if(Parsing::parse_setting(curLine, "keyNoFail", &str))
            try_set_key(str, &cv::MOD_NOFAIL);
        else if(Parsing::parse_setting(curLine, "keyHalfTime", &str))
            try_set_key(str, &cv::MOD_HALFTIME);
        else if(Parsing::parse_setting(curLine, "keyHardRock", &str))
            try_set_key(str, &cv::MOD_HARDROCK);
        else if(Parsing::parse_setting(curLine, "keySuddenDeath", &str))
            try_set_key(str, &cv::MOD_SUDDENDEATH);
        else if(Parsing::parse_setting(curLine, "keyDoubleTime", &str))
            try_set_key(str, &cv::MOD_DOUBLETIME);
        else if(Parsing::parse_setting(curLine, "keyHidden", &str))
            try_set_key(str, &cv::MOD_HIDDEN);
        else if(Parsing::parse_setting(curLine, "keyFlashlight", &str))
            try_set_key(str, &cv::MOD_FLASHLIGHT);
        else if(Parsing::parse_setting(curLine, "keyRelax", &str))
            try_set_key(str, &cv::MOD_RELAX);
        else if(Parsing::parse_setting(curLine, "keyAutopilot", &str))
            try_set_key(str, &cv::MOD_AUTOPILOT);
        else if(Parsing::parse_setting(curLine, "keySpunOut", &str))
            try_set_key(str, &cv::MOD_SPUNOUT);
        else if(Parsing::parse_setting(curLine, "keyAuto", &str))
            try_set_key(str, &cv::MOD_AUTO);
        else if(Parsing::parse_setting(curLine, "keyScoreV2", &str))
            try_set_key(str, &cv::MOD_SCOREV2);
    }
}
}  // namespace PeppyImporter
