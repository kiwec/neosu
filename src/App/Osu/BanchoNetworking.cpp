// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoNetworking.h"

#include "Bancho.h"
#include "BanchoProtocol.h"
#include "BanchoUsers.h"
#include "BeatmapInterface.h"
#include "Chat.h"
#include "ConVar.h"
#include "ConVarHandler.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "File.h"
#include "Lobby.h"
#include "NeosuUrl.h"
#include "NetworkHandler.h"
#include "OptionsMenu.h"
#include "ResourceManager.h"
#include "SongBrowser.h"
#include "UserCard.h"
#include "Logging.h"

#include <ctime>

#include <curl/curl.h>

// Bancho protocol

namespace BANCHO::Net {
namespace {  // static namespace

Packet outgoing;
time_t last_packet_tms = {0};
double seconds_between_pings{1.0};
std::string auth_token = "";
bool use_websockets = false;
std::shared_ptr<NetworkHandler::Websocket> websocket{nullptr};

void parse_packets(u8 *data, size_t s_data) {
    Packet batch = {
        .memory = data,
        .size = s_data,
        .pos = 0,
    };

    // + 7 for packet header
    while(batch.pos + 7 < batch.size) {
        u16 packet_id = batch.read<u16>();
        batch.pos++;  // skip compression flag
        u32 packet_len = batch.read<u32>();

        if(packet_len > 10485760) {
            debugLog("Received a packet over 10Mb! Dropping response.");
            break;
        }

        if(batch.pos + packet_len > batch.size) break;

        Packet incoming = {
            .id = packet_id,
            .memory = batch.memory + batch.pos,
            .size = packet_len,
            .pos = 0,
        };
        BanchoState::handle_packet(incoming);

        seconds_between_pings = 1.0;
        batch.pos += packet_len;
    }
}

void attempt_logging_in() {
    assert(BanchoState::get_uid() <= 0);

    NetworkHandler::RequestOptions options;
    options.timeout = 30;
    options.connect_timeout = 5;
    options.user_agent = "osu!";
    options.headers["x-mcosu-ver"] = BanchoState::neosu_version.toUtf8();
    options.post_data = BanchoState::build_login_packet();

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto query_url = fmt::format("{:s}c.{:s}/", scheme, BanchoState::endpoint);

    last_packet_tms = time(nullptr);

    networkHandler->httpRequestAsync(
        query_url,
        [func = __FUNCTION__](NetworkHandler::Response response) {
            if(!response.success) {
                // TODO: shows "HTTP 0" on curl/network errors!
                auto errmsg = UString::format("Failed to log in: HTTP %ld", response.response_code);
                osu->getNotificationOverlay()->addToast(errmsg, ERROR_TOAST);
                return;
            }

            // Update auth token
            auto cho_token_it = response.headers.find("cho-token");
            if(cho_token_it != response.headers.end()) {
                auth_token = cho_token_it->second;
                BanchoState::cho_token = UString(cho_token_it->second);
                use_websockets = cv::prefer_websockets.getBool();
            }

            auto features_it = response.headers.find("x-mcosu-features");
            if(features_it != response.headers.end()) {
                if(strstr(features_it->second.c_str(), "submit=0") != nullptr) {
                    BanchoState::score_submission_policy = ServerPolicy::NO;
                    debugLogLambda("Server doesn't want score submission. :(");
                } else if(strstr(features_it->second.c_str(), "submit=1") != nullptr) {
                    BanchoState::score_submission_policy = ServerPolicy::YES;
                    debugLogLambda("Server wants score submission! :D");
                }
            }

            parse_packets((u8 *)response.body.data(), response.body.length());
        },
        options);
}

void send_bancho_packet_http(Packet outgoing) {
    if(auth_token.empty()) return;

    NetworkHandler::RequestOptions options;
    options.timeout = 30;
    options.connect_timeout = 5;
    options.user_agent = "osu!";
    options.headers["x-mcosu-ver"] = BanchoState::neosu_version.toUtf8();
    options.headers["osu-token"] = auth_token;

    // copy outgoing packet data for POST
    options.post_data = std::string(reinterpret_cast<char *>(outgoing.memory), outgoing.pos);

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto query_url = fmt::format("{:s}c.{:s}/", scheme, BanchoState::endpoint);

    networkHandler->httpRequestAsync(
        query_url,
        [func = __FUNCTION__](NetworkHandler::Response response) {
            if(!response.success) {
                debugLogLambda("Failed to send packet, HTTP error {}", response.response_code);
                return;
            }

            parse_packets((u8 *)response.body.data(), response.body.length());
        },
        options);
}

void send_bancho_packet_ws(Packet outgoing) {
    if(auth_token.empty()) return;

    if(websocket == nullptr || websocket->status == NetworkHandler::WEBSOCKET_DISCONNECTED) {
        // We have been disconnected in less than 5 seconds.
        // Don't try to reconnect, server clearly doesn't want us to.
        // (without this, we would be spamming retries every frame)
        if(websocket && websocket->time_created + 5.0 > engine->getTime()) {
            // XXX: dropping websocket->out here
            use_websockets = false;
            send_bancho_packet_http(outgoing);
            return;
        }

        NetworkHandler::WebsocketOptions options;
        options.user_agent = "osu!";
        options.headers["x-mcosu-ver"] = BanchoState::neosu_version.toUtf8();
        options.headers["osu-token"] = auth_token;

        auto scheme = cv::use_https.getBool() ? "wss://" : "ws://";
        options.url = fmt::format("{}c.{}/ws/", scheme, BanchoState::endpoint);

        auto new_websocket = networkHandler->initWebsocket(options);
        if(websocket != nullptr) new_websocket->out = websocket->out;  // don't lose outgoing packet queue
        websocket = new_websocket;
    }

    if(websocket->status == NetworkHandler::WEBSOCKET_UNSUPPORTED) {
        // fallback to http!
        use_websockets = false;
        send_bancho_packet_http(outgoing);
    } else {
        // enqueue packets to be sent
        websocket->out.insert(websocket->out.end(), outgoing.memory, outgoing.memory + outgoing.pos);
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

    // Set ping timeout
    if(osu && osu->getLobby()->isVisible()) seconds_between_pings = 1;
    if(BanchoState::spectating) seconds_between_pings = 1;
    if(BanchoState::is_in_a_multi_room() && seconds_between_pings > 3) seconds_between_pings = 3;
    if(use_websockets) seconds_between_pings = 30;

    bool should_ping = difftime(time(nullptr), last_packet_tms) > seconds_between_pings;
    if(BanchoState::get_uid() <= 0) return;

    // Append missing presence/stats request packets
    // XXX: Rather than every second, this should be done once, and only once
    //      (if we remove the check, right now it could spam 1000x/second)
    static f64 last_presence_request = engine->getTime();
    if(engine->getTime() > last_presence_request + 1.f) {
        last_presence_request = engine->getTime();

        BANCHO::User::request_presence_batch();
        BANCHO::User::request_stats_batch();
    }

    // Handle login and outgoing packet processing
    if(should_ping && outgoing.pos == 0) {
        outgoing.write<u16>(PING);
        outgoing.write<u8>(0);
        outgoing.write<u32>(0);

        // Polling gets slower over time, but resets when we receive new data
        if(seconds_between_pings < 30.0) {
            seconds_between_pings += 1.0;
        }
    }

    if(outgoing.pos > 0) {
        last_packet_tms = time(nullptr);

        Packet out = outgoing;
        outgoing = Packet();

        // DEBUG: If we're not sending the right amount of bytes, bancho.py just
        // chugs along! To try to detect it faster, we'll send two packets per request.
        out.write<u16>(PING);
        out.write<u8>(0);
        out.write<u32>(0);

        if(use_websockets) {
            send_bancho_packet_ws(out);
        } else {
            send_bancho_packet_http(out);
        }
        free(out.memory);
    }

    if(websocket && !websocket->in.empty()) {
        parse_packets((u8 *)websocket->in.data(), websocket->in.size());
        websocket->in.clear();
    }
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
    outgoing.write<u16>(packet.id);
    outgoing.write<u8>(0);
    outgoing.write<u32>(packet.pos);

    // Some packets have an empty payload
    if(packet.memory != nullptr) {
        outgoing.write_bytes(packet.memory, packet.pos);
        free(packet.memory);
    }

    packet.memory = nullptr;
    packet.size = 0;
}

void cleanup_networking() {
    // no thread to kill, just cleanup any remaining state
    auth_token = "";
    free(outgoing.memory);
    outgoing = Packet();
}

}  // namespace BANCHO::Net

void BanchoState::disconnect() {
    cvars->resetServerCvars();

    // Logout
    // This is a blocking call, but we *do* want this to block when quitting the game.
    if(BanchoState::is_online()) {
        Packet packet;
        packet.write<u16>(LOGOUT);
        packet.write<u8>(0);
        packet.write<u32>(4);
        packet.write<u32>(0);

        NetworkHandler::RequestOptions options;
        options.timeout = 5;
        options.connect_timeout = 5;
        options.user_agent = "osu!";
        options.post_data = std::string(reinterpret_cast<char *>(packet.memory), packet.pos);
        options.headers["x-mcosu-ver"] = BanchoState::neosu_version.toUtf8();
        options.headers["osu-token"] = BANCHO::Net::auth_token;
        BANCHO::Net::auth_token = "";

        auto scheme = cv::use_https.getBool() ? "https://" : "http://";
        auto query_url = fmt::format("{:s}c.{:s}/", scheme, BanchoState::endpoint);

        // use sync request for logout to ensure it completes
        NetworkHandler::Response response = networkHandler->httpRequestSynchronous(query_url, options);

        free(packet.memory);
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
    if(strlen(cv::mp_password_md5.getString().c_str()) == 32) {
        BanchoState::pw_md5 = {cv::mp_password_md5.getString().c_str()};
    }

    // Admins told me they don't want any clients to connect
    static constexpr const auto server_blacklist = std::array{
        "ppy.sh"sv,  // haven't asked, but the answer is obvious
        "gatari.pw"sv,
    };

    if(std::ranges::contains(server_blacklist, BanchoState::endpoint)) {
        osu->getNotificationOverlay()->addToast(u"This server does not allow neosu clients.", ERROR_TOAST);
        return;
    }

    // Admins told me they don't want score submission enabled
    static constexpr const auto submit_blacklist = std::array{
        "akatsuki.gg"sv,
        "ripple.moe"sv,
    };

    if(std::ranges::contains(submit_blacklist, BanchoState::endpoint)) {
        BanchoState::score_submission_policy = ServerPolicy::NO;
    }

    osu->getOptionsMenu()->setLoginLoadingState(true);

    BANCHO::Net::attempt_logging_in();
}
