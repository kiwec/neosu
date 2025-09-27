// Copyright (c) 2023, kiwec, All rights reserved.

#ifdef _WIN32
#include "WinDebloatDefs.h"
#include <windows.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <cinttypes>

#else
#include <blkid/blkid.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "dynutils.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "BanchoUsers.h"
#include "BeatmapInterface.h"
#include "Chat.h"
#include "ConVar.h"
#include "Engine.h"
#include "Lobby.h"
#include "crypto.h"
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "RankingScreen.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "UIAvatar.h"
#include "UIButton.h"
#include "Timing.h"
#include "Logging.h"
#include "UserCard.h"

// defs
// some of these are atomic due to multithreaded access
std::string BanchoState::endpoint;
std::string BanchoState::username;
MD5Hash BanchoState::pw_md5;

bool BanchoState::is_oauth{false};
u8 BanchoState::oauth_challenge[32]{};
u8 BanchoState::oauth_verifier[32]{};

bool BanchoState::spectating{false};
i32 BanchoState::spectated_player_id{0};
std::vector<u32> BanchoState::spectators;
std::vector<u32> BanchoState::fellow_spectators;

UString BanchoState::server_icon_url;
Image *BanchoState::server_icon{nullptr};

ServerPolicy BanchoState::score_submission_policy{ServerPolicy::NO_PREFERENCE};

UString BanchoState::neosu_version;
UString BanchoState::cho_token{""};
UString BanchoState::user_agent;
UString BanchoState::client_hashes;

Room BanchoState::room;
bool BanchoState::match_started{false};
Slot BanchoState::last_scores[16];

std::unordered_map<std::string, BanchoState::Channel *> BanchoState::chat_channels;

bool BanchoState::print_new_channels{true};
UString BanchoState::disk_uuid;

std::atomic<i32> BanchoState::user_id{0};

/*###################################################################################################*/

namespace proto = BANCHO::Proto;

MD5Hash BanchoState::md5(u8 *msg, size_t msg_len) {
    u8 digest[16];
    crypto::hash::md5(msg, msg_len, &digest[0]);

    MD5Hash out;
    for(int i = 0; i < 16; i++) {
        out.hash[i * 2] = "0123456789abcdef"[digest[i] >> 4];
        out.hash[i * 2 + 1] = "0123456789abcdef"[digest[i] & 0xf];
    }
    out.hash[32] = 0;
    return out;
}

namespace proto = BANCHO::Proto;

void BanchoState::handle_packet(Packet &packet) {
    if(cv::debug_network.getBool()) {
        debugLog("packet id: {}", packet.id);
    }

    switch(packet.id) {
        case USER_ID: {
            i32 new_user_id = proto::read<i32>(packet);
            BanchoState::set_uid(new_user_id);
            osu->getOptionsMenu()->update_login_button();
            osu->getOptionsMenu()->setLoginLoadingState(false);
            BanchoState::is_oauth = !cv::mp_oauth_token.getString().empty();

            if(new_user_id > 0) {
                debugLog("Logged in as user #{:d}.", new_user_id);
                cv::mp_autologin.setValue(true);
                BanchoState::print_new_channels = true;

                std::string avatar_dir = fmt::format(NEOSU_AVATARS_PATH "/{}", BanchoState::endpoint);
                Environment::createDirectory(avatar_dir);

                std::string replays_dir = fmt::format(NEOSU_REPLAYS_PATH "/{}", BanchoState::endpoint);
                Environment::createDirectory(replays_dir);

                osu->onUserCardChange(BanchoState::username.c_str());
                osu->getSongBrowser()->onFilterScoresChange(UString("Global"), SongBrowser::LOGIN_STATE_FILTER_ID);

                // If server sent a score submission policy, update options menu to hide the checkbox
                osu->getOptionsMenu()->scheduleLayoutUpdate();
            } else {
                cv::mp_autologin.setValue(false);
                cv::mp_oauth_token.setValue("");

                debugLog("Failed to log in, server returned code {:d}.", BanchoState::get_uid());
                UString errmsg = UString::fmt("Failed to log in: {} (code {})\n", BanchoState::cho_token.toUtf8(),
                                              BanchoState::get_uid());
                if(new_user_id == -2) {
                    errmsg = "Client version is too old to connect to this server.";
                } else if(new_user_id == -3 || new_user_id == -4) {
                    errmsg = "You are banned from this server.";
                } else if(new_user_id == -5) {
                    errmsg = "Server had an error while trying to log you in.";
                } else if(new_user_id == -6) {
                    errmsg = "You need to buy supporter to connect to this server.";
                } else if(new_user_id == -7) {
                    errmsg = "You need to reset your password to connect to this server.";
                } else if(new_user_id == -8) {
                    if(BanchoState::is_oauth) {
                        errmsg = "osu! session expired, please log in again.";
                    } else {
                        errmsg = "Open the verification link sent to your email, then log in again.";
                    }
                } else {
                    if(BanchoState::cho_token == "user-already-logged-in") {
                        errmsg = "Already logged in on another client.";
                    } else if(BanchoState::cho_token == "unknown-username") {
                        errmsg = UString::fmt("No account by the username '{}' exists.", BanchoState::username);
                    } else if(BanchoState::cho_token == "incorrect-credentials") {
                        errmsg = "Incorrect username/password.";
                    } else if(BanchoState::cho_token == "incorrect-password") {
                        errmsg = "Incorrect password.";
                    } else if(BanchoState::cho_token == "contact-staff") {
                        errmsg = "Please contact an administrator of the server.";
                    }
                }
                osu->getNotificationOverlay()->addToast(errmsg, ERROR_TOAST);
            }
            break;
        }

        case RECV_MESSAGE: {
            UString sender = proto::read_string(packet);
            UString text = proto::read_string(packet);
            UString recipient = proto::read_string(packet);
            i32 sender_id = proto::read<i32>(packet);

            auto msg = ChatMessage{
                .tms = time(nullptr),
                .author_id = sender_id,
                .author_name = sender,
                .text = text,
            };
            osu->getChat()->addMessage(recipient, msg, true);

            break;
        }

        case PONG: {
            // (nothing to do)
            break;
        }

        case USER_STATS: {
            i32 raw_id = proto::read<i32>(packet);
            i32 stats_user_id = abs(raw_id);  // IRC clients are sent with negative IDs, hence the abs()
            auto action = (Action)proto::read<u8>(packet);

            UserInfo *user = BANCHO::User::get_user_info(stats_user_id);
            if(action != user->action) {
                // TODO @kiwec: i think client is supposed to regularly poll for friend stats
                if(user->is_friend() && cv::notify_friend_status_change.getBool() && action < NB_ACTIONS) {
                    static constexpr auto actions = std::array{
                        "idle",         "afk",           "playing", "editing",    "modding", "in a multiplayer lobby",
                        "spectating",   "vibing",        "testing", "submitting", "pausing", "testing",
                        "multiplaying", "browsing maps",
                    };
                    auto text = UString::format("%s is now %s", user->name.toUtf8(), actions[action]);
                    auto open_dms = [uid = stats_user_id]() -> void {
                        UserInfo *user = BANCHO::User::get_user_info(uid);
                        osu->getChat()->openChannel(user->name);
                    };

                    // TODO: figure out what stable does and do that. for now just throttling to avoid endless spam
                    if(user->stats_tms + 10000 < Timing::getTicksMS() && action != SUBMITTING) {
                        osu->getNotificationOverlay()->addToast(text, STATUS_TOAST, open_dms, ToastElement::TYPE::CHAT);
                    }
                }
            }

            user->irc_user = raw_id < 0;
            user->stats_tms = Timing::getTicksMS();
            user->action = action;
            user->info_text = proto::read_string(packet);
            user->map_md5 = proto::read_hash(packet);
            user->mods = proto::read<u32>(packet);
            user->mode = (GameMode)proto::read<u8>(packet);
            user->map_id = proto::read<i32>(packet);
            user->ranked_score = proto::read<i64>(packet);
            user->accuracy = proto::read<f32>(packet);
            user->plays = proto::read<i32>(packet);
            user->total_score = proto::read<i64>(packet);
            user->global_rank = proto::read<i32>(packet);
            user->pp = proto::read<u16>(packet);

            if(stats_user_id == BanchoState::get_uid()) {
                osu->getUserButton()->updateUserStats();
            }
            if(stats_user_id == BanchoState::spectated_player_id) {
                osu->getSpectatorScreen()->userCard->updateUserStats();
            }

            osu->getChat()->updateUserList();

            break;
        }

        case USER_LOGOUT: {
            i32 logged_out_id = proto::read<i32>(packet);
            proto::read<u8>(packet);
            if(logged_out_id == BanchoState::get_uid()) {
                debugLog("Logged out.");
                BanchoState::disconnect();
            } else {
                BANCHO::User::logout_user(logged_out_id);
            }
            break;
        }

        case SPECTATOR_JOINED: {
            i32 spectator_id = proto::read<i32>(packet);
            if(std::ranges::find(BanchoState::spectators, spectator_id) == BanchoState::spectators.end()) {
                debugLog("Spectator joined: user id {:d}", spectator_id);
                BanchoState::spectators.push_back(spectator_id);
            }

            break;
        }

        case SPECTATOR_LEFT: {
            i32 spectator_id = proto::read<i32>(packet);
            auto it = std::ranges::find(BanchoState::spectators, spectator_id);
            if(it != BanchoState::spectators.end()) {
                debugLog("Spectator left: user id {:d}", spectator_id);
                BanchoState::spectators.erase(it);
            }

            break;
        }

        case IN_SPECTATE_FRAMES: {
            i32 extra = proto::read<i32>(packet);
            (void)extra;  // this is mania seed or something we can't use

            if(BanchoState::spectating) {
                UserInfo *info = BANCHO::User::get_user_info(BanchoState::spectated_player_id, true);

                u16 nb_frames = proto::read<u16>(packet);
                for(u16 i = 0; i < nb_frames; i++) {
                    auto frame = proto::read<LiveReplayFrame>(packet);

                    if(frame.mouse_x < 0 || frame.mouse_x > 512 || frame.mouse_y < 0 || frame.mouse_y > 384) {
                        debugLog("WEIRD FRAME: time {:d}, x {:f}, y {:f}", frame.time, frame.mouse_x, frame.mouse_y);
                    }

                    osu->getMapInterface()->spectated_replay.push_back(LegacyReplay::Frame{
                        .cur_music_pos = frame.time,
                        .milliseconds_since_last_frame = 0,  // fixed below
                        .x = frame.mouse_x,
                        .y = frame.mouse_y,
                        .key_flags = frame.key_flags,
                    });
                }

                // NOTE: Server can send frames in the wrong order. So we're correcting it here.
                std::ranges::sort(
                    osu->getMapInterface()->spectated_replay,
                    [](LegacyReplay::Frame a, LegacyReplay::Frame b) { return a.cur_music_pos < b.cur_music_pos; });
                osu->getMapInterface()->last_frame_ms = 0;
                for(auto &frame : osu->getMapInterface()->spectated_replay) {
                    frame.milliseconds_since_last_frame = frame.cur_music_pos - osu->getMapInterface()->last_frame_ms;
                    osu->getMapInterface()->last_frame_ms = frame.cur_music_pos;
                }

                auto action = (LiveReplayBundle::Action)proto::read<u8>(packet);
                info->spec_action = action;

                if(osu->isInPlayMode()) {
                    if(action == LiveReplayBundle::Action::SONG_SELECT) {
                        info->map_id = 0;
                        info->map_md5 = MD5Hash();
                        osu->getMapInterface()->stop(true);
                    }
                    if(action == LiveReplayBundle::Action::UNPAUSE) {
                        osu->getMapInterface()->spectate_pause = false;
                    }
                    if(action == LiveReplayBundle::Action::PAUSE) {
                        osu->getMapInterface()->spectate_pause = true;
                    }
                    if(action == LiveReplayBundle::Action::SKIP) {
                        osu->getMapInterface()->skipEmptySection();
                    }
                    if(action == LiveReplayBundle::Action::FAIL) {
                        osu->getMapInterface()->fail(true);
                    }
                    if(action == LiveReplayBundle::Action::NEW_SONG) {
                        osu->getRankingScreen()->setVisible(false);
                        osu->getMapInterface()->restart(true);
                        osu->getMapInterface()->update();
                    }
                }

                auto score_frame = proto::read<ScoreFrame>(packet);
                osu->getMapInterface()->score_frames.push_back(score_frame);

                auto sequence = proto::read<u16>(packet);
                (void)sequence;  // don't know how to use this
            }

            break;
        }

        case VERSION_UPDATE: {
            // (nothing to do)
            break;
        }

        case SPECTATOR_CANT_SPECTATE: {
            i32 spectator_id = proto::read<i32>(packet);
            debugLog("Spectator can't spectate: user id {:d}", spectator_id);
            break;
        }

        case GET_ATTENTION: {
            // (nothing to do)
            break;
        }

        case NOTIFICATION: {
            UString notification = proto::read_string(packet);
            osu->getNotificationOverlay()->addToast(notification, INFO_TOAST);
            break;
        }

        case ROOM_UPDATED: {
            auto room = Room(packet);
            if(osu->getLobby()->isVisible()) {
                osu->getLobby()->updateRoom(room);
            } else if(room.id == BanchoState::room.id) {
                osu->getRoom()->on_room_updated(room);
            }

            break;
        }

        case ROOM_CREATED: {
            auto room = new Room(packet);
            osu->getLobby()->addRoom(room);
            break;
        }

        case ROOM_CLOSED: {
            i32 room_id = proto::read<i32>(packet);
            osu->getLobby()->removeRoom(room_id);
            break;
        }

        case ROOM_JOIN_SUCCESS: {
            // Sanity, in case some trolley admins do funny business
            if(BanchoState::spectating) {
                stop_spectating();
            }
            if(osu->isInPlayMode()) {
                osu->getMapInterface()->stop(true);
            }

            auto room = Room(packet);
            osu->getRoom()->on_room_joined(room);

            break;
        }

        case ROOM_JOIN_FAIL: {
            osu->getNotificationOverlay()->addToast("Failed to join room.", ERROR_TOAST);
            osu->getLobby()->on_room_join_failed();
            break;
        }

        case FELLOW_SPECTATOR_JOINED: {
            i32 spectator_id = proto::read<i32>(packet);
            if(std::ranges::find(BanchoState::fellow_spectators, spectator_id) ==
               BanchoState::fellow_spectators.end()) {
                debugLog("Fellow spectator joined: user id {:d}", spectator_id);
                BanchoState::fellow_spectators.push_back(spectator_id);
            }

            break;
        }

        case FELLOW_SPECTATOR_LEFT: {
            i32 spectator_id = proto::read<i32>(packet);
            auto it = std::ranges::find(BanchoState::fellow_spectators, spectator_id);
            if(it != BanchoState::fellow_spectators.end()) {
                debugLog("Fellow spectator left: user id {:d}", spectator_id);
                BanchoState::fellow_spectators.erase(it);
            }

            break;
        }

        case MATCH_STARTED: {
            auto room = Room(packet);
            osu->getRoom()->on_match_started(room);
            break;
        }

        case MATCH_SCORE_UPDATED: {
            osu->getRoom()->on_match_score_updated(packet);
            break;
        }

        case HOST_CHANGED: {
            // (nothing to do)
            break;
        }

        case MATCH_ALL_PLAYERS_LOADED: {
            osu->getRoom()->on_all_players_loaded();
            break;
        }

        case MATCH_PLAYER_FAILED: {
            i32 slot_id = proto::read<i32>(packet);
            osu->getRoom()->on_player_failed(slot_id);
            break;
        }

        case MATCH_FINISHED: {
            osu->getRoom()->on_match_finished();
            break;
        }

        case MATCH_SKIP: {
            osu->getRoom()->on_all_players_skipped();
            break;
        }

        case CHANNEL_JOIN_SUCCESS: {
            UString name = proto::read_string(packet);
            auto msg = ChatMessage{
                .tms = time(nullptr),
                .author_id = 0,
                .author_name = UString(""),
                .text = UString("Joined channel."),
            };
            osu->getChat()->addChannel(name, true);
            osu->getChat()->addMessage(name, msg, false);
            break;
        }

        case CHANNEL_INFO: {
            UString channel_name = proto::read_string(packet);
            UString channel_topic = proto::read_string(packet);
            i32 nb_members = proto::read<i32>(packet);
            BanchoState::update_channel(channel_name, channel_topic, nb_members, false);
            break;
        }

        case LEFT_CHANNEL: {
            UString name = proto::read_string(packet);
            osu->getChat()->removeChannel(name);
            break;
        }

        case CHANNEL_AUTO_JOIN: {
            UString channel_name = proto::read_string(packet);
            UString channel_topic = proto::read_string(packet);
            i32 nb_members = proto::read<i32>(packet);
            BanchoState::update_channel(channel_name, channel_topic, nb_members, true);
            break;
        }

        case PRIVILEGES: {
            proto::read<u32>(packet);  // not using it for anything
            break;
        }

        case FRIENDS_LIST: {
            BANCHO::User::friends.clear();

            u16 nb_friends = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_friends; i++) {
                i32 friend_id = proto::read<i32>(packet);
                BANCHO::User::friends.push_back(friend_id);
            }
            break;
        }

        case PROTOCOL_VERSION: {
            int protocol_version = proto::read<i32>(packet);
            if(protocol_version != 19) {
                osu->getNotificationOverlay()->addToast("This server may use an unsupported protocol version.",
                                                        ERROR_TOAST);
            }
            break;
        }

        case MAIN_MENU_ICON: {
            UString icon = proto::read_string(packet);
            auto urls = icon.split("|");
            if(urls.size() == 2 && ((urls[0].startsWith("http://")) || urls[0].startsWith("https://"))) {
                BanchoState::server_icon_url = urls[0];
            }
            break;
        }

        case MATCH_PLAYER_SKIPPED: {
            i32 user_id = proto::read<i32>(packet);
            osu->getRoom()->on_player_skip(user_id);
            break;
        }

        case USER_PRESENCE: {
            i32 raw_id = proto::read<i32>(packet);
            i32 presence_user_id = abs(raw_id);  // IRC clients are sent with negative IDs, hence the abs()
            auto presence_username = proto::read_string(packet);

            UserInfo *user = BANCHO::User::get_user_info(presence_user_id);
            user->irc_user = raw_id < 0;
            user->has_presence = true;
            user->name = presence_username;
            user->utc_offset = proto::read<u8>(packet);
            user->country = proto::read<u8>(packet);
            user->privileges = proto::read<u8>(packet);
            user->longitude = proto::read<f32>(packet);
            user->latitude = proto::read<f32>(packet);
            user->global_rank = proto::read<i32>(packet);

            BANCHO::User::login_user(presence_user_id);

            // Server can decide what username we use
            if(presence_user_id == BanchoState::get_uid()) {
                BanchoState::username = presence_username.toUtf8();
                osu->onUserCardChange(presence_username.utf8View());
            }

            osu->getChat()->updateUserList();
            break;
        }

        case RESTART: {
            // XXX: wait 'ms' milliseconds before reconnecting
            i32 ms = proto::read<i32>(packet);
            (void)ms;

            // Some servers send "restart" packets when password is incorrect
            // So, don't retry unless actually logged in
            if(BanchoState::is_online()) {
                BanchoState::reconnect();
            }
            break;
        }

        case ROOM_INVITE: {
            break;
        }

        case CHANNEL_INFO_END: {
            BanchoState::print_new_channels = false;
            break;
        }

        case ROOM_PASSWORD_CHANGED: {
            UString new_password = proto::read_string(packet);
            debugLog("Room changed password to {:s}", new_password.toUtf8());
            BanchoState::room.password = new_password;
            break;
        }

        case SILENCE_END: {
            i32 delta = proto::read<i32>(packet);
            debugLog("Silence ends in {:d} seconds.", delta);
            // XXX: Prevent user from sending messages while silenced
            break;
        }

        case USER_SILENCED: {
            i32 user_id = proto::read<i32>(packet);
            debugLog("User #{:d} silenced.", user_id);
            break;
        }

        case USER_PRESENCE_SINGLE: {
            i32 user_id = proto::read<i32>(packet);
            BANCHO::User::login_user(user_id);
            break;
        }

        case USER_PRESENCE_BUNDLE: {
            u16 nb_users = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_users; i++) {
                i32 user_id = proto::read<i32>(packet);
                BANCHO::User::login_user(user_id);
            }
            break;
        }

        case USER_DM_BLOCKED: {
            proto::read_string(packet);
            proto::read_string(packet);
            UString blocked = proto::read_string(packet);
            proto::read<u32>(packet);
            debugLog("Blocked {:s}.", blocked.toUtf8());
            break;
        }

        case TARGET_IS_SILENCED: {
            proto::read_string(packet);
            proto::read_string(packet);
            UString blocked = proto::read_string(packet);
            proto::read<u32>(packet);
            debugLog("Silenced {:s}.", blocked.toUtf8());
            break;
        }

        case VERSION_UPDATE_FORCED: {
            BanchoState::disconnect();
            osu->getNotificationOverlay()->addToast("This server requires a newer client version.", ERROR_TOAST);
            break;
        }

        case SWITCH_SERVER: {
            break;
        }

        case ACCOUNT_RESTRICTED: {
            osu->getNotificationOverlay()->addToast("Account restricted.", ERROR_TOAST);
            BanchoState::disconnect();
            break;
        }

        case MATCH_ABORT: {
            osu->getRoom()->on_match_aborted();
            break;
        }

            // neosu-specific below

        case PROTECT_VARIABLES: {
            u16 nb_variables = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_variables; i++) {
                auto name = proto::read_stdstring(packet);
                auto cvar = cvars->getConVarByName(name, false);
                if(cvar) {
                    cvar->setServerProtected(ConVar::ProtectionPolicy::PROTECTED);
                } else {
                    debugLog("Server wanted to protect cvar '{}', but it doesn't exist!", name);
                }
            }

            break;
        }

        case UNPROTECT_VARIABLES: {
            u16 nb_variables = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_variables; i++) {
                auto name = proto::read_stdstring(packet);
                auto cvar = cvars->getConVarByName(name, false);
                if(cvar) {
                    cvar->setServerProtected(ConVar::ProtectionPolicy::UNPROTECTED);
                } else {
                    debugLog("Server wanted to unprotect cvar '{}', but it doesn't exist!", name);
                }
            }

            break;
        }

        case FORCE_VALUES: {
            u16 nb_variables = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_variables; i++) {
                auto name = proto::read_stdstring(packet);
                auto val = proto::read_stdstring(packet);
                auto cvar = cvars->getConVarByName(name, false);
                if(cvar) {
                    cvar->setValue(val, true, ConVar::CvarEditor::SERVER);
                } else {
                    debugLog("Server wanted to set cvar '{}' to '{}', but it doesn't exist!", name, val);
                }
            }

            break;
        }

        // this should at least be in ConVarHandler...
        case RESET_VALUES: {
            u16 nb_variables = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_variables; i++) {
                auto name = proto::read_stdstring(packet);
                auto cvar = cvars->getConVarByName(name, false);
                if(cvar) {
                    cvar->hasServerValue.store(false, std::memory_order_release);
                } else {
                    debugLog("Server wanted to reset cvar '{}', but it doesn't exist!", name);
                }
            }

            break;
        }

        case REQUEST_MAP: {
            auto md5 = proto::read_hash(packet);

            // Load map (XXX: blocking)
            auto map = db->getBeatmapDifficulty(md5);
            if(!map) {
                // Incredibly rare, but this can happen if you enter song browser
                // on a difficulty the server doesn't have, then instantly refresh.
                debugLog("Server requested difficulty {} but we don't have it!", md5.string());
                break;
            }

            auto osu_file = map->getMapFile();
            auto md5_check = BanchoState::md5((u8 *)osu_file.c_str(), osu_file.length());
            if(md5 != md5_check) {
                debugLog("After loading map {}, we got different md5 {}!", md5.string(), md5_check.string());
                break;
            }

            // Submit map
            auto scheme = cv::use_https.getBool() ? "https://" : "http://";
            auto url = UString::fmt("{}osu.{}/web/neosu-submit-map.php", scheme, BanchoState::endpoint);
            url.append(UString::fmt("?hash={}", md5.string()));
            BANCHO::Net::append_auth_params(url);

            NetworkHandler::RequestOptions options;
            options.timeout = 60;
            options.connectTimeout = 5;
            options.userAgent = "osu!";
            options.mimeParts.push_back({
                .filename = fmt::format("{}.osu", md5.string()),
                .name = "osu_file",
                .data = {osu_file.begin(), osu_file.end()},
            });
            networkHandler->httpRequestAsync(url, [](const NetworkHandler::Response & /*response*/) {}, options);

            break;
        }

        default: {
            debugLog("Unknown packet ID {:d} ({:d} bytes)!", packet.id, packet.size);
            break;
        }
    }
}

Packet BanchoState::build_login_packet() {
    // Request format:
    // username\npasswd_md5\nosu_version|utc_offset|display_city|client_hashes|pm_private\n
    std::string req;

    if(cv::mp_oauth_token.getString().empty()) {
        req.append(BanchoState::username);
        req.append("\n");
        req.append(BanchoState::pw_md5.string(), 32);
        req.append("\n");
    } else {
        req.append("$oauth");
        req.append("\n");
        req.append(cv::mp_oauth_token.getString());
        req.append("\n");
    }

    // OSU_VERSION is something like "b20200201.2"
    req.append(OSU_VERSION "|");

    // UTC offset
    time_t now = time(nullptr);
    struct tm tmbuf;
    auto gmt = gmtime_x(&now, &tmbuf);
    auto local_time = localtime_x(&now, &tmbuf);
    i32 utc_offset = difftime(mktime(local_time), mktime(gmt)) / 3600;
    if(utc_offset < 0) {
        req.append("-");
        utc_offset *= -1;
    }
    req.push_back('0' + utc_offset);
    req.append("|");

    // Don't dox the user's city
    req.append("0|");

    const char *osu_path = Environment::getPathToSelf().c_str();
    MD5Hash osu_path_md5 = md5((u8 *)osu_path, strlen(osu_path));

    // XXX: Should get MAC addresses from network adapters
    // NOTE: Not sure how the MD5 is computed - does it include final "." ?
    const char *adapters = "runningunderwine";
    MD5Hash adapters_md5 = md5((u8 *)adapters, strlen(adapters));

    // XXX: Should remove '|' from the disk UUID just to be safe
    MD5Hash disk_md5 = md5((u8 *)BanchoState::get_disk_uuid().toUtf8(), BanchoState::get_disk_uuid().lengthUtf8());

    // XXX: Not implemented, I'm lazy so just reusing disk signature
    MD5Hash install_md5 = md5((u8 *)BanchoState::get_install_id().toUtf8(), BanchoState::get_install_id().lengthUtf8());

    BanchoState::client_hashes = UString::fmt("{:s}:{:s}:{:s}:{:s}:{:s}:", osu_path_md5.string(), adapters,
                                              adapters_md5.string(), install_md5.string(), disk_md5.string());

    req.append(BanchoState::client_hashes.toUtf8());
    req.append("|");

    // Allow PMs from strangers
    req.append("0\n");

    Packet packet;
    proto::write_bytes(packet, (u8 *)req.c_str(), req.length());
    return packet;
}

std::string BanchoState::get_username() {
    if(BanchoState::is_online()) {
        return BanchoState::username;
    } else {
        return cv::name.getString();
    }
}

bool BanchoState::can_submit_scores() {
    if(!BanchoState::is_online()) {
        return false;
    } else if(BanchoState::score_submission_policy == ServerPolicy::NO_PREFERENCE) {
        return cv::submit_scores.getBool();
    } else if(BanchoState::score_submission_policy == ServerPolicy::YES) {
        return true;
    } else {
        return false;
    }
}

void BanchoState::update_channel(const UString &name, const UString &topic, i32 nb_members, bool join) {
    Channel *chan{nullptr};
    auto name_str = std::string(name.toUtf8());
    auto it = BanchoState::chat_channels.find(name_str);
    if(it == BanchoState::chat_channels.end()) {
        chan = new Channel();
        chan->name = name;
        BanchoState::chat_channels[name_str] = chan;

        if(BanchoState::print_new_channels) {
            auto msg = ChatMessage{
                .tms = time(nullptr),
                .author_id = 0,
                .author_name = UString(""),
                .text = UString::format("%s: %s", name.toUtf8(), topic.toUtf8()),
            };
            osu->getChat()->addMessage(BanchoState::is_oauth ? "#neosu" : "#osu", msg, false);
        }
    } else {
        chan = it->second;
    }

    if(join) {
        osu->getChat()->join(name);
    }

    if(chan) {
        chan->topic = topic;
        chan->nb_members = nb_members;
    } else {
        debugLog("WARNING: no channel found??");
    }
}

const UString &BanchoState::get_disk_uuid() {
    static bool once = false;
    if(!once) {
        once = true;
        if constexpr(Env::cfg(OS::WINDOWS)) {
            BanchoState::disk_uuid = get_disk_uuid_win32();
        } else if constexpr(Env::cfg(OS::LINUX)) {
            BanchoState::disk_uuid = get_disk_uuid_blkid();
        } else {
            BanchoState::disk_uuid = "error getting disk uuid (unsupported platform)";
        }
    }
    return BanchoState::disk_uuid;
}

UString BanchoState::get_disk_uuid_blkid() {
    UString w_uuid{"error getting disk UUID"};
#ifdef MCENGINE_PLATFORM_LINUX
    using namespace dynutils;

    // we are only called once, only need libblkid temporarily
    lib_obj *blkid_lib = load_lib("libblkid.so.1");
    if(!blkid_lib) {
        debugLog("error loading blkid for obtaining disk UUID: {}", get_error());
        return w_uuid;
    }

    auto pblkid_devno_to_devname = load_func<decltype(blkid_devno_to_devname)>(blkid_lib, "blkid_devno_to_devname");
    auto pblkid_get_cache = load_func<decltype(blkid_get_cache)>(blkid_lib, "blkid_get_cache");
    auto pblkid_put_cache = load_func<decltype(blkid_put_cache)>(blkid_lib, "blkid_put_cache");
    auto pblkid_get_tag_value = load_func<decltype(blkid_get_tag_value)>(blkid_lib, "blkid_get_tag_value");

    if(!(pblkid_devno_to_devname && pblkid_get_cache && pblkid_put_cache && pblkid_get_tag_value)) {
        debugLog("error loading blkid functions for obtaining disk UUID: {}", get_error());
        unload_lib(blkid_lib);
        return w_uuid;
    }

    const std::string &exe_path = Environment::getPathToSelf();

    // get the device number of the device the current exe is running from
    struct stat st{};
    if(stat(exe_path.c_str(), &st) != 0) {
        unload_lib(blkid_lib);
        return w_uuid;
    }

    char *devname = pblkid_devno_to_devname(st.st_dev);
    if(!devname) {
        unload_lib(blkid_lib);
        return w_uuid;
    }

    // get the UUID of that device
    blkid_cache cache = nullptr;
    char *uuid = nullptr;

    if(pblkid_get_cache(&cache, nullptr) == 0) {
        uuid = pblkid_get_tag_value(cache, "UUID", devname);
        pblkid_put_cache(cache);
    }

    if(uuid) {
        w_uuid = UString{uuid};
        free(uuid);
    }

    free(devname);
    unload_lib(blkid_lib);
#endif
    return w_uuid;
}

UString BanchoState::get_disk_uuid_win32() {
    UString w_uuid{"error getting disk UUID"};
#ifdef MCENGINE_PLATFORM_WINDOWS

    // get the path to the executable
    const std::string &exe_path = Environment::getPathToSelf();
    if(exe_path.empty()) {
        return w_uuid;
    }

    int w_exe_len = MultiByteToWideChar(CP_UTF8, 0, exe_path.c_str(), -1, NULL, 0);
    if(w_exe_len == 0) {
        return w_uuid;
    }

    std::vector<wchar_t> w_exe_path(w_exe_len);
    if(MultiByteToWideChar(CP_UTF8, 0, exe_path.c_str(), -1, w_exe_path.data(), w_exe_len) == 0) {
        return w_uuid;
    }

    // get the volume path for the executable
    std::array<wchar_t, MAX_PATH> volume_path{};
    if(!GetVolumePathNameW(w_exe_path.data(), volume_path.data(), MAX_PATH)) {
        return w_uuid;
    }

    // get volume GUID path
    std::array<wchar_t, MAX_PATH> volume_name{};
    if(GetVolumeNameForVolumeMountPointW(volume_path.data(), volume_name.data(), MAX_PATH)) {
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, volume_name.data(), -1, NULL, 0, NULL, NULL);
        if(utf8_size > 0) {
            std::vector<char> utf8_buffer(utf8_size);
            if(WideCharToMultiByte(CP_UTF8, 0, volume_name.data(), -1, utf8_buffer.data(), utf8_size, NULL, NULL) > 0) {
                std::string volume_guid(utf8_buffer.data());

                // get the GUID from the path (i.e. \\?\Volume{GUID}\)
                size_t start = volume_guid.find('{');
                size_t end = volume_guid.find('}');
                if(start != std::string::npos && end != std::string::npos && end > start) {
                    // return just the GUID part without braces
                    w_uuid = UString{volume_guid.substr(start + 1, end - start - 1)};
                } else {
                    // use the entire volume GUID path as a fallback
                    if(volume_guid.length() > 12) {
                        w_uuid = UString{volume_guid};
                    }
                }
            }
        }
    } else {  // the above might fail under Wine, this should work well enough as a fallback
        std::array<wchar_t, 4> drive_root{};  // "C:\" + null
        if(volume_path[0] != L'\0' && volume_path[1] == L':') {
            drive_root[0] = volume_path[0];
            drive_root[1] = L':';
            drive_root[2] = L'\\';
            drive_root[3] = L'\0';

            u32 volume_serial = 0;
            if(GetVolumeInformationW(drive_root.data(), NULL, 0, (DWORD *)(&volume_serial), NULL, NULL, NULL, 0)) {
                // format volume serial as hex string
                std::array<char, 16> serial_buffer{};
                snprintf(serial_buffer.data(), serial_buffer.size(), "%08x", volume_serial);
                w_uuid = UString{serial_buffer.data()};
            }
        }
    }

#endif
    return w_uuid;
}
