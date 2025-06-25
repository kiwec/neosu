#include "BaseEnvironment.h"

#ifdef _WIN32
#include <minwinbase.h>
#include <wincrypt.h>
#else
#include <sys/random.h>
#endif

#ifndef LZMA_API_STATIC
#define LZMA_API_STATIC
#endif
#include <lzma.h>

#include "Bancho.h"
#include "BanchoAes.h"
#include "BanchoNetworking.h"
#include "BanchoSubmitter.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "base64.h"

void submit_score(FinishedScore score) {
    debugLog("Submitting score...\n");
    const char *GRADES[] = {"XH", "SH", "X", "S", "A", "B", "C", "D", "F", "N"};

    u8 *compressed_data = NULL;

    char score_time[80];
    struct tm *timeinfo = localtime((const time_t *)&score.unixTimestamp);
    strftime(score_time, sizeof(score_time), "%y%m%d%H%M%S", timeinfo);

    u8 iv[32];
#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptGenRandom(hCryptProv, sizeof(iv), iv);
    CryptReleaseContext(hCryptProv, 0);
#else
    getrandom(iv, sizeof(iv), 0);
#endif

    APIRequest request;
    request.type = SUBMIT_SCORE;
    request.path = "/web/osu-submit-modular-selector.php";

    CURL *curl = curl_easy_init();
    if(!curl) {
        debugLog("Failed to initialize cURL in submit_score()!\n");
        return;
    }

    // NOTE: cURL docs say it's ok to curl_mime_init on a curl handle
    //       different from the one used to send the request :)
    curl_mimepart *part = NULL;
    request.mime = curl_mime_init(curl);

    {
        auto quit = UString::format("%d", score.ragequit ? 1 : 0);
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "x");
        curl_mime_data(part, quit.toUtf8(), quit.lengthUtf8());
    }
    {
        auto fail_time = UString::format("%d", score.passed ? 0 : score.play_time_ms);
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "ft");
        curl_mime_data(part, fail_time.toUtf8(), fail_time.lengthUtf8());
    }
    {
        auto score_time = UString::format("%d", score.passed ? score.play_time_ms : 0);
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "st");
        curl_mime_data(part, score_time.toUtf8(), score_time.lengthUtf8());
    }
    {
        UString visual_settings_b64 = "0";  // TODO @kiwec: not used by bancho.py
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "fs");
        curl_mime_data(part, visual_settings_b64.toUtf8(), visual_settings_b64.lengthUtf8());
    }
    {
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "bmk");
        curl_mime_data(part, score.beatmap_hash.hash, CURL_ZERO_TERMINATED);
    }
    {
        auto unique_ids = UString::format("%s|%s", bancho.install_id.toUtf8(), bancho.disk_uuid.toUtf8());
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "c1");
        curl_mime_data(part, unique_ids.toUtf8(), unique_ids.lengthUtf8());
    }
    {
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "pass");
        curl_mime_data(part, bancho.pw_md5.hash, 32);
    }
    {
        auto osu_version = UString::format("%d", OSU_VERSION_DATEONLY);
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "osuver");
        curl_mime_data(part, osu_version.toUtf8(), osu_version.lengthUtf8());
    }
    {
        const char *iv_b64 = (const char *)base64_encode(iv, sizeof(iv), NULL);
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "iv");
        curl_mime_data(part, iv_b64, CURL_ZERO_TERMINATED);
        delete[] iv_b64;
    }
    {
        size_t s_client_hashes_encrypted = 0;
        u8 *client_hashes_encrypted = encrypt(iv, (u8 *)bancho.client_hashes.toUtf8(),
                                              bancho.client_hashes.lengthUtf8(), &s_client_hashes_encrypted);
        const char *client_hashes_b64 =
            (const char *)base64_encode(client_hashes_encrypted, s_client_hashes_encrypted, NULL);
        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "s");
        curl_mime_data(part, client_hashes_b64, CURL_ZERO_TERMINATED);
        delete[] client_hashes_b64;
    }
    {
        UString score_data;
        score_data.append(score.diff2->getMD5Hash().hash);
        score_data.append(UString::format(":%s", bancho.username.toUtf8()));
        {
            auto idiot_check = UString::format("chickenmcnuggets%d", score.num300s + score.num100s);
            idiot_check.append(UString::format("o15%d%d", score.num50s, score.numGekis));
            idiot_check.append(UString::format("smustard%d%d", score.numKatus, score.numMisses));
            idiot_check.append(UString::format("uu%s", score.diff2->getMD5Hash().toUtf8()));
            idiot_check.append(UString::format("%d%s", score.comboMax, score.perfect ? "True" : "False"));
            idiot_check.append(
                UString::format("%s%d%s", bancho.username.toUtf8(), score.score, GRADES[(int)score.grade]));
            idiot_check.append(UString::format("%dQ%s", score.mods.to_legacy(), score.passed ? "True" : "False"));
            idiot_check.append(UString::format("0%d%s", OSU_VERSION_DATEONLY, score_time));
            idiot_check.append(bancho.client_hashes);

            auto idiot_hash = md5((u8 *)idiot_check.toUtf8(), idiot_check.lengthUtf8());
            score_data.append(":");
            score_data.append(idiot_hash.hash);
        }
        score_data.append(UString::format(":%d", score.num300s));
        score_data.append(UString::format(":%d", score.num100s));
        score_data.append(UString::format(":%d", score.num50s));
        score_data.append(UString::format(":%d", score.numGekis));
        score_data.append(UString::format(":%d", score.numKatus));
        score_data.append(UString::format(":%d", score.numMisses));
        score_data.append(UString::format(":%d", score.score));
        score_data.append(UString::format(":%d", score.comboMax));
        score_data.append(UString::format(":%s", score.perfect ? "True" : "False"));
        score_data.append(UString::format(":%s", GRADES[(int)score.grade]));
        score_data.append(UString::format(":%d", score.mods.to_legacy()));
        score_data.append(UString::format(":%s", score.passed ? "True" : "False"));
        score_data.append(":0");  // gamemode, always std
        score_data.append(UString::format(":%s", score_time));
        score_data.append(":mcosu");  // anticheat flags

        size_t s_score_data_encrypted = 0;
        u8 *score_data_encrypted =
            encrypt(iv, (u8 *)score_data.toUtf8(), score_data.lengthUtf8(), &s_score_data_encrypted);
        const char *score_data_b64 = (const char *)base64_encode(score_data_encrypted, s_score_data_encrypted, NULL);

        part = curl_mime_addpart(request.mime);
        curl_mime_name(part, "score");
        curl_mime_data(part, score_data_b64, CURL_ZERO_TERMINATED);
        delete[] score_data_b64;
    }
    {
        size_t s_compressed_data = 0;
        LegacyReplay::compress_frames(score.replay, &compressed_data, &s_compressed_data);
        if(s_compressed_data <= 24) {
            debugLog("Replay too small to submit! Compressed size: %d bytes\n", s_compressed_data);
            goto err;
        }

        part = curl_mime_addpart(request.mime);
        curl_mime_filename(part, "replay");
        curl_mime_name(part, "score");
        curl_mime_data(part, (const char *)compressed_data, s_compressed_data);
        free(compressed_data);
    }

    send_api_request(request);
    curl_easy_cleanup(curl);
    return;

err:
    free(compressed_data);
    curl_mime_free(request.mime);
    curl_easy_cleanup(curl);
}
