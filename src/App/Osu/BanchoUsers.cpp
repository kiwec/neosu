// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoUsers.h"

#include <algorithm>

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "Chat.h"
#include "Engine.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "SpectatorScreen.h"

namespace BANCHO::User {

std::unordered_map<i32, UserInfo*> all_users;
std::unordered_map<i32, UserInfo*> online_users;
std::vector<i32> friends;
std::vector<UserInfo*> presence_requests;
std::vector<UserInfo*> stats_requests;

void enqueue_presence_request(UserInfo* info) {
    if(info->has_presence) return;
    if(std::find(presence_requests.begin(), presence_requests.end(), info) != presence_requests.end()) return;
    presence_requests.push_back(info);
}

void enqueue_stats_request(UserInfo* info) {
    if(info->irc_user) return;
    if(info->stats_tms + 5000 > Timing::getTicksMS()) return;
    if(std::find(stats_requests.begin(), stats_requests.end(), info) != stats_requests.end()) return;
    stats_requests.push_back(info);
}

void request_presence_batch() {
    std::vector<i32> actual_requests;
    for(auto req : presence_requests) {
        if(req->has_presence) continue;
        actual_requests.push_back(req->user_id);
    }

    presence_requests.clear();
    if(actual_requests.empty()) return;

    Packet packet;
    packet.id = USER_PRESENCE_REQUEST;
    BANCHO::Proto::write<u16>(&packet, actual_requests.size());
    for(auto user_id : actual_requests) {
        BANCHO::Proto::write<i32>(&packet, user_id);
    }
    BANCHO::Net::send_packet(packet);
}

void request_stats_batch() {
    std::vector<i32> actual_requests;
    for(auto req : stats_requests) {
        if(req->irc_user) continue;
        if(req->stats_tms + 5000 > Timing::getTicksMS()) continue;
        actual_requests.push_back(req->user_id);
    }

    stats_requests.clear();
    if(actual_requests.empty()) return;

    Packet packet;
    packet.id = USER_STATS_REQUEST;
    BANCHO::Proto::write<u16>(&packet, actual_requests.size());
    for(auto user_id : actual_requests) {
        BANCHO::Proto::write<i32>(&packet, user_id);
    }
    BANCHO::Net::send_packet(packet);
}

void login_user(i32 user_id) {
    // We mark the user as online, but don't request presence data
    // Presence & stats are only requested when the user shows up in UI
    online_users[user_id] = get_user_info(user_id, false);
}

void logout_user(i32 user_id) {
    for(auto it = online_users.begin(); it != online_users.end(); it++) {
        if(it->first == user_id) {
            debugLog("{:s} has disconnected.\n", it->second->name.toUtf8());
            if(it->first == BanchoState::spectated_player_id) {
                stop_spectating();
            }

            if(it->second->is_friend() && cv::notify_friend_status_change.getBool()) {
                auto text = UString::format("%s is now offline", it->second->name.toUtf8());
                osu->notificationOverlay->addToast(text, STATUS_TOAST, {}, ToastElement::TYPE::CHAT);
            }

            online_users.erase(it);

            osu->chat->updateUserList();

            return;
        }
    }
}

void logout_all_users() {
    for(auto& pair : all_users) {
        delete pair.second;
    }
    all_users.clear();
    online_users.clear();
    friends.clear();
    presence_requests.clear();
    stats_requests.clear();
}

UserInfo* find_user(const UString& username) {
    for(auto pair : all_users) {
        if(pair.second->name == username) {
            return pair.second;
        }
    }

    return nullptr;
}

UserInfo* find_user_starting_with(UString prefix, const UString& last_match) {
    bool matched = last_match.length() == 0;
    for(auto pair : all_users) {
        auto user = pair.second;
        if(!matched) {
            if(user->name == last_match) {
                matched = true;
            }
            continue;
        }
        UString lowerName{user->name};
        lowerName.lowerCase();
        prefix.lowerCase();
        if(lowerName.startsWith(prefix)) {
            return user;
        }
    }

    if(last_match.length() == 0) {
        return nullptr;
    } else {
        return find_user_starting_with(prefix, "");
    }
}

UserInfo* try_get_user_info(i32 user_id, bool wants_presence) {
    auto it = all_users.find(user_id);
    if(it != all_users.end()) {
        if(wants_presence) {
            enqueue_presence_request(it->second);
        }

        return it->second;
    }

    return nullptr;
}

UserInfo* get_user_info(i32 user_id, bool wants_presence) {
    auto info = try_get_user_info(user_id, wants_presence);
    if(!info) {
        info = new UserInfo();
        info->user_id = user_id;
        info->name = UString::format("User #%d", user_id);
        all_users[user_id] = info;
        osu->chat->updateUserList();

        if(wants_presence) {
            enqueue_presence_request(info);
        }
    }

    return info;
}

}  // namespace BANCHO::User

using namespace BANCHO::User;

bool UserInfo::is_friend() {
    auto it = std::ranges::find(friends, this->user_id);
    return it != friends.end();
}
