#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.
#include "BanchoProtocol.h"

namespace BANCHO::Api {

void append_auth_params(UString& url, std::string user_param = "u", std::string pw_param = "h");

// XXX: You should avoid using the stuff below.
//      Just use NetworkHandler->httpRequestAsync directly with a lambda,
//      or anything except this mess really.

enum RequestType : uint8_t {
    NONE,
    GET_BEATMAPSET_INFO,
    GET_MAP_LEADERBOARD,
    GET_REPLAY,
    MARK_AS_READ,
};

struct Request {
    UString path{""};
    u8* extra{nullptr};
    i32 extra_int{0};  // lazy
    RequestType type{NONE};
};

void send_request(const Request& request);

// Must be called regularly from main thread.
void update();

}  // namespace BANCHO::Api
