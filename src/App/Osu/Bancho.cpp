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
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "BanchoUsers.h"
#include "Beatmap.h"
#include "Chat.h"
#include "ConVar.h"
#include "Engine.h"
#include "Lobby.h"
#include "crypto.h"
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
#include "UserCard.h"

// static func
MD5Hash Bancho::md5(u8 *msg, size_t msg_len) {
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

void Bancho::handle_packet(Packet *packet) {
    // XXX: This is a bit of a mess, should at least group packets by type for readability
    if(cv::debug_network.getBool() && packet) {
        debugLog("packet id: {}\n", packet->id);
    }
    if(packet->id == USER_ID) {
        i32 new_user_id = proto::read<i32>(packet);
        this->user_id = new_user_id;
        osu->optionsMenu->update_login_button();
        osu->optionsMenu->setLoginLoadingState(false);

        Bancho::change_login_state(new_user_id > 0);

        if(new_user_id > 0) {
            debugLog("Logged in as user #{:d}.\n", new_user_id);
            cv::mp_autologin.setValue(true);
            this->print_new_channels = true;

            std::string avatar_dir = fmt::format(MCENGINE_DATA_DIR "avatars/{}", this->endpoint);
            env->createDirectory(avatar_dir);

            std::string replays_dir = fmt::format(MCENGINE_DATA_DIR "replays/{}", this->endpoint);
            env->createDirectory(replays_dir);

            osu->onUserCardChange(this->username.c_str());

            // XXX: We should toggle between "offline" sorting options and "online" ones
            //      Online ones would be "Local scores", "Global", "Country", "Selected mods" etc
            //      While offline ones would be "By score", "By pp", etc
            osu->songBrowser2->onSortScoresChange(UString("Online Leaderboard"), 0);

            // If server sent a score submission policy, update options menu to hide the checkbox
            osu->optionsMenu->scheduleLayoutUpdate();
        } else {
            cv::mp_autologin.setValue(false);
            cv::mp_oauth_token.setValue("");

            debugLog("Failed to log in, server returned code {:d}.\n", this->user_id);
            UString errmsg =
                UString::fmt("Failed to log in: {} (code {})\n", BANCHO::Net::cho_token.toUtf8(), this->user_id);
            if(new_user_id == -2) {
                errmsg = "Client version is too old to connect to this server.";
            } else if(new_user_id == -3 || new_user_id == -4) {
                errmsg = "You are banned from this server.";
            } else if(new_user_id == -6) {
                errmsg = "You need to buy supporter to connect to this server.";
            } else if(new_user_id == -7) {
                errmsg = "You need to reset your password to connect to this server.";
            } else if(new_user_id == -8) {
                errmsg = "Open the verification link sent to your email, then log in again.";
            } else {
                if(BANCHO::Net::cho_token == UString("user-already-logged-in")) {
                    errmsg = "Already logged in on another client.";
                } else if(BANCHO::Net::cho_token == UString("unknown-username")) {
                    errmsg = UString::fmt("No account by the username '{}' exists.", this->username);
                } else if(BANCHO::Net::cho_token == UString("incorrect-credentials")) {
                    errmsg = "This username is not registered.";
                } else if(BANCHO::Net::cho_token == UString("incorrect-password")) {
                    errmsg = "Incorrect password.";
                } else if(BANCHO::Net::cho_token == UString("contact-staff")) {
                    errmsg = "Please contact an administrator of the server.";
                }
            }
            osu->notificationOverlay->addToast(errmsg, ERROR_TOAST);
        }
    } else if(packet->id == RECV_MESSAGE) {
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
        osu->chat->addMessage(recipient, msg);
    } else if(packet->id == PONG) {
        // (nothing to do)
    } else if(packet->id == USER_STATS) {
        i32 raw_id = proto::read<i32>(packet);
        i32 stats_user_id = abs(raw_id);  // IRC clients are sent with negative IDs, hence the abs()
        auto action = (Action)proto::read<u8>(packet);

        UserInfo *user = BANCHO::User::get_user_info(stats_user_id);
        if(action != user->action) {
            // TODO @kiwec: i think client is supposed to regularly poll for friend stats
            if(user->is_friend() && cv::notify_friend_status_change.getBool() && action < NB_ACTIONS) {
                static constexpr auto actions = std::array{
                    "idle",   "afk",     "playing",    "editing", "modding", "in a multiplayer lobby", "spectating",
                    "vibing", "testing", "submitting", "pausing", "testing", "multiplaying",           "browsing maps",
                };
                auto text = UString::format("%s is now %s", user->name.toUtf8(), actions[action]);
                auto open_dms = [uid = stats_user_id]() -> void {
                    UserInfo *user = BANCHO::User::get_user_info(uid);
                    osu->chat->openChannel(user->name);
                };
                osu->notificationOverlay->addToast(text, STATUS_TOAST, open_dms, ToastElement::TYPE::CHAT);
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

        if(stats_user_id == this->user_id) {
            osu->userButton->updateUserStats();
        }
        if(stats_user_id == this->spectated_player_id) {
            osu->spectatorScreen->userCard->updateUserStats();
        }

        osu->chat->updateUserList();
    } else if(packet->id == USER_LOGOUT) {
        i32 logged_out_id = proto::read<i32>(packet);
        proto::read<u8>(packet);
        if(logged_out_id == this->user_id) {
            debugLog("Logged out.\n");
            bancho->disconnect();
        } else {
            BANCHO::User::logout_user(logged_out_id);
        }
    } else if(packet->id == IN_SPECTATE_FRAMES) {
        i32 extra = proto::read<i32>(packet);
        (void)extra;  // this is mania seed or something we can't use

        if(this->spectating) {
            UserInfo *info = BANCHO::User::get_user_info(this->spectated_player_id, true);
            auto beatmap = osu->getSelectedBeatmap();

            u16 nb_frames = proto::read<u16>(packet);
            for(u16 i = 0; i < nb_frames; i++) {
                auto frame = proto::read<LiveReplayFrame>(packet);

                if(frame.mouse_x < 0 || frame.mouse_x > 512 || frame.mouse_y < 0 || frame.mouse_y > 384) {
                    debugLog("WEIRD FRAME: time {:d}, x {:f}, y {:f}\n", frame.time, frame.mouse_x, frame.mouse_y);
                }

                beatmap->spectated_replay.push_back(LegacyReplay::Frame{
                    .cur_music_pos = frame.time,
                    .milliseconds_since_last_frame = 0,  // fixed below
                    .x = frame.mouse_x,
                    .y = frame.mouse_y,
                    .key_flags = frame.key_flags,
                });
            }

            // NOTE: Server can send frames in the wrong order. So we're correcting it here.
            std::ranges::sort(beatmap->spectated_replay, [](LegacyReplay::Frame a, LegacyReplay::Frame b) {
                return a.cur_music_pos < b.cur_music_pos;
            });
            beatmap->last_frame_ms = 0;
            for(auto &frame : beatmap->spectated_replay) {
                frame.milliseconds_since_last_frame = frame.cur_music_pos - beatmap->last_frame_ms;
                beatmap->last_frame_ms = frame.cur_music_pos;
            }

            auto action = (LiveReplayBundle::Action)proto::read<u8>(packet);
            info->spec_action = action;

            if(osu->isInPlayMode()) {
                if(action == LiveReplayBundle::Action::SONG_SELECT) {
                    info->map_id = 0;
                    info->map_md5 = MD5Hash();
                    beatmap->stop(true);
                }
                if(action == LiveReplayBundle::Action::UNPAUSE) {
                    beatmap->spectate_pause = false;
                }
                if(action == LiveReplayBundle::Action::PAUSE) {
                    beatmap->spectate_pause = true;
                }
                if(action == LiveReplayBundle::Action::SKIP) {
                    beatmap->skipEmptySection();
                }
                if(action == LiveReplayBundle::Action::FAIL) {
                    beatmap->fail(true);
                }
                if(action == LiveReplayBundle::Action::NEW_SONG) {
                    osu->rankingScreen->setVisible(false);
                    beatmap->restart(true);
                    beatmap->update();
                }
            }

            auto score_frame = proto::read<ScoreFrame>(packet);
            beatmap->score_frames.push_back(score_frame);

            auto sequence = proto::read<u16>(packet);
            (void)sequence;  // don't know how to use this
        }
    } else if(packet->id == SPECTATOR_JOINED) {
        i32 spectator_id = proto::read<i32>(packet);
        if(std::ranges::find(this->spectators, spectator_id) == this->spectators.end()) {
            debugLog("Spectator joined: user id {:d}\n", spectator_id);
            this->spectators.push_back(spectator_id);
        }
    } else if(packet->id == SPECTATOR_LEFT) {
        i32 spectator_id = proto::read<i32>(packet);
        auto it = std::ranges::find(this->spectators, spectator_id);
        if(it != this->spectators.end()) {
            debugLog("Spectator left: user id {:d}\n", spectator_id);
            this->spectators.erase(it);
        }
    } else if(packet->id == SPECTATOR_CANT_SPECTATE) {
        i32 spectator_id = proto::read<i32>(packet);
        debugLog("Spectator can't spectate: user id {:d}\n", spectator_id);
    } else if(packet->id == FELLOW_SPECTATOR_JOINED) {
        i32 spectator_id = proto::read<i32>(packet);
        if(std::ranges::find(this->fellow_spectators, spectator_id) == this->fellow_spectators.end()) {
            debugLog("Fellow spectator joined: user id {:d}\n", spectator_id);
            this->fellow_spectators.push_back(spectator_id);
        }
    } else if(packet->id == FELLOW_SPECTATOR_LEFT) {
        i32 spectator_id = proto::read<i32>(packet);
        auto it = std::ranges::find(this->fellow_spectators, spectator_id);
        if(it != this->fellow_spectators.end()) {
            debugLog("Fellow spectator left: user id {:d}\n", spectator_id);
            this->fellow_spectators.erase(it);
        }
    } else if(packet->id == GET_ATTENTION) {
        // (nothing to do)
    } else if(packet->id == NOTIFICATION) {
        UString notification = proto::read_string(packet);
        osu->notificationOverlay->addToast(notification, INFO_TOAST);
    } else if(packet->id == ROOM_UPDATED) {
        auto room = Room(packet);
        if(osu->lobby->isVisible()) {
            osu->lobby->updateRoom(room);
        } else if(room.id == this->room.id) {
            osu->room->on_room_updated(room);
        }
    } else if(packet->id == ROOM_CREATED) {
        auto room = new Room(packet);
        osu->lobby->addRoom(room);
    } else if(packet->id == ROOM_CLOSED) {
        i32 room_id = proto::read<i32>(packet);
        osu->lobby->removeRoom(room_id);
    } else if(packet->id == ROOM_JOIN_SUCCESS) {
        // Sanity, in case some trolley admins do funny business
        if(this->spectating) {
            stop_spectating();
        }
        if(osu->isInPlayMode()) {
            osu->getSelectedBeatmap()->stop(true);
        }

        auto room = Room(packet);
        osu->room->on_room_joined(room);
    } else if(packet->id == ROOM_JOIN_FAIL) {
        osu->notificationOverlay->addToast("Failed to join room.", ERROR_TOAST);
        osu->lobby->on_room_join_failed();
    } else if(packet->id == MATCH_STARTED) {
        auto room = Room(packet);
        osu->room->on_match_started(room);
    } else if(packet->id == UPDATE_MATCH_SCORE || packet->id == MATCH_SCORE_UPDATED) {
        osu->room->on_match_score_updated(packet);
    } else if(packet->id == HOST_CHANGED) {
        // (nothing to do)
    } else if(packet->id == MATCH_ALL_PLAYERS_LOADED) {
        osu->room->on_all_players_loaded();
    } else if(packet->id == MATCH_PLAYER_FAILED) {
        i32 slot_id = proto::read<i32>(packet);
        osu->room->on_player_failed(slot_id);
    } else if(packet->id == MATCH_FINISHED) {
        osu->room->on_match_finished();
    } else if(packet->id == MATCH_SKIP) {
        osu->room->on_all_players_skipped();
    } else if(packet->id == CHANNEL_JOIN_SUCCESS) {
        UString name = proto::read_string(packet);
        auto msg = ChatMessage{
            .tms = time(nullptr),
            .author_id = 0,
            .author_name = UString(""),
            .text = UString("Joined channel."),
        };
        osu->chat->addChannel(name);
        osu->chat->addMessage(name, msg, false);
    } else if(packet->id == CHANNEL_INFO) {
        UString channel_name = proto::read_string(packet);
        UString channel_topic = proto::read_string(packet);
        i32 nb_members = proto::read<i32>(packet);
        this->update_channel(channel_name, channel_topic, nb_members);
    } else if(packet->id == LEFT_CHANNEL) {
        UString name = proto::read_string(packet);
        osu->chat->removeChannel(name);
    } else if(packet->id == CHANNEL_AUTO_JOIN) {
        UString channel_name = proto::read_string(packet);
        UString channel_topic = proto::read_string(packet);
        i32 nb_members = proto::read<i32>(packet);
        this->update_channel(channel_name, channel_topic, nb_members);
    } else if(packet->id == PRIVILEGES) {
        proto::read<u32>(packet);
        // (nothing to do)
    } else if(packet->id == FRIENDS_LIST) {
        BANCHO::User::friends.clear();

        u16 nb_friends = proto::read<u16>(packet);
        for(u16 i = 0; i < nb_friends; i++) {
            i32 friend_id = proto::read<i32>(packet);
            BANCHO::User::friends.push_back(friend_id);
        }
    } else if(packet->id == PROTOCOL_VERSION) {
        int protocol_version = proto::read<i32>(packet);
        if(protocol_version != 19) {
            osu->notificationOverlay->addToast("This server may use an unsupported protocol version.", ERROR_TOAST);
        }
    } else if(packet->id == MAIN_MENU_ICON) {
        UString icon = proto::read_string(packet);
        auto urls = icon.split("|");
        if(urls.size() == 2 && ((urls[0].startsWith("http://")) || urls[0].startsWith("https://"))) {
            this->server_icon_url = urls[0];
        }
    } else if(packet->id == MATCH_PLAYER_SKIPPED) {
        i32 user_id = proto::read<i32>(packet);
        osu->room->on_player_skip(user_id);
    } else if(packet->id == USER_PRESENCE) {
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

        // Server can decide what username we use
        if(presence_user_id == this->user_id) { 
            this->username = presence_username.toUtf8();
            osu->onUserCardChange(presence_username);
        }

        osu->chat->updateUserList();
    } else if(packet->id == USER_PRESENCE_SINGLE) {
        BANCHO::User::get_user_info(proto::read<i32>(packet));  // add to online_users list
    } else if(packet->id == USER_PRESENCE_BUNDLE) {
        u16 nb_users = proto::read<u16>(packet);
        for(u16 i = 0; i < nb_users; i++) {
            BANCHO::User::get_user_info(proto::read<i32>(packet));  // add to online_users list
        }
    } else if(packet->id == RESTART) {
        // XXX: wait 'ms' milliseconds before reconnecting
        i32 ms = proto::read<i32>(packet);
        (void)ms;

        // Some servers send "restart" packets when password is incorrect
        // So, don't retry unless actually logged in
        if(this->is_online()) {
            bancho->reconnect();
        }
    } else if(packet->id == MATCH_INVITE) {
        UString sender = proto::read_string(packet);
        UString text = proto::read_string(packet);
        UString recipient = proto::read_string(packet);
        (void)recipient;
        i32 sender_id = proto::read<i32>(packet);
        auto msg = ChatMessage{
            .tms = time(nullptr),
            .author_id = sender_id,
            .author_name = sender,
            .text = text,
        };
        osu->chat->addMessage(recipient, msg);
    } else if(packet->id == CHANNEL_INFO_END) {
        this->print_new_channels = false;
        osu->chat->join("#announce");
        osu->chat->join("#osu");
    } else if(packet->id == ROOM_PASSWORD_CHANGED) {
        UString new_password = proto::read_string(packet);
        debugLog("Room changed password to {:s}\n", new_password.toUtf8());
        this->room.password = new_password;
    } else if(packet->id == SILENCE_END) {
        i32 delta = proto::read<i32>(packet);
        debugLog("Silence ends in {:d} seconds.\n", delta);
        // XXX: Prevent user from sending messages while silenced
    } else if(packet->id == USER_SILENCED) {
        i32 user_id = proto::read<i32>(packet);
        debugLog("User #{:d} silenced.\n", user_id);
    } else if(packet->id == USER_DM_BLOCKED) {
        proto::read_string(packet);
        proto::read_string(packet);
        UString blocked = proto::read_string(packet);
        proto::read<u32>(packet);
        debugLog("Blocked {:s}.\n", blocked.toUtf8());
    } else if(packet->id == TARGET_IS_SILENCED) {
        proto::read_string(packet);
        proto::read_string(packet);
        UString blocked = proto::read_string(packet);
        proto::read<u32>(packet);
        debugLog("Silenced {:s}.\n", blocked.toUtf8());
    } else if(packet->id == VERSION_UPDATE) {
        // (nothing to do)
    } else if(packet->id == VERSION_UPDATE_FORCED) {
        bancho->disconnect();
        osu->notificationOverlay->addToast("This server requires a newer client version.", ERROR_TOAST);
    } else if(packet->id == ACCOUNT_RESTRICTED) {
        osu->notificationOverlay->addToast("Account restricted.", ERROR_TOAST);
        bancho->disconnect();
    } else if(packet->id == MATCH_ABORT) {
        osu->room->on_match_aborted();
    } else {
        debugLog("Unknown packet ID {:d} ({:d} bytes)!\n", packet->id, packet->size);
    }
}

Packet Bancho::build_login_packet() {
    // Request format:
    // username\npasswd_md5\nosu_version|utc_offset|display_city|client_hashes|pm_private\n
    std::string req;

    if(cv::mp_oauth_token.getString().empty()) {
        req.append(this->username);
        req.append("\n");
        req.append(this->pw_md5.hash.data(), 32);
        req.append("\n");
    } else {
        req.append("$oauth");
        req.append("\n");
        req.append(cv::mp_oauth_token.getString());
        req.append("\n");
    }

    // OSU_VERSION is something like "b20200201.2"
    // This assert is so you avoid forgetting the 'b' when changing versions.
    assert(OSU_VERSION[0] == 'b');
    req.append(OSU_VERSION "|");

    // UTC offset
    time_t now = time(nullptr);
    auto gmt = gmtime(&now);
    auto local_time = localtime(&now);
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
    MD5Hash disk_md5 = md5((u8 *)this->get_disk_uuid().toUtf8(), this->get_disk_uuid().lengthUtf8());

    // XXX: Not implemented, I'm lazy so just reusing disk signature
    MD5Hash install_md5 = md5((u8 *)this->get_install_id().toUtf8(), this->get_install_id().lengthUtf8());

    this->client_hashes = UString::fmt("{:s}:{:s}:{:s}:{:s}:{:s}:", osu_path_md5.hash.data(), adapters,
                                       adapters_md5.hash.data(), install_md5.hash.data(), disk_md5.hash.data());
    req.append(this->client_hashes.toUtf8());
    req.append("|");

    // Allow PMs from strangers
    req.append("0\n");

    Packet packet;
    proto::write_bytes(&packet, (u8 *)req.c_str(), req.length());
    return packet;
}

std::string Bancho::get_username() const {
    if(this->is_online()) {
        return this->username;
    } else {
        return cv::name.getString();
    }
}

bool Bancho::can_submit_scores() const {
    if(!this->is_online()) {
        return false;
    } else if(this->score_submission_policy == ServerPolicy::NO_PREFERENCE) {
        return cv::submit_scores.getBool();
    } else if(this->score_submission_policy == ServerPolicy::YES) {
        return true;
    } else {
        return false;
    }
}

void Bancho::change_login_state(bool logged_in) {
    ConVar::relaxed_checks.store(!logged_in, std::memory_order_release);
};

void Bancho::update_channel(const UString &name, const UString &topic, i32 nb_members) {
    Channel *chan{nullptr};
    auto name_str = std::string(name.toUtf8());
    auto it = this->chat_channels.find(name_str);
    if(it == this->chat_channels.end()) {
        chan = new Channel();
        chan->name = name;
        this->chat_channels[name_str] = chan;

        if(this->print_new_channels) {
            auto msg = ChatMessage{
                .tms = time(nullptr),
                .author_id = 0,
                .author_name = UString(""),
                .text = UString::format("%s: %s", name.toUtf8(), topic.toUtf8()),
            };
            osu->chat->addMessage("#osu", msg, false);
        }
    } else {
        chan = it->second;
    }

    if(chan) {
        chan->topic = topic;
        chan->nb_members = nb_members;
    } else {
        debugLog("WARNING: no channel found??\n");
    }
}

const UString &Bancho::get_disk_uuid() const {
    static bool once = false;
    if(!once) {
        once = true;
        if constexpr(Env::cfg(OS::WINDOWS)) {
            this->disk_uuid = get_disk_uuid_win32();
        } else if constexpr(Env::cfg(OS::LINUX)) {
            this->disk_uuid = get_disk_uuid_blkid();
        } else {
            this->disk_uuid = "error getting disk uuid (unsupported platform)";
        }
    }
    return this->disk_uuid;
}

UString Bancho::get_disk_uuid_blkid() const {
    UString w_uuid{"error getting disk UUID"};
#ifdef MCENGINE_PLATFORM_LINUX
    const std::string &exe_path = Environment::getPathToSelf();

    // get the device number of the device the current exe is running from
    struct stat st{};
    if(stat(exe_path.c_str(), &st) != 0) {
        return w_uuid;
    }

    char *devname = blkid_devno_to_devname(st.st_dev);
    if(!devname) {
        return w_uuid;
    }

    // get the UUID of that device
    blkid_cache cache = nullptr;
    char *uuid = nullptr;

    if(blkid_get_cache(&cache, nullptr) == 0) {
        uuid = blkid_get_tag_value(cache, "UUID", devname);
        blkid_put_cache(cache);
    }

    if(uuid) {
        w_uuid = UString{uuid};
        free(uuid);
    }

    free(devname);
#endif
    return w_uuid;
}

UString Bancho::get_disk_uuid_win32() const {
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
