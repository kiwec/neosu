// Copyright (c) 2025, kiwec, All rights reserved.
#include "AsyncIOHandler.h"
#include "BanchoApi.h"
#include "BanchoLeaderboard.h"
#include "Downloader.cpp"
#include "NetworkHandler.h"
#include "NotificationOverlay.h"
#include "SyncMutex.h"

// osu! private API
static Sync::mutex api_responses_mutex;
static std::vector<Packet> api_response_queue;

static void handle_api_response(Packet &packet) {
    switch(packet.id) {
        case BANCHO::Api::GET_BEATMAPSET_INFO: {
            Downloader::process_beatmapset_info_response(packet);
            break;
        }

        case BANCHO::Api::GET_MAP_LEADERBOARD: {
            BANCHO::Leaderboard::process_leaderboard_response(packet);
            break;
        }

        case BANCHO::Api::GET_REPLAY: {
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

            auto replay_path =
                fmt::format(NEOSU_REPLAYS_PATH "/{:s}/{:d}.replay.lzma", score->server.c_str(), score->unixTimestamp);

            // TODO: progress bars? how do we make sure the user doesnt do anything weird while its saving to disk and break the flow
            debugLog("Saving replay to {}...", replay_path);

            io->write(replay_path, packet.memory, packet.size, [to_watch = *score](bool success) {
                if(success) {
                    LegacyReplay::load_and_watch(to_watch);
                } else {
                    osu->getNotificationOverlay()->addToast("Failed to save replay", ERROR_TOAST);
                }
            });

            break;
        }

        case BANCHO::Api::MARK_AS_READ: {
            // (nothing to do)
            break;
        }

        default: {
            // NOTE: API Response type is same as API Request type
            debugLog("No handler for API response type {:d}!", packet.id);
        }
    }
}


void BANCHO::Api::send_request(const BANCHO::Api::Request &request) {
    if(BanchoState::get_uid() <= 0) {
        debugLog("Cannot send API request of type {:d} since we are not logged in.",
                 static_cast<unsigned int>(request.type));
        return;
    }

    NetworkHandler::RequestOptions options;
    options.timeout = 60;
    options.connectTimeout = 5;
    options.userAgent = "osu!";

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto query_url = UString::format("%sosu.%s%s", scheme, BanchoState::endpoint.c_str(), request.path.toUtf8());

    networkHandler->httpRequestAsync(
        query_url,
        [request](NetworkHandler::Response response) {
            Packet api_response;
            api_response.id = request.type;
            api_response.extra = request.extra;
            api_response.extra_int = request.extra_int;

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

void BANCHO::Api::update() {
    Sync::scoped_lock lock(api_responses_mutex);
    while(!api_response_queue.empty()) {
        Packet incoming = api_response_queue.front();
        api_response_queue.erase(api_response_queue.begin());
        handle_api_response(incoming);
        free(incoming.memory);
        free(incoming.extra);
    }
}

void BANCHO::Api::append_auth_params(UString &url, std::string user_param, std::string pw_param) {
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
