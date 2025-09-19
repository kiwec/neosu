// Copyright (c) 2024, kiwec, All rights reserved.
#include "BaseEnvironment.h"

#include "Bancho.h"
#include "BanchoAes.h"
#include "BanchoNetworking.h"
#include "BanchoSubmitter.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "NetworkHandler.h"
#include "SongBrowser/SongBrowser.h"
#include "crypto.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace BANCHO::Net {
void submit_score(FinishedScore score) {
    debugLog("Submitting score...\n");
    constexpr auto GRADES = std::array{"XH", "SH", "X", "S", "A", "B", "C", "D", "F", "N"};

    u8 iv[32];
    crypto::rng::get_rand(iv);

    NetworkHandler::RequestOptions options;
    options.timeout = 60;
    options.connectTimeout = 5;
    options.userAgent = "osu!";
    options.headers["token"] = BanchoState::cho_token.toUtf8();

    auto quit = fmt::format("{}", score.ragequit ? 1 : 0);
    options.mimeParts.push_back({
        .name = "x",
        .data = {quit.begin(), quit.end()},
    });

    auto fail_time = fmt::format("{}", score.passed ? 0 : score.play_time_ms);
    options.mimeParts.push_back({
        .name = "ft",
        .data = {fail_time.begin(), fail_time.end()},
    });

    auto score_time = fmt::format("{}", score.passed ? score.play_time_ms : 0);
    options.mimeParts.push_back({
        .name = "st",
        .data = {score_time.begin(), score_time.end()},
    });

    std::string visual_settings_b64 = "0";  // TODO: not used by bancho.py
    options.mimeParts.push_back({
        .name = "fs",
        .data = {visual_settings_b64.begin(), visual_settings_b64.end()},
    });

    std::string beatmap_hash = score.beatmap_hash.string();
    options.mimeParts.push_back({
        .name = "bmk",
        .data = {beatmap_hash.begin(), beatmap_hash.end()},
    });

    auto unique_ids = fmt::format("{}|{}", BanchoState::get_install_id().toUtf8(), BanchoState::get_disk_uuid().toUtf8());
    options.mimeParts.push_back({
        .name = "c1",
        .data = {unique_ids.begin(), unique_ids.end()},
    });

    std::string password = BanchoState::is_oauth ? BanchoState::cho_token.toUtf8() : BanchoState::pw_md5.string();
    options.mimeParts.push_back({
        .name = "pass",
        .data = {password.begin(), password.end()},
    });

    auto osu_version = fmt::format("{}", OSU_VERSION_DATEONLY);
    options.mimeParts.push_back({
        .name = "osuver",
        .data = {osu_version.begin(), osu_version.end()},
    });

    auto iv_b64 = crypto::conv::encode64(iv, sizeof(iv));
    options.mimeParts.push_back({
        .name = "iv",
        .data = {iv_b64.begin(), iv_b64.end()},
    });

    {
        size_t s_client_hashes_encrypted = 0;
        u8 *client_hashes_encrypted = BANCHO::AES::encrypt(
            iv, (u8 *)BanchoState::client_hashes.toUtf8(), BanchoState::client_hashes.lengthUtf8(), &s_client_hashes_encrypted);
        auto client_hashes_b64 = crypto::conv::encode64(client_hashes_encrypted, s_client_hashes_encrypted);
        options.mimeParts.push_back({
            .name = "s",
            .data = {client_hashes_b64.begin(), client_hashes_b64.end()},
        });
    }

    {
        std::string score_data;
        score_data.append(score.map->getMD5Hash().string());

        if(BanchoState::is_oauth) {
            score_data.append(":$token");
        } else {
            score_data.append(fmt::format(":{}", BanchoState::get_username()));
        }

        char score_time[80];
        struct tm *timeinfo = localtime((const time_t *)&score.unixTimestamp);
        strftime(score_time, sizeof(score_time), "%y%m%d%H%M%S", timeinfo);

        {
            auto idiot_check = fmt::format("chickenmcnuggets{}", score.num300s + score.num100s);
            idiot_check.append(fmt::format("o15{}{}", score.num50s, score.numGekis));
            idiot_check.append(fmt::format("smustard{}{}", score.numKatus, score.numMisses));
            idiot_check.append(fmt::format("uu{}", score.map->getMD5Hash().string()));
            idiot_check.append(fmt::format("{}{}", score.comboMax, score.perfect ? "True" : "False"));

            if(BanchoState::is_oauth) {
                idiot_check.append("$token");
            } else {
                idiot_check.append(BanchoState::get_username());
            }

            idiot_check.append(fmt::format("{}{}", score.score, GRADES[(int)score.grade]));
            idiot_check.append(fmt::format("{}Q{}", score.mods.to_legacy(), score.passed ? "True" : "False"));
            idiot_check.append(fmt::format("0{}{}", OSU_VERSION_DATEONLY, score_time));
            idiot_check.append(BanchoState::client_hashes.toUtf8());

            auto idiot_hash = BanchoState::md5((u8 *)idiot_check.data(), idiot_check.size());
            score_data.append(":");
            score_data.append(idiot_hash.string());
        }
        score_data.append(fmt::format(":{}", score.num300s));
        score_data.append(fmt::format(":{}", score.num100s));
        score_data.append(fmt::format(":{}", score.num50s));
        score_data.append(fmt::format(":{}", score.numGekis));
        score_data.append(fmt::format(":{}", score.numKatus));
        score_data.append(fmt::format(":{}", score.numMisses));
        score_data.append(fmt::format(":{}", score.score));
        score_data.append(fmt::format(":{}", score.comboMax));
        score_data.append(fmt::format(":{}", score.perfect ? "True" : "False"));
        score_data.append(fmt::format(":{}", GRADES[(int)score.grade]));
        score_data.append(fmt::format(":{}", score.mods.to_legacy()));
        score_data.append(fmt::format(":{}", score.passed ? "True" : "False"));
        score_data.append(":0");  // gamemode, always std
        score_data.append(fmt::format(":{}", score_time));
        score_data.append(":mcosu");  // anticheat flags

        size_t s_score_data_encrypted = 0;
        u8 *score_data_encrypted = BANCHO::AES::encrypt(iv, (u8 *)score_data.data(), score_data.size(), &s_score_data_encrypted);
        auto score_data_b64 = crypto::conv::encode64(score_data_encrypted, s_score_data_encrypted);
        delete[] score_data_encrypted;
        options.mimeParts.push_back({
            .name = "score",
            .data = {score_data_b64.begin(), score_data_b64.end()},
        });
    }

    {
        auto compressed_data = LegacyReplay::compress_frames(score.replay);
        if(compressed_data.size() <= 24) {
            debugLog("Replay too small to submit! Compressed size: {:d} bytes\n", compressed_data.size());
            return;
        }

        options.mimeParts.push_back({
            .filename = "replay",
            .name = "score",
            .data = compressed_data,
        });
    }

    {
        Packet packet;
        Proto::write_mods(packet, score.mods);
        auto mods_data_b64 = crypto::conv::encode64(packet.memory, packet.pos);

        options.mimeParts.push_back({
            .name = "neosu-mods",
            .data = {mods_data_b64.begin(), mods_data_b64.end()},
        });
    }

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto url = UString::fmt("{}osu.{}/web/osu-submit-modular-selector.php", scheme, BanchoState::endpoint);
    networkHandler->httpRequestAsync(
        url,
        [](NetworkHandler::Response response) {
            if(response.success) {
                // TODO: handle success (pp, etc + error codes)
                debugLog("Score submit result: {}\n", response.body);

                // Reset leaderboards so new score will appear
                db->online_scores.clear();
                osu->getSongBrowser()->rebuildScoreButtons();
            } else {
                // TODO: handle failure
            }
        },
        options
    );
}
}  // namespace BANCHO::Net
