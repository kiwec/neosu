// Copyright (c) 2018, PG, All rights reserved.
#include "RichPresence.h"

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "BeatmapInterface.h"
#include "Chat.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "DiscordInterface.h"
#include "Engine.h"
#include "Environment.h"
#include "ModSelector.h"
#include "Osu.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "Sound.h"
#include "score.h"

#include <chrono>

const UString RichPresence::KEY_DISCORD_STATUS = "state";
const UString RichPresence::KEY_DISCORD_DETAILS = "details";

UString last_status = "[neosu]\nWaking up";
Action last_action = IDLE;

void crop_to(const UString& str, char* output, int max_len) {
    if(str.lengthUtf8() < max_len) {
        strcpy(output, str.toUtf8());
    } else {
        strncpy(output, str.toUtf8(), max_len - 4);
        output[max_len - 4] = '.';
        output[max_len - 3] = '.';
        output[max_len - 2] = '.';
        output[max_len - 1] = '\0';
    }
}

// output is assumed to be a char[128] string
void mapstr(DatabaseBeatmap* map, char* output, bool /*include_difficulty*/) {
    if(map == nullptr) {
        strcpy(output, "No map selected");
        return;
    }

    UString playingInfo;
    playingInfo.append(map->getArtist().c_str());
    playingInfo.append(" - ");
    playingInfo.append(map->getTitle().c_str());

    auto diffStr = UString::format(" [%s]", map->getDifficultyName().c_str());
    if(playingInfo.lengthUtf8() + diffStr.lengthUtf8() < 128) {
        playingInfo.append(diffStr);
    }

    crop_to(playingInfo, output, 128);
}

void RichPresence::setBanchoStatus(const char* info_text, Action action) {
    if(osu == nullptr) return;

    MD5Hash map_md5("");
    i32 map_id = 0;

    auto map = osu->getMapInterface()->beatmap;
    if(map != nullptr) {
        map_md5 = map->getMD5Hash();
        map_id = map->getID();
    }

    char fancy_text[1024] = {0};
    snprintf(fancy_text, 1023, "[neosu]\n%s", info_text);

    last_status = fancy_text;
    last_action = action;

    Packet packet;
    packet.id = CHANGE_ACTION;
    BANCHO::Proto::write<u8>(packet, action);
    BANCHO::Proto::write_string(packet, fancy_text);
    BANCHO::Proto::write_string(packet, map_md5.hash.data());
    BANCHO::Proto::write<u32>(packet, osu->getModSelector()->getModFlags());
    BANCHO::Proto::write<u8>(packet, 0);  // osu!std
    BANCHO::Proto::write<i32>(packet, map_id);
    BANCHO::Net::send_packet(packet);
}

void RichPresence::updateBanchoMods() {
    MD5Hash map_md5("");
    i32 map_id = 0;

    auto diff = osu->getMapInterface()->beatmap;
    if(diff != nullptr) {
        map_md5 = diff->getMD5Hash();
        map_id = diff->getID();
    }

    Packet packet;
    packet.id = CHANGE_ACTION;
    BANCHO::Proto::write<u8>(packet, last_action);
    BANCHO::Proto::write_string(packet, last_status.toUtf8());
    BANCHO::Proto::write_string(packet, map_md5.hash.data());
    BANCHO::Proto::write<u32>(packet, osu->getModSelector()->getModFlags());
    BANCHO::Proto::write<u8>(packet, 0);  // osu!std
    BANCHO::Proto::write<i32>(packet, map_id);
    BANCHO::Net::send_packet(packet);

    // Servers like akatsuki send different leaderboards based on what mods
    // you have selected. Reset leaderboard when switching mods.
    db->online_scores.clear();
    osu->getSongBrowser()->rebuildScoreButtons();
}

void RichPresence::onMainMenu() {
    bool force_not_afk = BanchoState::spectating || (osu->getChat()->isVisible() && osu->getChat()->user_list->isVisible());
    setBanchoStatus("Main Menu", force_not_afk ? IDLE : AFK);

    // NOTE: As much as I would like to show "Listening to", the Discord SDK ignores the activity 'type'
    struct DiscordActivity activity {};

    activity.type = DiscordActivityType_Listening;

    auto map = osu->getMapInterface()->beatmap;
    auto music = osu->getMapInterface()->getMusic();
    bool listening = map != nullptr && music != nullptr && music->isPlaying();
    if(listening) {
        mapstr(map, activity.details, false);
    }

    strcpy(activity.state, "Main menu");
    set_discord_presence(&activity);
}

void RichPresence::onSongBrowser() {
    struct DiscordActivity activity {};

    activity.type = DiscordActivityType_Playing;
    strcpy(activity.details, "Picking a map");

    if(osu->getRoom()->isVisible()) {
        setBanchoStatus("Picking a map", MULTIPLAYER);

        strcpy(activity.state, "Multiplayer");
        activity.party.size.current_size = BanchoState::room.nb_players;
        activity.party.size.max_size = BanchoState::room.nb_open_slots;
    } else {
        setBanchoStatus("Song selection", IDLE);

        strcpy(activity.state, "Singleplayer");
        activity.party.size.current_size = 0;
        activity.party.size.max_size = 0;
    }

    set_discord_presence(&activity);
    env->setWindowTitle("neosu");
}

void RichPresence::onPlayStart() {
    auto map = osu->getMapInterface()->beatmap;

    static DatabaseBeatmap* last_diff = nullptr;
    static int64_t tms = 0;
    if(tms == 0 || last_diff != map) {
        last_diff = map;
        tms = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();
    }

    struct DiscordActivity activity {};

    activity.type = DiscordActivityType_Playing;
    activity.timestamps.start = tms;
    activity.timestamps.end = 0;

    mapstr(map, activity.details, true);

    if(BanchoState::is_in_a_multi_room()) {
        setBanchoStatus(activity.details, MULTIPLAYER);

        strcpy(activity.state, "Playing in a lobby");
        activity.party.size.current_size = BanchoState::room.nb_players;
        activity.party.size.max_size = BanchoState::room.nb_open_slots;
    } else if(BanchoState::spectating) {
        setBanchoStatus(activity.details, WATCHING);
        activity.party.size.current_size = 0;
        activity.party.size.max_size = 0;

        auto user = BANCHO::User::get_user_info(BanchoState::spectated_player_id, true);
        if(user->has_presence) {
            snprintf(activity.state, 128, "Spectating %s", user->name.toUtf8());
        } else {
            strcpy(activity.state, "Spectating");
        }
    } else {
        setBanchoStatus(activity.details, PLAYING);

        strcpy(activity.state, "Playing Solo");
        activity.party.size.current_size = 0;
        activity.party.size.max_size = 0;
    }

    // also update window title
    auto windowTitle = UString::format("neosu - %s", activity.details);
    env->setWindowTitle(windowTitle);

    set_discord_presence(&activity);
}

void RichPresence::onPlayEnd(bool quit) {
    if(quit) return;

    // e.g.: 230pp 900x 95.50% HDHRDT 6*

    // pp
    UString scoreInfo = UString::format("%ipp", (int)(std::round(osu->getScore()->getPPv2())));

    // max combo
    scoreInfo.append(UString::format(" %ix", osu->getScore()->getComboMax()));

    // accuracy
    scoreInfo.append(UString::format(" %.2f%%", osu->getScore()->getAccuracy() * 100.0f));

    // mods
    UString mods = osu->getScore()->getModsStringForRichPresence();
    if(mods.length() > 0) {
        scoreInfo.append(" ");
        scoreInfo.append(mods);
    }

    // stars
    scoreInfo.append(UString::format(" %.2f*", osu->getScore()->getStarsTomTotal()));

    setBanchoStatus(scoreInfo.toUtf8(), SUBMITTING);
}

void RichPresence::onMultiplayerLobby() {
    struct DiscordActivity activity {};

    activity.type = DiscordActivityType_Playing;

    crop_to(BanchoState::endpoint.c_str(), activity.state, 128);
    crop_to(BanchoState::room.name.toUtf8(), activity.details, 128);
    activity.party.size.current_size = BanchoState::room.nb_players;
    activity.party.size.max_size = BanchoState::room.nb_open_slots;

    set_discord_presence(&activity);
}

void RichPresence::onRichPresenceChange(float oldValue, float newValue) {
    if(oldValue != newValue && !static_cast<int>(newValue)) {
        clear_discord_presence();
    }
}
