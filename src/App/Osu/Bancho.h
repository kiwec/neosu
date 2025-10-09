#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.

#include "BanchoProtocol.h"
#include "UString.h"

#include <atomic>
#include <unordered_map>

class Image;

enum class ServerPolicy : uint8_t {
    NO,
    YES,
    NO_PREFERENCE,
};

struct BanchoState final {
    // entirely static
    BanchoState() = delete;
    ~BanchoState() = delete;

    BanchoState &operator=(const BanchoState &) = delete;
    BanchoState &operator=(BanchoState &&) = delete;
    BanchoState(const BanchoState &) = delete;
    BanchoState(BanchoState &&) = delete;

    static std::string endpoint;
    static MD5Hash pw_md5;
    static u8 oauth_challenge[32];
    static u8 oauth_verifier[32];
    static bool is_oauth;

    static bool spectating;
    static i32 spectated_player_id;
    static std::vector<u32> spectators;
    static std::vector<u32> fellow_spectators;

    static std::string server_icon_url;
    static Image *server_icon;

    static ServerPolicy score_submission_policy;

    static UString neosu_version;
    static UString cho_token;
    static UString user_agent;
    static UString client_hashes;

    static Room room;
    static bool match_started;
    static Slot last_scores[16];

    struct Channel {
        UString name;
        UString topic;
        u32 nb_members;
    };

    static std::unordered_map<std::string, BanchoState::Channel *> chat_channels;

    // utils
    static void handle_packet(Packet &packet);
    static std::string build_login_packet();
    static MD5Hash md5(const u8 *msg, size_t msg_len);

    // cached uuid
    [[nodiscard]] static const UString &get_disk_uuid();

    // cached install id (currently unimplemented, just returns disk uuid)
    [[nodiscard]] static inline const UString &get_install_id() { return get_disk_uuid(); }

    // Room ID can be 0 on private servers! So we check if the room has players instead.
    [[nodiscard]] static inline bool is_in_a_multi_room() { return room.nb_players > 0; }
    [[nodiscard]] static inline bool is_playing_a_multi_map() { return match_started; }
    [[nodiscard]] static bool can_submit_scores();

    [[nodiscard]] static inline bool is_online() { return user_id.load(std::memory_order_acquire) > 0; }
    [[nodiscard]] static inline i32 get_uid() { return user_id.load(std::memory_order_acquire); }
    static inline void set_uid(i32 uid) { user_id.store(uid, std::memory_order_release); }

    [[nodiscard]] static std::string get_username();

    static void disconnect();
    static void reconnect();

   private:
    // internal helpers
    static void update_channel(const UString &name, const UString &topic, i32 nb_members, bool join);

    [[nodiscard]] static UString get_disk_uuid_win32();
    [[nodiscard]] static UString get_disk_uuid_blkid();

    static bool print_new_channels;

    // use get_username() to avoid having to check for online status
    static std::string username;

    // cached on first get
    static UString disk_uuid;
    // static UString install_id; // TODO?

    static std::atomic<i32> user_id;
};
