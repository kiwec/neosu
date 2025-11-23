// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoUsers.h"

#include <algorithm>

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "Chat.h"
#include "ConVar.h"
#include "Engine.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "Timing.h"
#include "SpectatorScreen.h"
#include "Logging.h"

namespace BANCHO::User {

std::unordered_map<i32, std::shared_ptr<UserInfo>> online_users;
std::vector<i32> friends;
namespace {  // static
std::unordered_map<i32, std::shared_ptr<UserInfo>> all_users;
std::vector<std::shared_ptr<UserInfo>> presence_requests;
std::vector<std::shared_ptr<UserInfo>> stats_requests;

std::shared_ptr<UserInfo> null_user{nullptr};
}  // namespace

void enqueue_presence_request(std::shared_ptr<UserInfo> info) {
    if(info->has_presence) return;
    if(!std::ranges::contains(presence_requests, info)) return;
    presence_requests.push_back(std::move(info));
}

void enqueue_stats_request(std::shared_ptr<UserInfo> info) {
    if(info->irc_user) return;
    if(info->stats_tms + 5000 > Timing::getTicksMS()) return;
    if(!std::ranges::contains(stats_requests, info)) return;
    stats_requests.push_back(std::move(info));
}

void request_presence_batch() {
    std::vector<i32> actual_requests;
    for(const auto& req : presence_requests) {
        if(req->has_presence) continue;
        actual_requests.push_back(req->user_id);
    }

    presence_requests.clear();
    if(actual_requests.empty()) return;

    Packet packet;
    packet.id = USER_PRESENCE_REQUEST;
    packet.write<u16>(actual_requests.size());
    for(const auto& user_id : actual_requests) {
        packet.write<i32>(user_id);
    }
    BANCHO::Net::send_packet(packet);
}

void request_stats_batch() {
    std::vector<i32> actual_requests;
    for(const auto& req : stats_requests) {
        if(req->irc_user) continue;
        if(req->stats_tms + 5000 > Timing::getTicksMS()) continue;
        actual_requests.push_back(req->user_id);
    }

    stats_requests.clear();
    if(actual_requests.empty()) return;

    Packet packet;
    packet.id = USER_STATS_REQUEST;
    packet.write<u16>(actual_requests.size());
    for(const auto& user_id : actual_requests) {
        packet.write<i32>(user_id);
    }
    BANCHO::Net::send_packet(packet);
}

void login_user(i32 user_id) {
    // We mark the user as online, but don't request presence data
    // Presence & stats are only requested when the user shows up in UI
    online_users[user_id] = get_user_info(user_id, false);
}

void logout_user(i32 user_id) {
    if(const auto& it = online_users.find(user_id); it != online_users.end()) {
        const auto& user_info = it->second;

        debugLog("{:s} has disconnected.", user_info->name);
        if(user_id == BanchoState::spectated_player_id) {
            Spectating::stop();
        }

        if(user_info->is_friend() && cv::notify_friend_status_change.getBool()) {
            auto text = fmt::format("{} is now offline", user_info->name);
            osu->getNotificationOverlay()->addToast(text, STATUS_TOAST, {}, ToastElement::TYPE::CHAT);
        }

        online_users.erase(it);
        osu->getChat()->updateUserList();
    }
}

void logout_all_users() {
    all_users.clear();
    online_users.clear();
    friends.clear();
    presence_requests.clear();
    stats_requests.clear();
}

const std::shared_ptr<UserInfo>& find_user(const UString& username) {
    if(const auto& it = std::ranges::find_if(all_users,
                                             [&username](const std::pair<i32, std::shared_ptr<UserInfo>>& uinfo) {
                                                 return uinfo.second->name == username;
                                             });
       it != all_users.end()) {
        return it->second;
    }

    return null_user;
}

const std::shared_ptr<UserInfo>& find_user_starting_with(UString prefix, const UString& last_match) {
    if(prefix.isEmpty()) return null_user;

    prefix.lowerCase();
    // cycle through matches
    bool matched = last_match.length() == 0;
    for(auto& [_, user] : online_users) {
        if(!matched) {
            if(user->name == last_match) {
                matched = true;
            }
            continue;
        }
        // if it starts with prefix
        if(user->name.findIgnoreCase(prefix) == 0) {
            return user;
        }
    }

    if(last_match.length() == 0) {
        return null_user;
    } else {
        return find_user_starting_with(prefix, "");
    }
}

const std::shared_ptr<UserInfo>& try_get_user_info(i32 user_id, bool wants_presence) {
    if(const auto& it = all_users.find(user_id); it != all_users.end()) {
        auto& user_info = it->second;
        if(wants_presence) {
            enqueue_presence_request(user_info);
        }

        return user_info;
    }

    return null_user;
}

std::shared_ptr<UserInfo>& get_user_info(i32 user_id, bool wants_presence) {
    auto& existing_info = try_get_user_info(user_id, wants_presence);
    // to avoid null_user sentinel being modified on the outside...
    if(existing_info && existing_info != null_user) {
        return const_cast<std::shared_ptr<UserInfo>&>(existing_info);
    }

    auto temp_new_info = std::make_shared<UserInfo>();

    temp_new_info->user_id = user_id;
    temp_new_info->name = UString::format("User #%d", user_id);
    const auto& [inserted_it, successfully_inserted] = all_users.insert({user_id, std::move(temp_new_info)});
    assert(successfully_inserted);
    auto& new_info = inserted_it->second;

    osu->getChat()->updateUserList();

    if(wants_presence) {
        enqueue_presence_request(new_info);
    }

    return new_info;
}

}  // namespace BANCHO::User

using namespace BANCHO::User;

bool UserInfo::is_friend() const { return std::ranges::contains(friends, this->user_id); }
