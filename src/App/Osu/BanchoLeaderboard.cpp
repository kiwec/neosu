// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoLeaderboard.h"

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Database.h"
#include "Engine.h"
#include "ModSelector.h"
#include "Parsing.h"
#include "SongBrowser/SongBrowser.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {  // static namespace
FinishedScore parse_score(char *score_line) {
    FinishedScore score;
    score.client = "peppy-unknown";
    score.server = BanchoState::endpoint;

    auto tokens = SString::split(score_line, "|");
    if(tokens.size() < 15) return score;

    score.bancho_score_id = strtoll(tokens[0].c_str(), nullptr, 10);
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
    UString url = "/web/osu-osz2-getscores.php?m=0&s=0&vv=4&a=";

    // TODO: b.py calls this "map_package_hash", could be useful for storyboard-specific LBs
    //       (assuming it's some hash that includes all relevant map files)
    url.append("&h=");

    // TODO: leaderboard filters
    url.append("&v=1");

    // Map info
    std::string map_filename = env->getFileNameFromFilePath(beatmap->getFilePath());
    url.append(UString::fmt("&f={}", env->urlEncode(map_filename)));
    url.append(UString::fmt("&c={:s}", beatmap->getMD5Hash().hash.data()));
    url.append(UString::fmt("&i={}", beatmap->getSetID()));

    // Some servers use mod flags, even without any leaderboard filter active (e.g. for relax)
    url.append(UString::fmt("&mods={}", osu->modSelector->getModFlags()));

    if(BanchoState::is_grass) {
        url.append(UString::fmt("&us=$token&ha={}", env->urlEncode(BanchoState::cho_token.toUtf8())));
    } else {
        url.append(UString::fmt("&us={}&ha={:s}", BanchoState::get_username(), BanchoState::pw_md5.hash.data()));
    }

    APIRequest request;
    request.type = GET_MAP_LEADERBOARD;
    request.path = url;
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

    char *ranked_status = Parsing::strtok_x('|', &body);
    info.ranked_status = strtol(ranked_status, nullptr, 10);

    char *server_has_osz2 = Parsing::strtok_x('|', &body);
    info.server_has_osz2 = !strcmp(server_has_osz2, "true");

    char *beatmap_id = Parsing::strtok_x('|', &body);
    info.beatmap_id = strtoul(beatmap_id, nullptr, 10);

    char *beatmap_set_id = Parsing::strtok_x('|', &body);
    info.beatmap_set_id = strtoul(beatmap_set_id, nullptr, 10);

    char *nb_scores = Parsing::strtok_x('|', &body);
    info.nb_scores = static_cast<i32>(strtol(nb_scores, nullptr, 10));

    char *fa_track_id = Parsing::strtok_x('|', &body);
    (void)fa_track_id;

    char *fa_license_text = Parsing::strtok_x('\n', &body);
    (void)fa_license_text;

    char *online_offset = Parsing::strtok_x('\n', &body);
    info.online_offset = static_cast<i32>(strtol(online_offset, nullptr, 10));

    char *map_name = Parsing::strtok_x('\n', &body);
    (void)map_name;

    char *user_ratings = Parsing::strtok_x('\n', &body);
    (void)user_ratings;  // no longer used

    char *pb_score = Parsing::strtok_x('\n', &body);
    (void)pb_score;

    char* score_line = nullptr;
    while((score_line = Parsing::strtok_x('\n', &body))[0] != '\0') {
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
