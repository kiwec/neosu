// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoLeaderboard.h"

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Database.h"
#include "Engine.h"
#include "ModSelector.h"
#include "SString.h"
#include "SongBrowser/SongBrowser.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include <curl/curl.h>

namespace {  // static namespace
FinishedScore parse_score(char *score_line) {
    FinishedScore score;
    score.client = "peppy-unknown";
    score.server = bancho->endpoint;

    auto tokens = SString::split(score_line, "|");
    if(tokens.size() < 15) return score;

    score.bancho_score_id = strtoull(tokens[0].c_str(), nullptr, 10);
    score.playerName = tokens[1].c_str();
    score.score = strtoull(tokens[2].c_str(), nullptr, 10);
    score.comboMax = static_cast<i32>(strtol(tokens[3].c_str(), nullptr, 10));
    score.num50s = static_cast<i32>(strtol(tokens[4].c_str(), nullptr, 10));
    score.num100s = static_cast<i32>(strtol(tokens[5].c_str(), nullptr, 10));
    score.num300s = static_cast<i32>(strtol(tokens[6].c_str(), nullptr, 10));
    score.numMisses = static_cast<i32>(strtol(tokens[7].c_str(), nullptr, 10));
    score.numKatus = static_cast<i32>(strtol(tokens[8].c_str(), nullptr, 10));
    score.numGekis = static_cast<i32>(strtol(tokens[9].c_str(), nullptr, 10));
    score.perfect = strtoul(tokens[10].c_str(), nullptr, 10) == 1;
    score.mods = Replay::Mods::from_legacy(static_cast<u32>(strtoul(tokens[11].c_str(), nullptr, 10)));
    score.player_id = static_cast<i32>(strtol(tokens[12].c_str(), nullptr, 10));
    score.unixTimestamp = strtoull(tokens[14].c_str(), nullptr, 10);

    // @PPV3: score can only be ppv2, AND we need to recompute ppv2 on it
    // might also be missing some important fields here, double check

    // Set username for given user id, since we now know both
    auto user = BANCHO::User::get_user_info(score.player_id);
    user->name = UString(score.playerName.c_str());

    // Mark as a player. Setting this also makes the has_user_info check pass,
    // which unlocks context menu actions such as sending private messages.
    user->privileges |= 1;

    return score;
}

}  // namespace

namespace BANCHO::Leaderboard {
void fetch_online_scores(DatabaseBeatmap *beatmap) {
    UString path = "/web/osu-osz2-getscores.php?s=0&vv=4&v=1";
    path.append(UString::fmt("&c={:s}", beatmap->getMD5Hash().hash.data()));

    std::string osu_file_path = beatmap->getFilePath();
    auto path_end = osu_file_path.find_last_of('/');
    if(path_end != std::string::npos) {
        osu_file_path.erase(0, path_end + 1);
    }

    CURL *curl = curl_easy_init();
    if(!curl) {
        debugLog("Failed to initialize cURL in fetch_online_scores()!\n");
        return;
    }
    char *encoded_filename = curl_easy_escape(curl, osu_file_path.c_str(), 0);
    if(!encoded_filename) {
        debugLog("Failed to encode map filename!\n");
        curl_easy_cleanup(curl);
        return;
    }
    path.append(UString::format("&f=%s", encoded_filename));
    curl_free(encoded_filename);
    curl_easy_cleanup(curl);
    path.append(UString::format("&m=0&i=%d&mods=%u&h=&a=0&us=%s&ha=%s", beatmap->getSetID(),
                                osu->modSelector->getModFlags(), bancho->get_username().c_str(), bancho->pw_md5.hash.data())
                    .toUtf8());

    APIRequest request;
    request.type = GET_MAP_LEADERBOARD;
    request.path = path;
    request.mime = nullptr;
    request.extra = (u8 *)strdup(beatmap->getMD5Hash().hash.data());

    BANCHO::Net::send_api_request(request);
}

void process_leaderboard_response(Packet response) {
    // Don't update the leaderboard while playing, that's weird
    if(osu->isInPlayMode()) return;

    // NOTE: We're not doing anything with the "info" struct.
    //       Server can return partial responses in some cases, so make sure
    //       you actually received the data if you plan on using it.
    OnlineMapInfo info{};
    MD5Hash beatmap_hash = (char *)response.extra;
    std::vector<FinishedScore> scores;
    char *body = (char *)response.memory;

    char *ranked_status = SString::strtok_x('|', &body);
    info.ranked_status = strtol(ranked_status, nullptr, 10);

    char *server_has_osz2 = SString::strtok_x('|', &body);
    info.server_has_osz2 = !strcmp(server_has_osz2, "true");

    char *beatmap_id = SString::strtok_x('|', &body);
    info.beatmap_id = strtoul(beatmap_id, nullptr, 10);

    char *beatmap_set_id = SString::strtok_x('|', &body);
    info.beatmap_set_id = strtoul(beatmap_set_id, nullptr, 10);

    char *nb_scores = SString::strtok_x('|', &body);
    info.nb_scores = static_cast<i32>(strtol(nb_scores, nullptr, 10));

    char *fa_track_id = SString::strtok_x('|', &body);
    (void)fa_track_id;

    char *fa_license_text = SString::strtok_x('\n', &body);
    (void)fa_license_text;

    char *online_offset = SString::strtok_x('\n', &body);
    info.online_offset = static_cast<i32>(strtol(online_offset, nullptr, 10));

    char *map_name = SString::strtok_x('\n', &body);
    (void)map_name;

    char *user_ratings = SString::strtok_x('\n', &body);
    (void)user_ratings;  // no longer used

    char *pb_score = SString::strtok_x('\n', &body);
    (void)pb_score;

    char* score_line = nullptr;
    while((score_line = SString::strtok_x('\n', &body))[0] != '\0') {
        FinishedScore score = parse_score(score_line);
        score.beatmap_hash = beatmap_hash;
        scores.push_back(score);
    }

    // XXX: We should also separately display either the "personal best" the server sent us,
    //      or the local best, depending on which score is better.
    debugLog("Received online leaderboard for Beatmap ID {:d}\n", info.beatmap_id);
    auto diff = db->getBeatmapDifficulty(beatmap_hash);
    if(diff) {
        diff->setOnlineOffset(info.online_offset);
    }
    db->online_scores[beatmap_hash] = std::move(scores);
    osu->getSongBrowser()->rebuildScoreButtons();
}
}  // namespace BANCHO::Leaderboard
