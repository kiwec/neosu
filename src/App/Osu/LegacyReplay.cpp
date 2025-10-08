// Copyright (c) 2016, PG, All rights reserved.
#include "LegacyReplay.h"

#ifndef LZMA_API_STATIC
#define LZMA_API_STATIC
#endif
#include <lzma.h>

#include "ConVar.h"
#include "Bancho.h"
#include "BanchoApi.h"
#include "File.h"
#include "BeatmapInterface.h"
#include "Database.h"
#include "Engine.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "SongBrowser.h"
#include "score.h"
#include "Parsing.h"
#include "Logging.h"

#include <cstdlib>
#include <string>

namespace LegacyReplay {

BEATMAP_VALUES getBeatmapValuesForModsLegacy(u32 modsLegacy, float legacyAR, float legacyCS, float legacyOD,
                                             float legacyHP) {
    BEATMAP_VALUES v;

    // HACKHACK: code duplication, see Osu::getDifficultyMultiplier()
    v.difficultyMultiplier = 1.0f;
    {
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::HardRock)) v.difficultyMultiplier = 1.4f;
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Easy)) v.difficultyMultiplier = 0.5f;
    }

    // HACKHACK: code duplication, see Osu::getCSDifficultyMultiplier()
    v.csDifficultyMultiplier = 1.0f;
    {
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::HardRock)) v.csDifficultyMultiplier = 1.3f;  // different!
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Easy)) v.csDifficultyMultiplier = 0.5f;
    }

    // apply legacy mods to legacy beatmap values
    v.AR = std::clamp<float>(legacyAR * v.difficultyMultiplier, 0.0f, 10.0f);
    v.CS = std::clamp<float>(legacyCS * v.csDifficultyMultiplier, 0.0f, 10.0f);
    v.OD = std::clamp<float>(legacyOD * v.difficultyMultiplier, 0.0f, 10.0f);
    v.HP = std::clamp<float>(legacyHP * v.difficultyMultiplier, 0.0f, 10.0f);

    return v;
}

std::vector<Frame> get_frames(u8* replay_data, i32 replay_size) {
    std::vector<Frame> replay_frames;
    if(replay_size <= 0) return replay_frames;

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_alone_decoder(&strm, UINT64_MAX);
    if(ret != LZMA_OK) {
        debugLog("Failed to init lzma library ({:d})", static_cast<unsigned int>(ret));
        return replay_frames;
    }

    i64 cur_music_pos = 0;
    std::array<u8, BUFSIZ> outbuf;
    Packet output;
    strm.next_in = replay_data;
    strm.avail_in = replay_size;
    do {
        strm.next_out = outbuf.data();
        strm.avail_out = outbuf.size();

        ret = lzma_code(&strm, LZMA_FINISH);
        if(ret != LZMA_OK && ret != LZMA_STREAM_END) {
            debugLog("Decompression error ({:d})", static_cast<unsigned int>(ret));
            goto end;
        }

        output.write_bytes(outbuf.data(), outbuf.size() - strm.avail_out);
    } while(strm.avail_out == 0);
    output.write<u8>('\0');

    {
        char* line = (char*)output.memory;
        while(*line) {
            Frame frame;

            char* ms = Parsing::strtok_x('|', &line);
            frame.milliseconds_since_last_frame = strtoll(ms, nullptr, 10);

            char* x = Parsing::strtok_x('|', &line);
            frame.x = strtof(x, nullptr);

            char* y = Parsing::strtok_x('|', &line);
            frame.y = strtof(y, nullptr);

            char* flags = Parsing::strtok_x(',', &line);
            frame.key_flags = static_cast<u8>(strtoul(flags, nullptr, 10));

            if(frame.milliseconds_since_last_frame != -12345) {
                cur_music_pos += frame.milliseconds_since_last_frame;
                frame.cur_music_pos = cur_music_pos;
                replay_frames.push_back(frame);
            }
        }
    }

end:
    free(output.memory);
    lzma_end(&strm);
    return replay_frames;
}

std::vector<u8> compress_frames(const std::vector<Frame>& frames) {
    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_options_lzma options;
    lzma_lzma_preset(&options, LZMA_PRESET_DEFAULT);
    lzma_ret ret = lzma_alone_encoder(&stream, &options);
    if(ret != LZMA_OK) {
        debugLog("Failed to initialize lzma encoder: error {:d}", static_cast<unsigned int>(ret));
        return {};
    }

    std::string replay_string;
    for(auto frame : frames) {
        auto frame_str = UString::format("%lld|%.4f|%.4f|%hhu,", frame.milliseconds_since_last_frame, frame.x, frame.y,
                                         frame.key_flags);
        replay_string.append(frame_str.toUtf8(), frame_str.lengthUtf8());
    }

    // osu!stable doesn't consider a replay valid unless it ends with this
    replay_string.append("-12345|0.0000|0.0000|0,");

    std::vector<u8> compressed;
    compressed.resize(replay_string.length());

    stream.avail_in = replay_string.length();
    stream.next_in = (const u8*)replay_string.c_str();
    stream.avail_out = compressed.size();
    stream.next_out = compressed.data();
    do {
        ret = lzma_code(&stream, LZMA_FINISH);
        if(ret == LZMA_OK) {
            compressed.resize(compressed.size() * 2);
            stream.avail_out = compressed.size() - stream.total_out;
            stream.next_out = compressed.data() + stream.total_out;
        } else if(ret != LZMA_STREAM_END) {
            debugLog("Error while compressing replay: error {:d}", static_cast<unsigned int>(ret));
            stream.total_out = 0;
            break;
        }
    } while(ret != LZMA_STREAM_END);

    compressed.resize(stream.total_out);
    lzma_end(&stream);
    return compressed;
}

Info from_bytes(u8* data, int s_data) {
    Info info;

    Packet replay;
    replay.memory = data;
    replay.size = s_data;

    info.gamemode = replay.read<u8>();
    if(info.gamemode != 0) {
        debugLog("Replay has unexpected gamemode {:d}!", info.gamemode);
        return info;
    }

    info.osu_version = replay.read<u32>();
    info.map_md5 = replay.read_string();
    info.username = replay.read_string();
    info.replay_md5 = replay.read_string();
    info.num300s = replay.read<u16>();
    info.num100s = replay.read<u16>();
    info.num50s = replay.read<u16>();
    info.numGekis = replay.read<u16>();
    info.numKatus = replay.read<u16>();
    info.numMisses = replay.read<u16>();
    info.score = replay.read<i32>();
    info.comboMax = replay.read<u16>();
    info.perfect = replay.read<u8>();
    info.mod_flags = replay.read<u32>();
    info.life_bar_graph = replay.read_string();
    info.timestamp = replay.read<i64>() / 10LL;

    i32 replay_size = replay.read<i32>();
    if(replay_size <= 0) return info;
    auto replay_data = new u8[replay_size];
    replay.read_bytes(replay_data, replay_size);
    info.frames = get_frames(replay_data, replay_size);
    delete[] replay_data;

    // https://github.com/ppy/osu/blob/a0e300c3/osu.Game/Scoring/Legacy/LegacyScoreDecoder.cs
    if(info.osu_version >= 20140721) {
        info.bancho_score_id = replay.read<i64>();
    } else if(info.osu_version >= 20121008) {
        info.bancho_score_id = replay.read<i32>();
    }

    // XXX: handle lazer replay data (versions 30000001 to 30000016)
    // XXX: handle neosu replay data (versions 40000000+?)

    return info;
}

bool load_from_disk(FinishedScore& score, bool update_db) {
    if(score.peppy_replay_tms > 0) {
        auto osu_folder = cv::osu_folder.getString();
        auto path =
            fmt::format("{:s}/Data/r/{:s}-{:d}.osr", osu_folder, score.beatmap_hash.string(), score.peppy_replay_tms);

        FILE* replay_file = File::fopen_c(path.c_str(), "rb");
        if(replay_file == nullptr) return false;

        fseek(replay_file, 0, SEEK_END);
        size_t s_full_replay = ftell(replay_file);
        rewind(replay_file);

        u8* full_replay = new u8[s_full_replay];
        fread(full_replay, s_full_replay, 1, replay_file);
        fclose(replay_file);
        auto info = from_bytes(full_replay, s_full_replay);
        score.replay = info.frames;
        delete[] full_replay;
    } else {
        auto path = fmt::format(NEOSU_REPLAYS_PATH "/{:s}/{:d}.replay.lzma", score.server, score.unixTimestamp);

        FILE* replay_file = File::fopen_c(path.c_str(), "rb");
        if(replay_file == nullptr) return false;

        fseek(replay_file, 0, SEEK_END);
        size_t s_compressed_replay = ftell(replay_file);
        rewind(replay_file);

        u8* compressed_replay = new u8[s_compressed_replay];
        fread(compressed_replay, s_compressed_replay, 1, replay_file);
        fclose(replay_file);
        score.replay = get_frames(compressed_replay, s_compressed_replay);
        delete[] compressed_replay;
    }

    if(update_db) {
        Sync::scoped_lock lock(db->scores_mtx);
        auto& map_scores = (*(db->getScores()))[score.beatmap_hash];
        for(auto& db_score : map_scores) {
            if(db_score.unixTimestamp != score.unixTimestamp) continue;
            if(&db_score != &score) {  // interesting logic...?
                db_score.replay = score.replay;
            }

            break;
        }
    }

    return true;
}

void load_and_watch(FinishedScore score) {
    // Check if replay is loaded
    if(score.replay.empty()) {
        if(!load_from_disk(score, true)) {
            if(score.server.c_str() != BanchoState::endpoint) {
                auto msg = UString::format("Please connect to %s to view this replay!", score.server.c_str());
                osu->getNotificationOverlay()->addToast(msg, ERROR_TOAST);
            }

            // Need to allocate with calloc since BANCHO::Api::Requests free() the .extra
            void* mem = calloc(1, sizeof(FinishedScore));
            auto* score_cpy = new(mem) FinishedScore;
            *score_cpy = score;

            std::string url{"/web/osu-getreplay.php?m=0"};
            url.append(fmt::format("&c={}", score.bancho_score_id));
            BANCHO::Api::append_auth_params(url);

            BANCHO::Api::Request request;
            request.type = BANCHO::Api::GET_REPLAY;
            request.path = url;
            request.extra = (u8*)score_cpy;
            BANCHO::Api::send_request(request);

            osu->getNotificationOverlay()->addNotification("Downloading replay...");
            return;
        }
    }

    // We tried loading from memory, we tried loading from file, we tried loading from server... RIP
    if(score.replay.empty()) {
        osu->getNotificationOverlay()->addToast(u"Failed to load replay", ERROR_TOAST);
        return;
    }

    auto map = db->getBeatmapDifficulty(score.beatmap_hash);
    if(map == nullptr) {
        // XXX: Auto-download beatmap
        osu->getNotificationOverlay()->addToast(u"Missing beatmap for this replay", ERROR_TOAST);
    } else {
        osu->getSongBrowser()->onDifficultySelected(map, false);
        osu->getSongBrowser()->selectSelectedBeatmapSongButton();
        osu->getMapInterface()->watch(score, 0);
    }
}

}  // namespace LegacyReplay
