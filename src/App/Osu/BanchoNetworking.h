#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.

#include "BanchoProtocol.h"

#define NEOSU_DOMAIN "neosu.net"

// NOTE: Full version can be something like "b20200201.2cuttingedge"
#define OSU_VERSION "b20250815"
#define OSU_VERSION_DATEONLY 20250815

enum APIRequestType : uint8_t {
    GET_BEATMAPSET_INFO,
    GET_MAP_LEADERBOARD,
    GET_REPLAY,
    MARK_AS_READ,
};

struct APIRequest {
    APIRequestType type;
    UString path{""};
    u8* extra{nullptr};
    i32 extra_int{0};  // lazy
};

namespace BANCHO::Net {

// Send an API request.
void send_api_request(const APIRequest& request);

// Send a packet to Bancho. Do not free it after calling this.
void send_packet(Packet& packet);

// Poll for new packets. Should be called regularly from main thread.
void receive_api_responses();
void receive_bancho_packets();

// Process networking logic. Should be called regularly from main thread.
void update_networking();

// Clean up networking. Should be called once when exiting neosu.
void cleanup_networking();

// Callback for complete_oauth command
void complete_oauth(std::string_view code);

void append_auth_params(UString& url, std::string user_param = "u", std::string pw_param = "h");

}  // namespace BANCHO::Net
