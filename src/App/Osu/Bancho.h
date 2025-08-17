#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.

#include "templates.h"
#include "BanchoProtocol.h"
#include "UString.h"

#include <unordered_map>

class Image;

enum class ServerPolicy : uint8_t {
    NO,
    YES,
    NO_PREFERENCE,
};

struct Bancho final {
    Bancho() = default;
    ~Bancho() = default;

    Bancho &operator=(const Bancho &) = delete;
    Bancho &operator=(Bancho &&) = delete;
    Bancho(const Bancho &) = delete;
    Bancho(Bancho &&) = delete;

    // static convenience
    static MD5Hash md5(u8 *msg, size_t msg_len);

    UString neosu_version;

    std::string endpoint;
    i32 user_id{0};
    MD5Hash pw_md5;
    u8 oauth_challenge[32]{};
    u8 oauth_verifier[32]{};
    Room room;

    bool spectating{false};
    i32 spectated_player_id{0};
    std::vector<u32> spectators;
    std::vector<u32> fellow_spectators;

    UString server_icon_url;
    Image *server_icon{nullptr};

    ServerPolicy score_submission_policy{ServerPolicy::NO_PREFERENCE};

    UString user_agent;
    UString client_hashes;

    bool match_started{false};
    Slot last_scores[16];

    struct Channel {
        UString name;
        UString topic;
        u32 nb_members;
    };

    std::unordered_map<std::string, Bancho::Channel *> chat_channels;

    // utils
    void handle_packet(Packet *packet);
    Packet build_login_packet();

    // cached uuid
    [[nodiscard]] const UString &get_disk_uuid() const;

    // cached install id (currently unimplemented, just returns disk uuid)
    [[nodiscard]] inline const UString &get_install_id() const { return this->get_disk_uuid(); }

    // Room ID can be 0 on private servers! So we check if the room has players instead.
    [[nodiscard]] inline bool is_in_a_multi_room() const { return this->room.nb_players > 0; }
    [[nodiscard]] inline bool is_playing_a_multi_map() const { return this->match_started; }
    [[nodiscard]] inline bool is_online() const { return this->user_id > 0; }
    [[nodiscard]] std::string get_username() const;
    [[nodiscard]] bool can_submit_scores() const;

    static void change_login_state(bool logged);
    void disconnect();
    void reconnect();

   private:
    // internal helpers
    void update_channel(const UString &name, const UString &topic, i32 nb_members);

    [[nodiscard]] UString get_disk_uuid_win32() const;
    [[nodiscard]] UString get_disk_uuid_blkid() const;

    bool print_new_channels{true};

    // use get_username() to avoid having to check for online status
    std::string username;

    // cached on first get
    mutable UString disk_uuid;
    // mutable UString install_id; // TODO?
};

// initialized by NetworkHandler
// declared here for convenience
extern mcatomic_ref<Bancho *> bancho;
