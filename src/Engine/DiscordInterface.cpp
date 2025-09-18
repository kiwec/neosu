// Copyright (c) 2018, PG, All rights reserved.
#include "DiscordInterface.h"

#ifndef USE_DISCORD_SDK

void init_discord_sdk() {}
void tick_discord_sdk() {}
void destroy_discord_sdk() {}
void clear_discord_presence() {}
void set_discord_presence(struct DiscordActivity * /*activity*/) {}

#else

static bool initialized = false;

#include "Bancho.h"
#include "BeatmapInterface.h"
#include "ConVar.h"
#include "Engine.h"
#include "Osu.h"
#include "Sound.h"
#include "dynutils.h"

#define DISCORD_CLIENT_ID 1288141291686989846

namespace  // static
{
struct Application {
    struct IDiscordCore *core;
    struct IDiscordUserManager *users;
    struct IDiscordAchievementManager *achievements;
    struct IDiscordActivityManager *activities;
    struct IDiscordRelationshipManager *relationships;
    struct IDiscordApplicationManager *application;
    struct IDiscordLobbyManager *lobbies;
    DiscordUserId user_id;
} dapp{};

struct IDiscordActivityEvents activities_events{};
struct IDiscordRelationshipEvents relationships_events{};
struct IDiscordUserEvents users_events{};

#if !(defined(_WIN32) && !defined(_WIN64))  // doesn't work on winx32
void on_discord_log(void * /*cdata*/, enum EDiscordLogLevel level, const char *message) {
    //(void)cdata;
    if(level == DiscordLogLevel_Error) {
        Engine::logRaw("[Discord] ERROR: {:s}\n", message);
    } else {
        Engine::logRaw("[Discord] {:s}\n", message);
    }
}
#endif

dynutils::lib_obj *discord_handle{nullptr};

}  // namespace

void init_discord_sdk() {
    discord_handle = dynutils::load_lib("discord_game_sdk");
    if(!discord_handle) {
        debugLog("Failed to load Discord SDK! (error {:s})\n", dynutils::get_error());
        return;
    }

    auto pDiscordCreate = dynutils::load_func<decltype(DiscordCreate)>(discord_handle, "DiscordCreate");
    if(!pDiscordCreate) {
        debugLog("Failed to load DiscordCreate from discord_game_sdk.dll! (error {:s})\n", dynutils::get_error());
        return;
    }

    memset(&dapp, 0, sizeof(dapp));
    memset(&activities_events, 0, sizeof(activities_events));
    memset(&relationships_events, 0, sizeof(relationships_events));
    memset(&users_events, 0, sizeof(users_events));

    // users_events.on_current_user_update = OnUserUpdated;
    // relationships_events.on_refresh = OnRelationshipsRefresh;

    struct DiscordCreateParams params{};
    params.client_id = DISCORD_CLIENT_ID;
    params.flags = DiscordCreateFlags_NoRequireDiscord;
    params.event_data = &dapp;
    params.activity_events = &activities_events;
    params.relationship_events = &relationships_events;
    params.user_events = &users_events;

    int res = pDiscordCreate(DISCORD_VERSION, &params, &dapp.core);
    if(res != DiscordResult_Ok) {
        debugLog("Failed to initialize Discord SDK! (error {:d})\n", res);
        return;
    }

#if !(defined(_WIN32) && !defined(_WIN64))
    dapp.core->set_log_hook(dapp.core, DiscordLogLevel_Warn, nullptr, on_discord_log);
#endif
    dapp.activities = dapp.core->get_activity_manager(dapp.core);

    dapp.activities->register_command(dapp.activities, "neosu://run");

    // dapp.users = dapp.core->get_user_manager(dapp.core);
    // dapp.achievements = dapp.core->get_achievement_manager(dapp.core);
    // dapp.application = dapp.core->get_application_manager(dapp.core);
    // dapp.lobbies = dapp.core->get_lobby_manager(dapp.core);
    // dapp.lobbies->connect_lobby_with_activity_secret(dapp.lobbies, "invalid_secret", &app, OnLobbyConnect);
    // dapp.application->get_oauth2_token(dapp.application, &app, OnOAuth2Token);
    // dapp.relationships = dapp.core->get_relationship_manager(dapp.core);

    initialized = true;
}

void tick_discord_sdk() {
    if(!initialized) return;
    dapp.core->run_callbacks(dapp.core);
}

void destroy_discord_sdk() {
    // not doing anything because it will fucking CRASH if you close discord first
    if(discord_handle) {
        dynutils::unload_lib(discord_handle);
    }
}

void clear_discord_presence() {
    if(!initialized) return;

    // TODO @kiwec: test if this works
    struct DiscordActivity activity{};
    memset(&activity, 0, sizeof(activity));
    dapp.activities->update_activity(dapp.activities, &activity, nullptr, nullptr);
}

void set_discord_presence([[maybe_unused]] struct DiscordActivity *activity) {
    if(!initialized) return;

    if(!cv::rich_presence.getBool()) return;

    // activity->type: int
    //     DiscordActivityType_Playing,
    //     DiscordActivityType_Streaming,
    //     DiscordActivityType_Listening,
    //     DiscordActivityType_Watching,

    // activity->details: char[128]; // what the player is doing
    // activity->state:   char[128]; // party status

    // Only set "end" if in multiplayer lobby, else it doesn't make sense since user can pause
    // Keep "start" across retries
    // activity->timestamps->start: int64_t
    // activity->timestamps->end:   int64_t

    // activity->party: DiscordActivityParty
    // activity->secrets: DiscordActivitySecrets
    // currently unused. should be lobby id, etc

    activity->application_id = DISCORD_CLIENT_ID;
    strcpy(&activity->name[0], "neosu");
    strcpy(&activity->assets.large_image[0], "neosu_icon");
    activity->assets.large_text[0] = '\0';
    strcpy(&activity->assets.small_image[0], "None");
    activity->assets.small_text[0] = '\0';

    auto map = osu->active_map->beatmap;
    auto music = osu->active_map->getMusic();
    bool listening = map != nullptr && music != nullptr && music->isPlaying();
    bool playing = map != nullptr && osu->isInPlayMode();
    if(listening || playing) {
        auto url = UString::format("https://assets.ppy.sh/beatmaps/%d/covers/list@2x.jpg?%d", map->getSetID(),
                                   map->getID());
        strncpy(&activity->assets.large_image[0], url.toUtf8(), 127);

        if(BanchoState::server_icon_url.length() > 0 && cv::main_menu_use_server_logo.getBool()) {
            strncpy(&activity->assets.small_image[0], BanchoState::server_icon_url.toUtf8(), 127);
            strncpy(&activity->assets.small_text[0], BanchoState::endpoint.c_str(), 127);
        } else {
            strcpy(&activity->assets.small_image[0], "neosu_icon");
            activity->assets.small_text[0] = '\0';
        }
    }

    dapp.activities->update_activity(dapp.activities, activity, nullptr, nullptr);
}

// void (DISCORD_API *send_request_reply)(struct IDiscordActivityManager* manager, DiscordUserId user_id, enum
//     EDiscordActivityJoinRequestReply reply, void* callback_data, void (DISCORD_API *callback)(void* callback_data,
//     enum EDiscordResult result));

// void (DISCORD_API *send_invite)(struct IDiscordActivityManager* manager, DiscordUserId user_id, enum
//     EDiscordActivityActionType type, const char* content, void* callback_data, void (DISCORD_API *callback)(void*
//     callback_data, enum EDiscordResult result));

// void (DISCORD_API *accept_invite)(struct IDiscordActivityManager* manager, DiscordUserId user_id, void*
//     callback_data, void (DISCORD_API *callback)(void* callback_data, enum EDiscordResult result));

#endif
