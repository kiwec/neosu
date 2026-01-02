#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.

#include "BanchoProtocol.h"
#include "ModFlags.h"

#include <unordered_map>
#include <string>
#include <vector>

// TEMP:
// old: 192 bytes
// reordered struct: 160 bytes
struct UserInfo {
    UserInfo() noexcept = default;
    ~UserInfo() noexcept;

    UserInfo(const UserInfo &) noexcept = default;
    UserInfo &operator=(const UserInfo &) noexcept = default;
    UserInfo(UserInfo &&) noexcept = default;
    UserInfo &operator=(UserInfo &&) noexcept = default;

    MD5Hash map_md5;

    std::string name{};  // From presence
    std::string info_text{"Loading..."};

    i32 user_id{0};

    // Stats (via USER_STATS_REQUEST)
    i32 map_id{0};
    u64 stats_tms{0};
    i64 total_score{0};
    i64 ranked_score{0};
    i32 plays{0};
    f32 accuracy{0.f};
    LegacyFlags mods{};
    u16 pp{0};

    Action action{Action::UNKNOWN};
    GameMode mode{GameMode::STANDARD};

    // Presence (via USER_PRESENCE_REQUEST or USER_PRESENCE_REQUEST_ALL)
    f32 longitude{0.f};
    f32 latitude{0.f};
    i32 global_rank{0};
    u8 utc_offset{0};
    u8 country{0};
    u8 privileges{0};

    // lower 6 bits stores spectator action, upper 2 bits store presence + irc state
    // dumb way to avoid annoying struct padding
    enum DataBits : u8 { PRESENCE = (1 << 6), IRC = (1 << 7) };
    u8 extra_data{0};

    [[nodiscard]] bool is_friend() const;

    [[nodiscard]] constexpr bool has_presence() const { return this->extra_data & PRESENCE; }
    [[nodiscard]] constexpr bool is_irc() const { return this->extra_data & IRC; }

    [[nodiscard]] constexpr LiveReplayAction get_spec_action() const {
        static_assert((u8)LiveReplayAction::MAX_ACTION < 0b00111111);
        return (LiveReplayAction)(this->extra_data & 0b00111111);
    }

    inline void set_is_irc(bool isirc) {
        if(isirc) {
            this->extra_data |= IRC;
        } else {
            this->extra_data &= ~IRC;
        }
    }

    inline void set_has_presence(bool presence) {
        if(presence) {
            this->extra_data |= PRESENCE;
        } else {
            this->extra_data &= ~PRESENCE;
        }
    }

    // Received when spectating
    inline void set_spec_action(LiveReplayAction action) {
        const u8 new_spec_flag_lobits = (u8)action & 0b00111111;
        const u8 cur_non_spec_hibits = this->extra_data & 0b11000000;
        this->extra_data = cur_non_spec_hibits | new_spec_flag_lobits;
    }
};

namespace BANCHO::User {

extern std::unordered_map<i32, UserInfo *> online_users;
extern std::vector<i32> friends;

void login_user(i32 user_id);
void logout_user(i32 user_id);
void logout_all_users();

UserInfo *find_user(std::string_view username);
UserInfo *find_user_starting_with(std::string_view prefix, std::string_view last_match);
UserInfo *try_get_user_info(i32 user_id, bool wants_presence = false);
UserInfo *get_user_info(i32 user_id, bool wants_presence = false);

void dequeue_presence_request(const UserInfo *info);
void dequeue_stats_request(const UserInfo *info);
void enqueue_presence_request(const UserInfo *info);
void enqueue_stats_request(const UserInfo *info);
void request_presence_batch();
void request_stats_batch();
}  // namespace BANCHO::User
