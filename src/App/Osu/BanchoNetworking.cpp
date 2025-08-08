// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoNetworking.h"

#include <ctime>

#include <mutex>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "SString.h"

#include "Bancho.h"
#include "BanchoLeaderboard.h"
#include "BanchoProtocol.h"
#include "BanchoUsers.h"
#include "Beatmap.h"
#include "Chat.h"
#include "ConVar.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "File.h"
#include "Lobby.h"
#include "NetworkHandler.h"
#include "OptionsMenu.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "UIButton.h"
#include "UserCard.h"

#include <curl/curl.h>

// Bancho protocol
namespace proto = BANCHO::Proto;

namespace BANCHO::Net {
namespace {  // static namespace

bool try_logging_in = false;
Packet outgoing;
std::mutex incoming_mutex;
std::vector<Packet> incoming_queue;
time_t last_packet_tms = {0};
std::atomic<double> seconds_between_pings{1.0};

std::mutex auth_mutex;
std::string auth_header = "";

// osu! private API
std::mutex api_requests_mutex;
std::mutex api_responses_mutex;
std::vector<APIRequest> api_request_queue;
std::vector<Packet> api_response_queue;

void send_api_request_async(const APIRequest &api_out) {
    NetworkHandler::RequestOptions options;
    options.timeout = 60;
    options.connectTimeout = 5;
    options.userAgent = "osu!";

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto query_url = UString::format("%sosu.%s%s", scheme, bancho->endpoint.c_str(), api_out.path.toUtf8());

    if(api_out.type == SUBMIT_SCORE) {
        auto token_header = UString::format("token: %s", cho_token.toUtf8());
        options.headers["token"] = cho_token.toUtf8();
    }

    if(api_out.mime != nullptr) {
        options.mimeData = api_out.mime;
    }

    networkHandler->httpRequestAsync(
        query_url,
        [api_out](NetworkHandler::Response response) {
            Packet api_response;
            api_response.id = api_out.type;
            api_response.extra = api_out.extra;
            api_response.extra_int = api_out.extra_int;

            if(response.success) {
                api_response.size = response.body.length() + 1;  // +1 for null terminator
                api_response.memory = (u8 *)malloc(api_response.size);
                memcpy(api_response.memory, response.body.data(), response.body.length());
                api_response.memory[response.body.length()] = '\0';  // null terminate

                api_responses_mutex.lock();
                api_response_queue.push_back(api_response);
                api_responses_mutex.unlock();
            }

            // cleanup mime data if it was provided
            if(api_out.mime) {
                curl_mime_free(api_out.mime);
            }
        },
        options);
}

void send_bancho_packet_async(Packet outgoing) {
    NetworkHandler::RequestOptions options;
    options.timeout = 30;
    options.connectTimeout = 5;
    options.userAgent = "osu!";

    auto version_header = UString::format("x-mcosu-ver: %s", bancho->neosu_version.toUtf8());
    options.headers["x-mcosu-ver"] = bancho->neosu_version.toUtf8();

    {
        std::scoped_lock<std::mutex> lock{auth_mutex};
        if(!auth_header.empty()) {
            // extract token from "osu-token: TOKEN" format
            size_t colon_pos = auth_header.find(':');
            if(colon_pos != std::string::npos) {
                std::string token = auth_header.substr(colon_pos + 1);
                // trim whitespace
                SString::trim(&token);
                options.headers["osu-token"] = token;
            }
        }
    }

    // copy outgoing packet data for POST
    options.postData = std::string(reinterpret_cast<char *>(outgoing.memory), outgoing.pos);

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto query_url = UString::format("%sc.%s/", scheme, bancho->endpoint.c_str());

    last_packet_tms = time(nullptr);

    networkHandler->httpRequestAsync(
        query_url,
        [](NetworkHandler::Response response) {
            if(!response.success) {
                Engine::logRaw("[httpRequestAsync] Failed to send packet, HTTP error {}\n", response.responseCode);
                std::scoped_lock<std::mutex> lock{auth_mutex};
                if(auth_header.empty()) {
                    auto errmsg = UString::format("Failed to log in: HTTP %ld", response.responseCode);
                    osu->notificationOverlay->addToast(errmsg, ERROR_TOAST);
                }
                return;
            }

            // // debug
            // if (cv::debug_network.getBool()) {
            //     Engine::logRaw("DEBUG headers:\n");
            //     for(const auto &headerstr : response.headers) {
            //         Engine::logRaw("{:s} {:s}\n", headerstr.first.c_str(), headerstr.second.c_str());
            //     }
            // }

            // Update auth token
            auto cho_token_it = response.headers.find("cho-token");
            if(cho_token_it != response.headers.end()) {
                std::scoped_lock<std::mutex> lock{auth_mutex};
                auth_header = "osu-token: " + cho_token_it->second;
                cho_token = UString(cho_token_it->second);
            }

            auto features_it = response.headers.find("x-mcosu-features");
            if(features_it != response.headers.end()) {
                if(strstr(features_it->second.c_str(), "submit=0") != nullptr) {
                    bancho->score_submission_policy = ServerPolicy::NO;
                    Engine::logRaw("[httpRequestAsync] Server doesn't want score submission. :(\n");
                } else if(strstr(features_it->second.c_str(), "submit=1") != nullptr) {
                    bancho->score_submission_policy = ServerPolicy::YES;
                    Engine::logRaw("[httpRequestAsync] Server wants score submission! :D\n");
                }
            }

            // parse response packets
            Packet response_packet = {
                .memory = (u8 *)malloc(response.body.length() + 1),  // +1 for null terminator
                .size = response.body.length() + 1,
                .pos = 0,
            };
            memcpy(response_packet.memory, response.body.data(), response.body.length());
            response_packet.memory[response.body.length()] = '\0';  // null terminate

            while(response_packet.pos < response_packet.size) {
                if(response_packet.pos + 7 > response_packet.size) break;  // need at least packet header

                u16 packet_id = proto::read<u16>(&response_packet);
                response_packet.pos++;  // skip compression flag
                u32 packet_len = proto::read<u32>(&response_packet);

                if(packet_len > 10485760) {
                    Engine::logRaw("[httpRequestAsync] Received a packet over 10Mb! Dropping response.\n");
                    break;
                }

                if(response_packet.pos + packet_len > response_packet.size) break;

                Packet incoming = {
                    .id = packet_id,
                    .memory = (u8 *)calloc(packet_len, sizeof(*Packet::memory)),
                    .size = packet_len,
                    .pos = 0,
                };
                memcpy(incoming.memory, response_packet.memory + response_packet.pos, packet_len);

                incoming_mutex.lock();
                incoming_queue.push_back(incoming);
                incoming_mutex.unlock();

                seconds_between_pings = 1.0;
                response_packet.pos += packet_len;
            }

            free(response_packet.memory);
        },
        options);

    free(outgoing.memory);
}

void handle_api_response(Packet packet) {
    switch(packet.id) {
        case GET_BEATMAPSET_INFO: {
            Downloader::process_beatmapset_info_response(packet);
            break;
        }

        case GET_MAP_LEADERBOARD: {
            BANCHO::Leaderboard::process_leaderboard_response(packet);
            break;
        }

        case GET_REPLAY: {
            if(packet.size == 0) {
                // Most likely, 404
                osu->notificationOverlay->addToast("Failed to download replay", ERROR_TOAST);
                break;
            }

            assert(packet.extra && "handle_api_response(GET_REPLAY) got invalid packet for replay data");
            // we should look into this if it ever gets hit, shouldn't crash either way but still... a warning is a
            // warning
            assert(reinterpret_cast<uintptr_t>(packet.extra) % alignof(FinishedScore) == 0 &&
                   "handle_api_response(GET_REPLAY) packet.extra is not aligned");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
            auto *score = reinterpret_cast<FinishedScore *>(packet.extra);
#pragma GCC diagnostic pop

            auto replay_path = UString::format(MCENGINE_DATA_DIR "replays/%s/%d.replay.lzma", score->server.c_str(),
                                               score->unixTimestamp);

            // XXX: this is blocking main thread
            FILE *replay_file = fopen(replay_path.toUtf8(), "wb");
            if(replay_file == nullptr) {
                osu->notificationOverlay->addToast("Failed to save replay", ERROR_TOAST);
                break;
            }

            fwrite(packet.memory, packet.size, 1, replay_file);
            fclose(replay_file);
            LegacyReplay::load_and_watch(*score);
            break;
        }

        case MARK_AS_READ: {
            // (nothing to do)
            break;
        }

        case SUBMIT_SCORE: {
            // TODO @kiwec: handle response
            debugLog("Score submit result: {:s}\n", reinterpret_cast<const char *>(packet.memory));

            // Reset leaderboards so new score will appear
            db->online_scores.clear();
            osu->getSongBrowser()->rebuildScoreButtons();
            break;
        }

        default: {
            // NOTE: API Response type is same as API Request type
            debugLog("No handler for API response type {:d}!\n", packet.id);
        }
    }
}

}  // namespace

// this is a global var
UString cho_token = "";

void disconnect() {
    Bancho::change_login_state(false);

    // Logout
    // This is a blocking call, but we *do* want this to block when quitting the game.
    if(bancho->is_online()) {
        Packet packet;
        proto::write<u16>(&packet, LOGOUT);
        proto::write<u8>(&packet, 0);
        proto::write<u32>(&packet, 4);
        proto::write<u32>(&packet, 0);

        NetworkHandler::RequestOptions options;
        options.timeout = 5;
        options.connectTimeout = 5;
        options.userAgent = "osu!";
        options.postData = std::string(reinterpret_cast<char *>(packet.memory), packet.pos);

        auto version_header = UString::format("x-mcosu-ver: %s", bancho->neosu_version.toUtf8());
        options.headers["x-mcosu-ver"] = bancho->neosu_version.toUtf8();

        {
            std::scoped_lock<std::mutex> lock{auth_mutex};
            if(!auth_header.empty()) {
                size_t colon_pos = auth_header.find(':');
                if(colon_pos != std::string::npos) {
                    std::string token = auth_header.substr(colon_pos + 1);
                    SString::trim(&token);
                    options.headers["osu-token"] = token;
                }
            }
        }

        auto scheme = cv::use_https.getBool() ? "https://" : "http://";
        auto query_url = UString::format("%sc.%s/", scheme, bancho->endpoint.c_str());

        // use sync request for logout to ensure it completes
        NetworkHandler::Response response = networkHandler->performSyncRequest(query_url, options);

        free(packet.memory);
    }

    try_logging_in = false;
    {
        std::scoped_lock<std::mutex> lock{auth_mutex};
        auth_header = "";
    }
    free(outgoing.memory);
    outgoing = Packet();

    bancho->user_id = 0;
    osu->userButton->setID(0);

    bancho->endpoint = "";
    bancho->spectating = false;
    bancho->spectated_player_id = 0;
    bancho->spectators.clear();
    bancho->fellow_spectators.clear();
    bancho->server_icon_url = "";
    if(bancho->server_icon != nullptr) {
        resourceManager->destroyResource(bancho->server_icon);
        bancho->server_icon = nullptr;
    }

    bancho->score_submission_policy = ServerPolicy::NO_PREFERENCE;
    osu->optionsMenu->update_login_button();
    osu->optionsMenu->scheduleLayoutUpdate();

    for(auto &pair : BANCHO::User::online_users) {
        delete pair.second;
    }
    BANCHO::User::online_users.clear();
    BANCHO::User::friends.clear();

    osu->chat->onDisconnect();

    // XXX: We should toggle between "offline" sorting options and "online" ones
    //      Online ones would be "Local scores", "Global", "Country", "Selected mods" etc
    //      While offline ones would be "By score", "By pp", etc
    osu->songBrowser2->onSortScoresChange(UString("Sort by pp"), 0);

    Downloader::abort_downloads();
}

void reconnect() {
    disconnect();

    // Disable autologin, in case there's an error while logging in
    // Will be reenabled after the login succeeds
    cv::mp_autologin.setValue(false);

    bancho->username = cv::name.getString().c_str();
    bancho->endpoint = cv::mp_server.getString();

    // Admins told me they don't want any clients to connect
    constexpr auto server_blacklist = std::array{
        "ppy.sh",  // haven't asked, but the answer is obvious
        "gatari.pw",
    };
    for(const char *endpoint : server_blacklist) {
        if(!strcmp(endpoint, bancho->endpoint.c_str())) {
            osu->notificationOverlay->addToast("This server does not allow neosu clients.", ERROR_TOAST);
            return;
        }
    }

    {
        auto *tempConVar = &cv::mp_password_temporary;
        if(!tempConVar->getString().empty() &&
           tempConVar->getString() != cv::mp_password_md5.getString())  // HACK: find a better way to do this...
        {
            const char *tempStr{tempConVar->getString().c_str()};
            const auto hash{Bancho::md5((u8 *)tempStr, strlen(tempStr))};
            cv::mp_password_md5.setValue(hash.hash.data());
            tempConVar->setValue("");
        }
    }
    if(cv::mp_password_md5.getString().length() != 32) {
        // debugLog("no password, md5: {} length: {}\n", cv::mp_password_md5.getString().length(),
        // cv::mp_password_md5.getString());
        //  No password: don't try to log in
        return;
    }

    bancho->pw_md5 = {cv::mp_password_md5.getString().c_str()};
    //    bancho->pw_md5 = Bancho::md5((u8 *)pw, strlen(pw));

    // Admins told me they don't want score submission enabled
    constexpr auto submit_blacklist = std::array{
        "akatsuki.gg",
        "ripple.moe",
    };
    for(const char *endpoint : submit_blacklist) {
        if(!strcmp(endpoint, bancho->endpoint.c_str())) {
            bancho->score_submission_policy = ServerPolicy::NO;
            break;
        }
    }

    osu->getOptionsMenu()->setLoginLoadingState(true);
    try_logging_in = true;
}

void update_networking() {
    // Rate limit to every 1ms at most
    static double last_update = 0;
    double current_time = engine->getTime();
    if(current_time - last_update < 0.001) return;
    last_update = current_time;

    // Initialize last_packet_tms on first call
    static bool initialized = false;
    if(!initialized) {
        last_packet_tms = time(nullptr);
        initialized = true;
    }

    // Process API requests
    api_requests_mutex.lock();
    if(api_request_queue.empty()) {
        api_requests_mutex.unlock();
    } else {
        APIRequest api_out = api_request_queue.front();
        api_request_queue.erase(api_request_queue.begin());
        api_requests_mutex.unlock();

        send_api_request_async(api_out);
    }

    // Set ping timeout
    if(osu && osu->lobby->isVisible()) seconds_between_pings = 1;
    if(bancho->spectating) seconds_between_pings = 1;
    if(bancho->is_in_a_multi_room() && seconds_between_pings > 3) seconds_between_pings = 3;
    bool should_ping = difftime(time(nullptr), last_packet_tms) > seconds_between_pings;
    if(bancho->user_id <= 0) should_ping = false;

    // Handle login and outgoing packet processing
    if(try_logging_in) {
        try_logging_in = false;
        if(bancho->user_id <= 0) {
            Packet login = bancho->build_login_packet();
            send_bancho_packet_async(login);
        }
    } else if(should_ping && outgoing.pos == 0) {
        proto::write<u16>(&outgoing, PING);
        proto::write<u8>(&outgoing, 0);
        proto::write<u32>(&outgoing, 0);

        // Polling gets slower over time, but resets when we receive new data
        if(seconds_between_pings < 30.0) {
            seconds_between_pings += 1.0;
        }
    }

    if(outgoing.pos > 0) {
        Packet out = outgoing;
        outgoing = Packet();

        // DEBUG: If we're not sending the right amount of bytes, bancho.py just
        // chugs along! To try to detect it faster, we'll send two packets per request.
        proto::write<u16>(&out, PING);
        proto::write<u8>(&out, 0);
        proto::write<u32>(&out, 0);

        send_bancho_packet_async(out);
    }
}

void receive_api_responses() {
    std::scoped_lock lock(api_responses_mutex);
    while(!api_response_queue.empty()) {
        Packet incoming = api_response_queue.front();
        api_response_queue.erase(api_response_queue.begin());
        handle_api_response(incoming);
        free(incoming.memory);
        free(incoming.extra);
    }
}

void receive_bancho_packets() {
    std::scoped_lock lock(incoming_mutex);
    while(!incoming_queue.empty()) {
        Packet incoming = incoming_queue.front();
        incoming_queue.erase(incoming_queue.begin());
        bancho->handle_packet(&incoming);
        free(incoming.memory);
    }

    // Request presence/stats every second
    // XXX: Rather than every second, this should be done every time we're calling send_bancho_packet
    //      But that function is on the networking thread, so requires extra brain power to do correctly
    static f64 last_presence_request = engine->getTime();
    if(engine->getTime() > last_presence_request + 1.f) {
        last_presence_request = engine->getTime();

        BANCHO::User::request_presence_batch();

        if(!BANCHO::User::stats_requests.empty()) {
            Packet packet;
            packet.id = USER_STATS_REQUEST;
            proto::write<u16>(&packet, BANCHO::User::stats_requests.size());
            for(auto user_id : BANCHO::User::stats_requests) {
                proto::write<i32>(&packet, user_id);
            }
            send_packet(packet);

            BANCHO::User::stats_requests.clear();
        }
    }
}

void send_api_request(const APIRequest &request) {
    if(bancho->user_id <= 0) {
        debugLog("Cannot send API request of type {:d} since we are not logged in.\n",
                 static_cast<unsigned int>(request.type));
        return;
    }

    std::scoped_lock lock(api_requests_mutex);

    // Jank way to do things... remove outdated requests now
    std::erase_if(api_request_queue, [request](APIRequest r) { return r.type = request.type; });

    api_request_queue.push_back(request);
}

void send_packet(Packet &packet) {
    if(bancho->user_id <= 0) {
        // Don't queue any packets until we're logged in
        free(packet.memory);
        packet.memory = nullptr;
        packet.size = 0;
        return;
    }

    // debugLog("Sending packet of type {:}: ", packet.id);
    // for (int i = 0; i < packet.pos; i++) {
    //     Engine::logRaw("{:02x} ", packet.memory[i]);
    // }
    // Engine::logRaw("\n");

    // We're not sending it immediately, instead we just add it to the pile of
    // packets to send
    proto::write<u16>(&outgoing, packet.id);
    proto::write<u8>(&outgoing, 0);
    proto::write<u32>(&outgoing, packet.pos);
    proto::write_bytes(&outgoing, packet.memory, packet.pos);

    free(packet.memory);
    packet.memory = nullptr;
    packet.size = 0;
}

void cleanup_networking() {
    // no thread to kill, just cleanup any remaining state
    try_logging_in = false;
    auth_header = "";
    free(outgoing.memory);
    outgoing = Packet();
}

}  // namespace BANCHO::Net
