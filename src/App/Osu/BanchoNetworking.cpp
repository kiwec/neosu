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
#include "BeatmapInterface.h"
#include "Chat.h"
#include "ConVar.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "File.h"
#include "Lobby.h"
#include "NeosuUrl.h"
#include "NetworkHandler.h"
#include "OptionsMenu.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "UIButton.h"
#include "UserCard.h"
#include "Logging.h"

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
    auto query_url = UString::format("%sosu.%s%s", scheme, BanchoState::endpoint.c_str(), api_out.path.toUtf8());

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
        },
        options);
}

void send_bancho_packet_async(Packet outgoing) {
    NetworkHandler::RequestOptions options;
    options.timeout = 30;
    options.connectTimeout = 5;
    options.userAgent = "osu!";
    options.headers["x-mcosu-ver"] = BanchoState::neosu_version.toUtf8();

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
    auto query_url = UString::format("%sc.%s/", scheme, BanchoState::endpoint.c_str());

    last_packet_tms = time(nullptr);

    networkHandler->httpRequestAsync(
        query_url,
        [](NetworkHandler::Response response) {
            if(!response.success) {
                Logger::logRaw("[httpRequestAsync] Failed to send packet, HTTP error {}", response.responseCode);
                std::scoped_lock<std::mutex> lock{auth_mutex};
                if(auth_header.empty()) {
                    auto errmsg = UString::format("Failed to log in: HTTP %ld", response.responseCode);
                    osu->getNotificationOverlay()->addToast(errmsg, ERROR_TOAST);
                }
                return;
            }

            // // debug
            // if (cv::debug_network.getBool()) {
            //     Logger::logRaw("DEBUG headers:");
            //     for(const auto &headerstr : response.headers) {
            //         Logger::logRaw("{:s} {:s}", headerstr.first.c_str(), headerstr.second.c_str());
            //     }
            // }

            // Update auth token
            auto cho_token_it = response.headers.find("cho-token");
            if(cho_token_it != response.headers.end()) {
                std::scoped_lock<std::mutex> lock{auth_mutex};
                auth_header = "osu-token: " + cho_token_it->second;
                BanchoState::cho_token = UString(cho_token_it->second);
            }

            auto features_it = response.headers.find("x-mcosu-features");
            if(features_it != response.headers.end()) {
                if(strstr(features_it->second.c_str(), "submit=0") != nullptr) {
                    BanchoState::score_submission_policy = ServerPolicy::NO;
                    Logger::logRaw("[httpRequestAsync] Server doesn't want score submission. :(");
                } else if(strstr(features_it->second.c_str(), "submit=1") != nullptr) {
                    BanchoState::score_submission_policy = ServerPolicy::YES;
                    Logger::logRaw("[httpRequestAsync] Server wants score submission! :D");
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

            // + 7 for packet header
            while(response_packet.pos + 7 < response_packet.size) {
                u16 packet_id = proto::read<u16>(response_packet);
                response_packet.pos++;  // skip compression flag
                u32 packet_len = proto::read<u32>(response_packet);

                if(packet_len > 10485760) {
                    Logger::logRaw("[httpRequestAsync] Received a packet over 10Mb! Dropping response.");
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

void handle_api_response(Packet &packet) {
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
                osu->getNotificationOverlay()->addToast("Failed to download replay", ERROR_TOAST);
                break;
            }

            assert(packet.extra && "handle_api_response(GET_REPLAY) got invalid packet for replay data");
            // we should look into this if it ever gets hit, shouldn't crash either way but still... a warning is a warning
            assert(reinterpret_cast<uintptr_t>(packet.extra) % alignof(FinishedScore) == 0 &&
                   "handle_api_response(GET_REPLAY) packet.extra is not aligned");
            // excuse all the preprocessor bloat
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
            auto *score = reinterpret_cast<FinishedScore *>(packet.extra);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

            auto replay_path = UString::format(MCENGINE_DATA_DIR "replays/%s/%d.replay.lzma", score->server.c_str(),
                                               score->unixTimestamp);

            // XXX: this is blocking main thread
            FILE *replay_file = fopen(replay_path.toUtf8(), "wb");
            if(replay_file == nullptr) {
                osu->getNotificationOverlay()->addToast("Failed to save replay", ERROR_TOAST);
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

        default: {
            // NOTE: API Response type is same as API Request type
            debugLog("No handler for API response type {:d}!", packet.id);
        }
    }
}

}  // namespace

// Used as fallback for Linux or other setups where neosu:// protocol handler doesn't work
void complete_oauth(std::string_view code) {
    auto url = fmt::format("neosu://login/{}/{}", cv::mp_server.getString(), code);
    debugLog("Manually logging in: {}", url);
    handle_neosu_url(url.c_str());
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
    if(osu && osu->getLobby()->isVisible()) seconds_between_pings = 1;
    if(BanchoState::spectating) seconds_between_pings = 1;
    if(BanchoState::is_in_a_multi_room() && seconds_between_pings > 3) seconds_between_pings = 3;
    bool should_ping = difftime(time(nullptr), last_packet_tms) > seconds_between_pings;
    if(BanchoState::get_uid() <= 0) should_ping = false;

    // Handle login and outgoing packet processing
    if(try_logging_in) {
        try_logging_in = false;
        if(BanchoState::get_uid() <= 0) {
            Packet login = BanchoState::build_login_packet();
            send_bancho_packet_async(login);
        }
    } else if(should_ping && outgoing.pos == 0) {
        proto::write<u16>(outgoing, PING);
        proto::write<u8>(outgoing, 0);
        proto::write<u32>(outgoing, 0);

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
        proto::write<u16>(out, PING);
        proto::write<u8>(out, 0);
        proto::write<u32>(out, 0);

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
        BanchoState::handle_packet(incoming);
        free(incoming.memory);
    }

    // Request presence/stats every second
    // XXX: Rather than every second, this should be done every time we're calling send_bancho_packet
    //      But that function is on the networking thread, so requires extra brain power to do correctly
    static f64 last_presence_request = engine->getTime();
    if(engine->getTime() > last_presence_request + 1.f) {
        last_presence_request = engine->getTime();

        BANCHO::User::request_presence_batch();
        BANCHO::User::request_stats_batch();
    }
}

void send_api_request(const APIRequest &request) {
    if(BanchoState::get_uid() <= 0) {
        debugLog("Cannot send API request of type {:d} since we are not logged in.",
                 static_cast<unsigned int>(request.type));
        return;
    }

    std::scoped_lock lock(api_requests_mutex);

    // Jank way to do things... remove outdated requests now
    std::erase_if(api_request_queue, [request](APIRequest r) { return r.type = request.type; });

    api_request_queue.push_back(request);
}

void send_packet(Packet &packet) {
    if(BanchoState::get_uid() <= 0) {
        // Don't queue any packets until we're logged in
        free(packet.memory);
        packet.memory = nullptr;
        packet.size = 0;
        return;
    }

    // debugLog("Sending packet of type {:}: ", packet.id);
    // for (int i = 0; i < packet.pos; i++) {
    //     Logger::logRaw("{:02x} ", packet.memory[i]);
    // }
    // Logger::logRaw("");

    // We're not sending it immediately, instead we just add it to the pile of
    // packets to send
    proto::write<u16>(outgoing, packet.id);
    proto::write<u8>(outgoing, 0);
    proto::write<u32>(outgoing, packet.pos);

    // Some packets have an empty payload
    if(packet.memory != nullptr) {
        proto::write_bytes(outgoing, packet.memory, packet.pos);
        free(packet.memory);
    }

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

void append_auth_params(UString& url, std::string user_param, std::string pw_param) {
    std::string user, pw;
    if(BanchoState::is_oauth) {
        user = "$token";
        pw = BanchoState::cho_token.toUtf8();
    } else {
        user = BanchoState::get_username();
        pw = BanchoState::pw_md5.string();
    }

    url.append(UString::fmt("&{}={}&{}={}", user_param, user, pw_param, pw));
}

}  // namespace BANCHO::Net

void BanchoState::disconnect() {
    cvars->resetServerCvars();

    // Logout
    // This is a blocking call, but we *do* want this to block when quitting the game.
    if(BanchoState::is_online()) {
        Packet packet;
        proto::write<u16>(packet, LOGOUT);
        proto::write<u8>(packet, 0);
        proto::write<u32>(packet, 4);
        proto::write<u32>(packet, 0);

        NetworkHandler::RequestOptions options;
        options.timeout = 5;
        options.connectTimeout = 5;
        options.userAgent = "osu!";
        options.postData = std::string(reinterpret_cast<char *>(packet.memory), packet.pos);
        options.headers["x-mcosu-ver"] = BanchoState::neosu_version.toUtf8();

        {
            std::scoped_lock<std::mutex> lock{BANCHO::Net::auth_mutex};
            if(!BANCHO::Net::auth_header.empty()) {
                size_t colon_pos = BANCHO::Net::auth_header.find(':');
                if(colon_pos != std::string::npos) {
                    std::string token = BANCHO::Net::auth_header.substr(colon_pos + 1);
                    SString::trim(&token);
                    options.headers["osu-token"] = token;
                }
            }
        }

        auto scheme = cv::use_https.getBool() ? "https://" : "http://";
        auto query_url = UString::format("%sc.%s/", scheme, BanchoState::endpoint.c_str());

        // use sync request for logout to ensure it completes
        NetworkHandler::Response response = networkHandler->performSyncRequest(query_url, options);

        free(packet.memory);
    }

    BANCHO::Net::try_logging_in = false;
    {
        std::scoped_lock<std::mutex> lock2{BANCHO::Net::auth_mutex};
        BANCHO::Net::auth_header = "";
    }
    free(BANCHO::Net::outgoing.memory);
    BANCHO::Net::outgoing = Packet();

    BanchoState::set_uid(0);
    osu->getUserButton()->setID(0);

    BanchoState::is_oauth = false;
    BanchoState::endpoint = "";
    BanchoState::spectating = false;
    BanchoState::spectated_player_id = 0;
    BanchoState::spectators.clear();
    BanchoState::fellow_spectators.clear();
    BanchoState::server_icon_url = "";
    if(BanchoState::server_icon != nullptr) {
        resourceManager->destroyResource(BanchoState::server_icon);
        BanchoState::server_icon = nullptr;
    }

    BanchoState::score_submission_policy = ServerPolicy::NO_PREFERENCE;
    osu->getOptionsMenu()->update_login_button();
    osu->getOptionsMenu()->setLoginLoadingState(false);
    osu->getOptionsMenu()->scheduleLayoutUpdate();

    BANCHO::User::logout_all_users();
    osu->getChat()->onDisconnect();
    osu->getSongBrowser()->onFilterScoresChange("Local", SongBrowser::LOGIN_STATE_FILTER_ID);

    Downloader::abort_downloads();
}

void BanchoState::reconnect() {
    BanchoState::disconnect();

    // Disable autologin, in case there's an error while logging in
    // Will be reenabled after the login succeeds
    cv::mp_autologin.setValue(false);

    // XXX: Put this in cv::mp_password callback?
    if(!cv::mp_password.getString().empty()) {
        const char *password = cv::mp_password.getString().c_str();
        const auto hash{BanchoState::md5((u8 *)password, strlen(password))};
        cv::mp_password_md5.setValue(hash.string());
        cv::mp_password.setValue("");
    }

    BanchoState::endpoint = cv::mp_server.getString();
    BanchoState::username = cv::name.getString().c_str();
    BanchoState::pw_md5 = {cv::mp_password_md5.getString().c_str()};

    // Admins told me they don't want any clients to connect
    static constexpr const auto server_blacklist = std::array{
        "ppy.sh"sv,  // haven't asked, but the answer is obvious
        "gatari.pw"sv,
    };

    if (std::ranges::contains(server_blacklist, BanchoState::endpoint)) {
        osu->getNotificationOverlay()->addToast("This server does not allow neosu clients.", ERROR_TOAST);
        return;
    }

    // Admins told me they don't want score submission enabled
    static constexpr const auto submit_blacklist = std::array{
        "akatsuki.gg"sv,
        "ripple.moe"sv,
    };

    if (std::ranges::contains(submit_blacklist, BanchoState::endpoint)) {
        BanchoState::score_submission_policy = ServerPolicy::NO;
    }

    osu->getOptionsMenu()->setLoginLoadingState(true);
    BANCHO::Net::try_logging_in = true;
}
