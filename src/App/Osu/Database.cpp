// Copyright (c) 2016, PG, All rights reserved.
#include "Database.h"

#include "AsyncIOHandler.h"
#include "Bancho.h"
#include "Parsing.h"
#include "SString.h"
#include "MD5Hash.h"
#include "ByteBufferedFile.h"
#include "Collections.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "DirectoryWatcher.h"
#include "Engine.h"
#include "File.h"
#include "LegacyReplay.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "AsyncPPCalculator.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "DiffCalc/MapCalcThread.h"
#include "DiffCalc/ScoreConverterThread.h"
#include "SongBrowser/SongBrowser.h"
#include "Timing.h"
#include "Logging.h"
#include "score.h"

#include <algorithm>
#include <unordered_set>

#include <algorithm>
#include <cstring>
#include <utility>

std::unique_ptr<Database> db = nullptr;

bool Database::sortScoreByScore(FinishedScore const &a, FinishedScore const &b) {
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool Database::sortScoreByCombo(FinishedScore const &a, FinishedScore const &b) {
    if(a.comboMax != b.comboMax) return a.comboMax > b.comboMax;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool Database::sortScoreByDate(FinishedScore const &a, FinishedScore const &b) {
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool Database::sortScoreByMisses(FinishedScore const &a, FinishedScore const &b) {
    if(a.numMisses != b.numMisses) return a.numMisses < b.numMisses;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool Database::sortScoreByAccuracy(FinishedScore const &a, FinishedScore const &b) {
    auto a_acc = LiveScore::calculateAccuracy(a.num300s, a.num100s, a.num50s, a.numMisses);
    auto b_acc = LiveScore::calculateAccuracy(b.num300s, b.num100s, b.num50s, b.numMisses);
    if(a_acc != b_acc) return a_acc > b_acc;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool Database::sortScoreByPP(FinishedScore const &a, FinishedScore const &b) {
    auto a_pp = std::max(a.get_pp() * 1000.0, 0.0);
    auto b_pp = std::max(b.get_pp() * 1000.0, 0.0);
    if(a_pp != b_pp) return a_pp > b_pp;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

// static helper
std::string Database::getDBPath(DatabaseType db_type) {
    static_assert(DatabaseType::LAST == DatabaseType::STABLE_MAPS, "add missing case to getDBPath");
    using enum DatabaseType;

    switch(db_type) {
        case INVALID_DB: {
            engine->showMessageError("Database Error",
                                     fmt::format("Invalid database type {}", static_cast<u8>(db_type)));
            return {""};
        }
        case NEOSU_SCORES:
            return NEOSU_DB_DIR "neosu_scores.db";
        case MCNEOSU_SCORES:
            return NEOSU_DB_DIR "scores.db";
        case MCNEOSU_COLLECTIONS:
            return NEOSU_DB_DIR "collections.db";
        case NEOSU_MAPS:
            return NEOSU_DB_DIR "neosu_maps.db";
        case STABLE_SCORES:
        case STABLE_COLLECTIONS:
        case STABLE_MAPS: {
            std::string osu_folder = cv::osu_folder.getString();
            if(osu_folder.back() != '/' && osu_folder.back() != '\\') osu_folder.push_back('/');
            switch(db_type) {
                case STABLE_SCORES:
                    return fmt::format("{}scores.db", osu_folder);
                case STABLE_COLLECTIONS:
                    // note the missing plural...
                    return fmt::format("{}collection.db", osu_folder);
                case STABLE_MAPS:
                    return fmt::format("{}osu!.db", osu_folder);
                default:
                    std::unreachable();
            }
        }
    }
    std::unreachable();
}

// static helper (for figuring out the type of external databases to be imported)
Database::DatabaseType Database::getDBType(std::string_view db_path) {
    std::string db_name = Environment::getFileNameFromFilePath(db_path);

    using enum DatabaseType;
    if(db_name == "collection.db") {
        // osu! collections
        return STABLE_COLLECTIONS;
    }
    if(db_name == "collections.db") {
        // mcosu/neosu collections
        return MCNEOSU_COLLECTIONS;
    }
    if(db_name == "neosu_scores.db") {
        // neosu!
        return NEOSU_SCORES;
    }

    if(db_name == "scores.db") {
        ByteBufferedFile::Reader score_db{db_path};
        u32 db_version = score_db.read<u32>();
        if(!score_db.good() || db_version == 0) {
            return INVALID_DB;
        }

        if(db_version == 20210106 || db_version == 20210108 || db_version == 20210110) {
            // McOsu 100%!
            return MCNEOSU_SCORES;
        } else {
            // We need to do some heuristics to detect whether this is an old neosu or a peppy database.
            u32 nb_beatmaps = score_db.read<u32>();
            for(uSz i = 0; i < nb_beatmaps; i++) {
                auto map_md5 = score_db.read_hash();
                (void)map_md5;
                u32 nb_scores = score_db.read<u32>();
                for(u32 j = 0; j < nb_scores; j++) {
                    /* u8 gamemode = */ score_db.skip<u8>();         // could check for 0xA9, but better method below
                    /* u32 score_version = */ score_db.skip<u32>();  // useless

                    // Here, neosu stores an int64 timestamp. First 32 bits should be 0 (until 2106).
                    // Meanwhile, peppy stores the beatmap hash, which will NEVER be 0, since
                    // it is stored as a string, which starts with an uleb128 (its length).
                    u32 timestamp_check = score_db.read<u32>();
                    if(timestamp_check == 0) {
                        // neosu 100%!
                        return MCNEOSU_SCORES;
                    } else {
                        // peppy 100%!
                        return STABLE_SCORES;
                    }

                    // unreachable
                }
            }

            // 0 maps or 0 scores
            return INVALID_DB;
        }
    }

    return INVALID_DB;
}

// run after at least one engine frame (due to resourceManager->update() in Engine::onUpdate())
void Database::AsyncDBLoader::init() {
    if(!db) return;  // don't crash when exiting while loading db

    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) start");

    if(db->needs_raw_load) {
        db->scheduleLoadRaw();
    } else {
        MapCalcThread::start_calc(db->maps_to_recalc);
        VolNormalization::start_calc(db->loudness_to_calc);
        sct_calc(db->scores);

        // signal that we are done
        db->loading_progress = 1.0f;
        this->setReady(true);
    }

    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) done");
}

// run immediately on a separate thread when resourceManager->loadResource() is called
void Database::AsyncDBLoader::initAsync() {
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) start");
    assert(db != nullptr);

    db->findDatabases();

    if(db->load_interrupted.load(std::memory_order_acquire)) goto done;

    using enum Database::DatabaseType;
    db->loadScores(db->database_files[NEOSU_SCORES]);
    if(db->load_interrupted.load(std::memory_order_acquire)) goto done;
    db->loadOldMcNeosuScores(db->database_files[MCNEOSU_SCORES]);
    if(db->load_interrupted.load(std::memory_order_acquire)) goto done;
    db->loadPeppyScores(db->database_files[STABLE_SCORES]);
    db->scores_loaded = true;
    if(db->load_interrupted.load(std::memory_order_acquire)) goto done;

    db->loadMaps();
    if(db->load_interrupted.load(std::memory_order_acquire)) goto done;

    if(!db->needs_raw_load) {
        Collections::load_all();
        if(db->load_interrupted.load(std::memory_order_acquire)) goto done;
    }

    // .db files that were dropped on the main window
    for(const auto &db_pair : db->external_databases) {
        db->importDatabase(db_pair);
        if(db->load_interrupted.load(std::memory_order_acquire)) goto done;
    }
    // only clear this after we have actually loaded them
    db->extern_db_paths_to_import_async_copy.clear();
    db->external_databases.clear();

done:

    this->setAsyncReady(true);
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) done");
}

void Database::startLoader() {
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("start");
    this->destroyLoader();

    // stop threads that rely on database content
    sct_abort();
    AsyncPPC::set_map(nullptr);
    MapCalcThread::abort();
    VolNormalization::abort();

    // only clear diffs/sets for full reloads (only handled for raw re-loading atm)
    const bool lastLoadWasRaw{this->needs_raw_load};

    this->needs_raw_load =
        (!Environment::fileExists(getDBPath(DatabaseType::STABLE_MAPS)) || !cv::database_enabled.getBool());

    const bool nextLoadIsRaw{this->needs_raw_load};

    if(!lastLoadWasRaw || !nextLoadIsRaw) {
        db->loudness_to_calc.clear();
        db->maps_to_recalc.clear();
        {
            Sync::unique_lock lock(this->beatmap_difficulties_mtx);
            this->beatmap_difficulties.clear();
        }
        for(auto &beatmapset : this->temp_loading_beatmapsets) {
            SAFE_DELETE(beatmapset);
        }
        this->temp_loading_beatmapsets.clear();
        for(auto &beatmapset : this->beatmapsets) {
            SAFE_DELETE(beatmapset);
        }
        this->beatmapsets.clear();
    }

    // append, the copy will only be cleared if loading them succeeded
    this->extern_db_paths_to_import_async_copy.append_range(this->extern_db_paths_to_import);
    this->extern_db_paths_to_import.clear();

    this->loader = new AsyncDBLoader();
    resourceManager->requestNextLoadAsync();
    resourceManager->loadResource(this->loader);

    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("done");
}

void Database::destroyLoader() {
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("start");
    directoryWatcher->stop_watching(NEOSU_MAPS_PATH "/");
    if(this->loader) {
        resourceManager->destroyResource(this->loader, ResourceManager::DestroyMode::FORCE_BLOCKING);  // force blocking
        this->loader = nullptr;
    }
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("done");
}

Database::Database() : importTimer(std::make_unique<Timer>()) {
    // convar callback
    cv::cmd::save.setCallback(SA::MakeDelegate<&Database::save>(this));
}

Database::~Database() {
    cv::cmd::save.removeCallback();
    this->destroyLoader();

    sct_abort();
    AsyncPPC::set_map(nullptr);
    VolNormalization::abort();
    this->loudness_to_calc.clear();

    MapCalcThread::abort();
    this->maps_to_recalc.clear();

    {
        Sync::unique_lock lock(this->beatmap_difficulties_mtx);
        this->beatmap_difficulties.clear();
    }
    for(auto &beatmapset : this->temp_loading_beatmapsets) {
        SAFE_DELETE(beatmapset);
    }
    this->temp_loading_beatmapsets.clear();
    for(auto &beatmapset : this->beatmapsets) {
        SAFE_DELETE(beatmapset);
    }
    this->beatmapsets.clear();

    Collections::unload_all();
}

void Database::update() {
    // loadRaw() logic
    if(this->raw_load_scheduled) {
        Timer t;

        while(t.getElapsedTime() < 0.033f) {
            if(this->load_interrupted.load(std::memory_order_acquire)) break;  // cancellation point

            if(this->raw_load_beatmap_folders.size() > 0 &&
               this->cur_raw_load_idx < this->raw_load_beatmap_folders.size()) {
                std::string curBeatmap = this->raw_load_beatmap_folders[this->cur_raw_load_idx++];
                this->raw_loaded_beatmap_folders.push_back(
                    curBeatmap);  // for future incremental loads, so that we know what's been loaded already

                std::string fullBeatmapPath = this->raw_load_osu_song_folder;
                fullBeatmapPath.append(curBeatmap);
                fullBeatmapPath.append("/");

                this->addBeatmapSet(fullBeatmapPath);
            }

            // update progress
            this->loading_progress = (float)this->cur_raw_load_idx / (float)this->num_beatmaps_to_load;

            // check if we are finished
            if(this->cur_raw_load_idx >= this->num_beatmaps_to_load ||
               std::cmp_greater(this->cur_raw_load_idx, (this->raw_load_beatmap_folders.size() - 1))) {
                this->raw_load_beatmap_folders.clear();
                this->raw_load_scheduled = false;
                this->importTimer->update();

                this->beatmapsets = std::move(this->temp_loading_beatmapsets);
                this->temp_loading_beatmapsets.clear();

                debugLog("Refresh finished, added {} beatmaps in {:f} seconds.", this->beatmapsets.size(),
                         this->importTimer->getElapsedTime());

                Collections::load_all();

                // clang-format off
                for(auto &diff : this->beatmapsets
                                // for all diffs within the set with fStarsNomod <= 0.f
                                | std::views::transform([](const auto &set) -> auto & { return *set->difficulties; })
                                | std::views::join
                                | std::views::filter([](const auto &diff) { return diff->fStarsNomod <= 0.f; })) {
                    diff->fStarsNomod *= -1.f;
                    this->maps_to_recalc.push_back(diff);
                }
                // clang-format on

                this->loading_progress = 1.0f;

                MapCalcThread::start_calc(this->maps_to_recalc);
                VolNormalization::start_calc(this->loudness_to_calc);
                sct_calc(this->scores);

                break;
            }

            t.update();
        }
    }
}

void Database::load() {
    this->load_interrupted = false;
    this->loading_progress = 0.0f;

    // reset scheduled logic
    this->raw_load_scheduled = false;

    this->startLoader();
}

void Database::cancel() {
    this->load_interrupted = true;
    this->loading_progress = 1.0f;  // force finished
    this->raw_found_changes = true;
}

void Database::save() {
    Collections::save_collections();
    this->saveMaps();
    this->saveScores();
}

BeatmapSet *Database::addBeatmapSet(const std::string &beatmapFolderPath, i32 set_id_override) {
    BeatmapSet *beatmap = this->loadRawBeatmap(beatmapFolderPath);
    if(beatmap == nullptr) return nullptr;

    // Some beatmaps don't provide beatmap/beatmapset IDs in the .osu files
    // But we know the beatmapset ID because we just downloaded it!
    if(set_id_override != -1) {
        beatmap->iSetID = set_id_override;
        for(auto &diff : beatmap->getDifficulties()) {
            diff->iSetID = set_id_override;
        }
    }

    {
        Sync::unique_lock lock(this->beatmap_difficulties_mtx);
        for(const auto &diff : beatmap->getDifficulties()) {
            this->beatmap_difficulties[diff->getMD5()] = diff;
        }
    }

    // do not add to songbrowser yet unless we are finished loading
    if(this->isFinished()) {
        this->beatmapsets.push_back(beatmap);

        osu->getSongBrowser()->addBeatmapSet(beatmap);

        // XXX: Very slow
        osu->getSongBrowser()->onSortChange(cv::songbrowser_sortingtype.getString().c_str());
    } else {
        this->temp_loading_beatmapsets.push_back(beatmap);
    }

    return beatmap;
}

int Database::addScore(const FinishedScore &score) {
    this->addScoreRaw(score);
    this->sortScores(score.beatmap_hash);

    this->scores_changed = true;

    if(cv::scores_save_immediately.getBool()) this->saveScores();

    // @PPV3: use new replay format

    // XXX: this is blocking main thread
    auto compressed_replay = LegacyReplay::compress_frames(score.replay);
    if(!compressed_replay.empty()) {
        auto replay_path = fmt::format(NEOSU_REPLAYS_PATH "/{:d}.replay.lzma", score.unixTimestamp);

        debugLog("Saving replay to {}...", replay_path);
        io->write(replay_path, compressed_replay, [replay_path, func = __FUNCTION__](bool success) {
            if(success) {
                debugLogLambda("Replay saved to {}.", replay_path);
            } else {
                debugLogLambda("Failed to save replay to {}", replay_path);
            }
        });
    }

    // return sorted index
    Sync::shared_lock lock(this->scores_mtx);
    for(int i = 0; i < this->scores[score.beatmap_hash].size(); i++) {
        if(this->scores[score.beatmap_hash][i].unixTimestamp == score.unixTimestamp) return i;
    }

    return -1;
}

int Database::isScoreAlreadyInDB(u64 unix_timestamp, const MD5Hash &map_hash) {
    Sync::shared_lock lock(this->scores_mtx);

    // operator[] might add a new entry
    const auto &scoreit = this->scores.find(map_hash);
    if(scoreit == this->scores.end()) return -1;

    for(int existing_pos = -1; const auto &existing : scoreit->second) {
        existing_pos++;
        if(existing.unixTimestamp == unix_timestamp) {
            // Score has already been added
            return existing_pos;
        }
    }

    return -1;
}

bool Database::addScoreRaw(const FinishedScore &score) {
    const bool new_might_have_replay{score.has_possible_replay()};

    int existing_pos{-1};
    bool overwrite{false};

    if((existing_pos = this->isScoreAlreadyInDB(score.unixTimestamp, score.beatmap_hash)) >= 0) {
        // a bit hacky, but allow overwriting mcosu scores with peppy/neosu scores
        // otherwise scores imported to mcosu from stable will be marked as "from mcosu"
        // which we consider to never have a replay available

        // we don't want to overwrite in any case if the new score has no possible replay
        if(!new_might_have_replay) {
            return false;
        }

        {
            Sync::shared_lock lock(this->scores_mtx);
            // otherwise check if the old one doesn't have a replay
            // if it has one, don't overwrite it
            overwrite = !this->scores[score.beatmap_hash][existing_pos].has_possible_replay();
        }

        if(!overwrite) {
            return false;
        }
        // otherwise overwrite it
    }

    Sync::unique_lock lock(this->scores_mtx);

    if(overwrite) {
        this->scores[score.beatmap_hash][existing_pos] = score;
    } else {
        // new score
        this->scores[score.beatmap_hash].push_back(score);
    }

    return true;
}

void Database::deleteScore(const MD5Hash &beatmapMD5Hash, u64 scoreUnixTimestamp) {
    Sync::unique_lock lock(this->scores_mtx);
    for(int i = 0; i < this->scores[beatmapMD5Hash].size(); i++) {
        if(this->scores[beatmapMD5Hash][i].unixTimestamp == scoreUnixTimestamp) {
            this->scores[beatmapMD5Hash].erase(this->scores[beatmapMD5Hash].begin() + i);
            this->scores_changed = true;
            break;
        }
    }
}

void Database::sortScoresInPlace(std::vector<FinishedScore> &scores) {
    if(scores.size() < 2) return;

    const auto &sortTypeString{cv::songbrowser_scores_sortingtype.getString()};
    for(const auto &sortMethod : Database::SCORE_SORTING_METHODS) {
        if(sortTypeString == sortMethod.name) {
            std::ranges::sort(scores, sortMethod.comparator);
            return;
        }
    }

    // Fallback
    cv::songbrowser_scores_sortingtype.setValue("By pp");
    std::ranges::sort(scores, sortScoreByPP);
}

void Database::sortScores(const MD5Hash &beatmapMD5Hash) {
    Sync::unique_lock lock(this->scores_mtx);
    this->sortScoresInPlace(this->scores[beatmapMD5Hash]);
}

std::vector<UString> Database::getPlayerNamesWithPPScores() {
    std::vector<UString> names;
    {
        Sync::shared_lock lock(this->scores_mtx);

        for(const auto &[hash, scoreVec] : this->scores) {
            for(const auto &name : scoreVec | std::views::transform([](const auto &score) -> auto {
                                       return score.playerName;
                                   }) | std::views::filter([](const auto &name) -> bool { return !name.empty(); })) {
                names.emplace_back(name);
            }
        }
    }

    // always add local user, even if there were no scores
    names.emplace_back(BanchoState::get_username());

    return names;
}

std::vector<UString> Database::getPlayerNamesWithScoresForUserSwitcher() {
    std::unordered_set<std::string> tempNames;

    {
        Sync::shared_lock lock(this->scores_mtx);
        for(const auto &[hash, _] : this->scores) {
            for(const auto &name : this->scores[hash] | std::views::transform([](const auto &score) -> auto {
                                       return score.playerName;
                                   })) {
                tempNames.insert(name);
            }
        }
    }

    // always add local user, even if there were no scores
    tempNames.insert(BanchoState::get_username());

    std::vector<UString> names;
    names.reserve(tempNames.size());
    for(const auto &name : tempNames) {
        if(name.length() > 0) names.emplace_back(name);
    }

    return names;
}

Database::PlayerPPScores Database::getPlayerPPScores(const std::string &playerName) {
    PlayerPPScores ppScores;
    ppScores.totalScore = 0;
    if(this->getProgress() < 1.0f) return ppScores;

    // hoist out of the loop
    const bool include_autopilot_relax = cv::user_include_relax_and_autopilot_for_stats.getBool();

    u64 totalScore = 0;

    std::vector<FinishedScore *> scores;
    {
        Sync::shared_lock lock(this->scores_mtx);

        for(auto &[hash, scorevec] : this->scores | std::views::filter([](const auto &pair) -> auto {
                                         // filter out empty vectors
                                         return pair.second.size() > 0;
                                     })) {
            FinishedScore *tempScore = &scorevec[0];

            // only add highest pp score per diff
            bool foundValidScore = false;
            f64 prevPP = -1.0;
            for(auto &score :
                scorevec | std::views::filter([&playerName, include_autopilot_relax](const auto &sc) -> auto {
                    // filter out scores set with a different name or if we shouldn't allow relax/autopilot
                    return !(!include_autopilot_relax &&
                             (u64)sc.mods.flags & ((u64)ModFlags::Relax | (u64)ModFlags::Autopilot)) &&
                           (playerName == sc.playerName);
                })) {
                foundValidScore = true;
                totalScore += score.score;

                const auto scorePP = score.get_pp();
                if(scorePP > prevPP || prevPP < 0.0) {
                    prevPP = scorePP;
                    tempScore = &score;
                }
            }

            if(foundValidScore) scores.push_back(tempScore);
        }

        // sort by pp
        // for some reason this was originally backwards from sortScoreByPP, so negating it here
        std::ranges::sort(scores, [](FinishedScore *a, FinishedScore *b) -> bool {
            if(a == b) return false;
            return !sortScoreByPP(*a, *b);
        });
    }

    ppScores.ppScores = std::move(scores);
    ppScores.totalScore = totalScore;

    return ppScores;
}

Database::PlayerStats Database::calculatePlayerStats(const std::string &playerName) {
    // FIXME: returning cached statistics even if we got new scores
    // this is makes this function a "sneaky" API that might return stale stats
    // done for performance to not tank FPS during score recalc where this is done on the main thread
    // every frame (by UserCard::updateUserStats)

    // should be done by the caller but it's more complicated because the prevPlayerStats are
    // cached inside Database...
    const bool scoresChanged = this->scores_changed.load(std::memory_order_acquire);
    const bool returnCached = playerName == this->prevPlayerStats.name.utf8View() &&
                              (!scoresChanged || (sct_running() && !engine->throttledShouldRun(60)));
    if(returnCached) {
        return this->prevPlayerStats;
    }

    const PlayerPPScores ps = this->getPlayerPPScores(playerName);

    // delay caching until we actually have scores loaded
    if(ps.ppScores.size() > 0 || this->isFinished()) {
        this->scores_changed.store(false, std::memory_order_release);
    }

    // "If n is the amount of scores giving more pp than a given score, then the score's weight is 0.95^n"
    // "Total pp = PP[1] * 0.95^0 + PP[2] * 0.95^1 + PP[3] * 0.95^2 + ... + PP[n] * 0.95^(n-1)"
    // also, total accuracy is apparently weighted the same as pp

    float pp = 0.0f;
    float acc = 0.0f;
    for(uSz i = 0; i < ps.ppScores.size(); i++) {
        const float weight = getWeightForIndex(ps.ppScores.size() - 1 - i);

        pp += ps.ppScores[i]->get_pp() * weight;
        acc += LiveScore::calculateAccuracy(ps.ppScores[i]->num300s, ps.ppScores[i]->num100s, ps.ppScores[i]->num50s,
                                            ps.ppScores[i]->numMisses) *
               weight;
    }

    // bonus pp
    // https://osu.ppy.sh/wiki/en/Performance_points
    if(cv::scores_bonus_pp.getBool()) pp += getBonusPPForNumScores(ps.ppScores.size());

    // normalize accuracy
    if(ps.ppScores.size() > 0) acc /= (20.0f * (1.0f - getWeightForIndex(ps.ppScores.size())));

    // fill stats
    this->prevPlayerStats.name = playerName.c_str();
    this->prevPlayerStats.pp = pp;
    this->prevPlayerStats.accuracy = acc;
    this->prevPlayerStats.numScoresWithPP = ps.ppScores.size();

    if(ps.totalScore != this->prevPlayerStats.totalScore) {
        this->prevPlayerStats.level = getLevelForScore(ps.totalScore);

        const u64 requiredScoreForCurrentLevel = getRequiredScoreForLevel(this->prevPlayerStats.level);
        const u64 requiredScoreForNextLevel = getRequiredScoreForLevel(this->prevPlayerStats.level + 1);

        if(requiredScoreForNextLevel > requiredScoreForCurrentLevel)
            this->prevPlayerStats.percentToNextLevel =
                (double)(ps.totalScore - requiredScoreForCurrentLevel) /
                (double)(requiredScoreForNextLevel - requiredScoreForCurrentLevel);
    }

    this->prevPlayerStats.totalScore = ps.totalScore;

    return this->prevPlayerStats;
}

float Database::getWeightForIndex(int i) { return std::pow(0.95f, (f32)i); }

float Database::getBonusPPForNumScores(size_t numScores) {
    return (417.0 - 1.0 / 3.0) * (1.0 - pow(0.995, std::min(1000.0, (f64)numScores)));
}

u64 Database::getRequiredScoreForLevel(int level) {
    // https://zxq.co/ripple/ocl/src/branch/master/level.go
    if(level <= 100) {
        if(level > 1)
            return (u64)std::floor(5000 / 3 * (4 * pow(level, 3) - 3 * pow(level, 2) - level) +
                                   std::floor(1.25 * pow(1.8, (double)(level - 60))));

        return 1;
    }

    return (u64)26931190829 + (u64)100000000000 * (u64)(level - 100);
}

int Database::getLevelForScore(u64 score, int maxLevel) {
    // https://zxq.co/ripple/ocl/src/branch/master/level.go
    int i = 0;
    while(true) {
        if(maxLevel > 0 && i >= maxLevel) return i;

        const u64 lScore = getRequiredScoreForLevel(i);

        if(score < lScore) return (i - 1);

        i++;
    }
}

DatabaseBeatmap *Database::getBeatmapDifficulty(const MD5Hash &md5hash) {
    if(this->isLoading()) return nullptr;

    Sync::shared_lock lock(this->beatmap_difficulties_mtx);
    auto it = this->beatmap_difficulties.find(md5hash);
    if(it == this->beatmap_difficulties.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

DatabaseBeatmap *Database::getBeatmapDifficulty(i32 map_id) {
    if(this->isLoading()) return nullptr;

    Sync::shared_lock lock(this->beatmap_difficulties_mtx);
    for(const auto &[_, diff] : this->beatmap_difficulties) {
        if(diff->getID() == map_id) {
            return diff;
        }
    }

    return nullptr;
}

DatabaseBeatmap *Database::getBeatmapSet(i32 set_id) {
    if(this->isLoading()) {
        debugLog("we are loading, progress {}, not returning a DatabaseBeatmap*", this->getProgress());
        return nullptr;
    }

    for(const auto &beatmap : this->beatmapsets) {
        if(beatmap->getSetID() == set_id) {
            return beatmap;
        }
    }

    return nullptr;
}

std::string Database::getOsuSongsFolder() {
    std::string songs_dir = Environment::normalizeDirectory(cv::songs_folder.getString());
    if(!env->isAbsolutePath(songs_dir)) {
        // cv::osu_folder is already normalized, so just concatenating is fine
        songs_dir = cv::osu_folder.getString() + songs_dir;
    }

    return songs_dir;
}

void Database::scheduleLoadRaw() {
    this->raw_load_osu_song_folder = Database::getOsuSongsFolder();

    debugLog("Database: sRawBeatmapLoadOsuSongFolder = {:s}", this->raw_load_osu_song_folder);

    this->raw_load_beatmap_folders = env->getFoldersInFolder(this->raw_load_osu_song_folder);
    this->num_beatmaps_to_load = this->raw_load_beatmap_folders.size();

    // if this isn't the first load, only load the differences
    if(!this->is_first_load) {
        std::vector<std::string> toLoad;
        for(uSz i = 0; i < this->num_beatmaps_to_load; i++) {
            bool alreadyLoaded = false;
            for(const auto &rawBeatmapFolder : this->raw_loaded_beatmap_folders) {
                if(this->raw_load_beatmap_folders[i] == rawBeatmapFolder) {
                    alreadyLoaded = true;
                    break;
                }
            }

            if(!alreadyLoaded) toLoad.push_back(this->raw_load_beatmap_folders[i]);
        }

        // only load differences
        this->raw_load_beatmap_folders = toLoad;
        this->num_beatmaps_to_load = this->raw_load_beatmap_folders.size();

        debugLog("Database: Found {} new/changed beatmaps.", this->num_beatmaps_to_load);

        this->raw_found_changes = this->num_beatmaps_to_load > 0;
        if(this->raw_found_changes)
            osu->getNotificationOverlay()->addNotification(
                UString::format(this->num_beatmaps_to_load == 1 ? "Adding %i new beatmap." : "Adding %i new beatmaps.",
                                this->num_beatmaps_to_load),
                0xff00ff00);
        else
            osu->getNotificationOverlay()->addNotification(
                UString::format("No new beatmaps detected.", this->num_beatmaps_to_load), 0xff00ff00);
    }

    debugLog("Database: Building beatmap database ...");
    debugLog("Database: Found {} folders to load.", this->raw_load_beatmap_folders.size());

    // only start loading if we have something to load
    if(this->raw_load_beatmap_folders.size() > 0) {
        this->loading_progress = 0.0f;
        this->cur_raw_load_idx = 0;

        this->raw_load_scheduled = true;
        this->importTimer->start();
    } else
        this->loading_progress = 1.0f;

    this->is_first_load = false;
}

void Database::loadMaps() {
    const auto &peppy_db_path = this->database_files[DatabaseType::STABLE_MAPS];
    const auto &neosu_maps_path = this->database_files[DatabaseType::NEOSU_MAPS];

    const std::string &songFolder = Database::getOsuSongsFolder();
    debugLog("Database: songFolder = {:s}", songFolder.c_str());

    this->importTimer->start();

    u32 nb_neosu_maps = 0;
    u32 nb_peppy_maps = 0;
    u32 nb_overrides = 0;

    // read beatmapInfos, and also build two hashmaps (diff hash -> BeatmapDifficulty, diff hash -> Beatmap)
    struct Beatmap_Set {
        int setID{0};
        std::vector<DatabaseBeatmap *> *diffs2{nullptr};
    };
    std::vector<Beatmap_Set> beatmapSets;
    std::unordered_map<int, size_t> setIDToIndex;

    // Load neosu map database
    {
        ByteBufferedFile::Reader neosu_maps(neosu_maps_path);
        if(neosu_maps.total_size > 0) {
            u32 version = neosu_maps.read<u32>();
            if(version < NEOSU_MAPS_DB_VERSION) {
                // Reading from older database version: backup just in case
                auto backup_path = fmt::format("{}.{}", neosu_maps_path, version);
                ByteBufferedFile::copy(neosu_maps_path, backup_path);
            }

            u32 nb_sets = neosu_maps.read<u32>();
            for(uSz i = 0; i < nb_sets; i++) {
                if(this->load_interrupted.load(std::memory_order_acquire)) break;  // cancellation point

                u32 progress_bytes = this->bytes_processed + neosu_maps.total_pos;
                f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
                this->loading_progress = std::clamp(progress_float, 0.01, 0.99);

                i32 set_id = neosu_maps.read<i32>();
                u16 nb_diffs = neosu_maps.read<u16>();

                std::string mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);

                auto *diffs = new std::vector<DatabaseBeatmap *>();
                for(u16 j = 0; j < nb_diffs; j++) {
                    if(this->load_interrupted.load(std::memory_order_acquire)) {  // cancellation point
                        // clean up partially loaded diffs in current set
                        Sync::unique_lock lock(this->beatmap_difficulties_mtx);
                        for(DatabaseBeatmap *diff : *diffs) {
                            this->beatmap_difficulties.erase(diff->getMD5());
                            delete diff;
                        }
                        delete diffs;
                        diffs = nullptr;
                        break;
                    }

                    std::string osu_filename = neosu_maps.read_string();

                    std::string map_path = mapset_path;
                    map_path.append(osu_filename);

                    auto diff =
                        new BeatmapDifficulty(map_path, mapset_path, DatabaseBeatmap::BeatmapType::NEOSU_DIFFICULTY);
                    diff->iID = neosu_maps.read<i32>();
                    diff->iSetID = set_id;
                    diff->sTitle = neosu_maps.read_string();
                    diff->sAudioFileName = neosu_maps.read_string();
                    diff->sFullSoundFilePath = mapset_path;
                    diff->sFullSoundFilePath.append(diff->sAudioFileName);
                    diff->iLengthMS = neosu_maps.read<i32>();
                    diff->fStackLeniency = neosu_maps.read<f32>();
                    diff->sArtist = neosu_maps.read_string();
                    diff->sCreator = neosu_maps.read_string();
                    diff->sDifficultyName = neosu_maps.read_string();
                    diff->sSource = neosu_maps.read_string();
                    diff->sTags = neosu_maps.read_string();
                    diff->writeMD5(neosu_maps.read_hash());
                    diff->fAR = neosu_maps.read<f32>();
                    diff->fCS = neosu_maps.read<f32>();
                    diff->fHP = neosu_maps.read<f32>();
                    diff->fOD = neosu_maps.read<f32>();
                    diff->fSliderMultiplier = neosu_maps.read<f64>();
                    diff->iPreviewTime = neosu_maps.read<u32>();
                    diff->last_modification_time = neosu_maps.read<u64>();
                    diff->iLocalOffset = neosu_maps.read<i16>();
                    diff->iOnlineOffset = neosu_maps.read<i16>();
                    diff->iNumCircles = neosu_maps.read<u16>();
                    diff->iNumSliders = neosu_maps.read<u16>();
                    diff->iNumSpinners = neosu_maps.read<u16>();
                    diff->iNumObjects = diff->iNumCircles + diff->iNumSliders + diff->iNumSpinners;
                    diff->fStarsNomod = neosu_maps.read<f64>();
                    diff->iMinBPM = neosu_maps.read<i32>();
                    diff->iMaxBPM = neosu_maps.read<i32>();
                    diff->iMostCommonBPM = neosu_maps.read<i32>();

                    if(version < 20240812) {
                        u32 nb_timing_points = neosu_maps.read<u32>();
                        neosu_maps.skip_bytes(sizeof(DB_TIMINGPOINT) * nb_timing_points);
                    }

                    if(version >= 20240703) {
                        diff->draw_background = neosu_maps.read<u8>();
                    }

                    f32 loudness = 0.f;
                    if(version >= 20240812) {
                        loudness = neosu_maps.read<f32>();
                    }
                    if(loudness == 0.f) {
                        this->loudness_to_calc.push_back(diff);
                    } else {
                        diff->loudness = loudness;
                    }

                    if(version >= 20250801) {
                        diff->sTitleUnicode = neosu_maps.read_string();
                        diff->sArtistUnicode = neosu_maps.read_string();
                    } else {
                        diff->sTitleUnicode = diff->sTitle;
                        diff->sArtistUnicode = diff->sArtist;
                    }

                    if(SString::is_wspace_only(diff->sTitleUnicode)) {
                        diff->bEmptyTitleUnicode = true;
                    }
                    if(SString::is_wspace_only(diff->sArtistUnicode)) {
                        diff->bEmptyArtistUnicode = true;
                    }

                    // we cache the background image filename in the database past this version
                    if(version >= 20251009) {
                        diff->sBackgroundImageFileName = neosu_maps.read_string();
                    }

                    {
                        Sync::unique_lock lock(this->beatmap_difficulties_mtx);
                        this->beatmap_difficulties[diff->getMD5()] = diff;
                    }
                    diffs->push_back(diff);
                    nb_neosu_maps++;
                }

                // if we were canceled during inner loop, diffs will be nullptr
                if(diffs == nullptr) break;

                // NOTE: Ignoring mapsets with ID -1, since we most likely saved them in the correct folder,
                //       but mistakenly set their ID to -1 (because the ID was missing from the .osu file).
                if(diffs->empty() || set_id == -1) {
                    delete diffs;
                } else {
                    auto set = new BeatmapSet(diffs, DatabaseBeatmap::BeatmapType::NEOSU_BEATMAPSET);
                    this->temp_loading_beatmapsets.push_back(set);

                    // NOTE: Don't add neosu sets to beatmapSets since they're already processed
                    // Adding them would create duplicate ownership of the diffs vector
                }
            }

            if(version >= 20240812) {
                nb_overrides = neosu_maps.read<u32>();
                Sync::unique_lock lock(this->peppy_overrides_mtx);
                for(uSz i = 0; i < nb_overrides; i++) {
                    MapOverrides over;
                    auto map_md5 = neosu_maps.read_hash();
                    over.local_offset = neosu_maps.read<i16>();
                    over.online_offset = neosu_maps.read<i16>();
                    over.star_rating = neosu_maps.read<f32>();
                    over.loudness = neosu_maps.read<f32>();
                    over.min_bpm = neosu_maps.read<i32>();
                    over.max_bpm = neosu_maps.read<i32>();
                    over.avg_bpm = neosu_maps.read<i32>();
                    over.draw_background = neosu_maps.read<u8>();
                    if(version >= 20251009) {
                        over.background_image_filename = neosu_maps.read_string();
                    }
                    this->peppy_overrides[map_md5] = over;
                }
            }
        }
        this->bytes_processed += neosu_maps.total_size;
        this->neosu_maps_loaded = true;
    }

    if(!this->needs_raw_load) {
        ByteBufferedFile::Reader dbr(peppy_db_path);
        u32 osu_db_version = (dbr.good() && dbr.total_size > 0) ? dbr.read<u32>() : 0;
        bool should_read_peppy_database = osu_db_version > 0;
        if(should_read_peppy_database) {
            // read header
            u32 osu_db_folder_count = dbr.read<u32>();
            dbr.skip<u8>();
            dbr.skip<u64>() /* timestamp */;
            auto playerName = dbr.read_string();
            this->num_beatmaps_to_load = dbr.read<u32>();

            debugLog("Database: version = {:d}, folderCount = {:d}, playerName = {:s}, numDiffs = {:d}", osu_db_version,
                     osu_db_folder_count, playerName.c_str(), this->num_beatmaps_to_load);

            // hard cap upper db version
            if(osu_db_version > cv::database_version.getVal<u32>() && !cv::database_ignore_version.getBool()) {
                osu->getNotificationOverlay()->addToast(
                    UString::format("osu!.db version unknown (%i), osu!stable maps will not get loaded.",
                                    osu_db_version),
                    ERROR_TOAST);
                should_read_peppy_database = false;
            }
        }

        if(should_read_peppy_database) {
            zarray<BPMTuple> bpm_calculation_buffer;
            zarray<DB_TIMINGPOINT> timing_points_buffer;

            for(uSz i = 0; i < this->num_beatmaps_to_load; i++) {
                if(this->load_interrupted.load(std::memory_order_acquire)) break;  // cancellation point

                logIfCV(debug_db, "Database: Reading beatmap {:d}/{:d} ...", (i + 1), this->num_beatmaps_to_load);
                // update progress (another thread checks if progress >= 1.f to know when we're done)
                u32 progress_bytes = this->bytes_processed + dbr.total_pos;
                f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
                this->loading_progress = std::clamp(progress_float, 0.01, 0.99);

                // NOTE: This is documented wrongly in many places.
                //       This int was added in 20160408 and removed in 20191106
                //       https://osu.ppy.sh/home/changelog/stable40/20160408.3
                //       https://osu.ppy.sh/home/changelog/cuttingedge/20191106
                if(osu_db_version >= 20160408 && osu_db_version < 20191106) {
                    // size in bytes of the beatmap entry
                    dbr.skip<u32>();
                }

                std::string artistName = dbr.read_string();
                SString::trim_inplace(artistName);
                std::string artistNameUnicode = dbr.read_string();
                std::string songTitle = dbr.read_string();
                SString::trim_inplace(songTitle);
                std::string songTitleUnicode = dbr.read_string();
                std::string creatorName = dbr.read_string();
                SString::trim_inplace(creatorName);
                std::string difficultyName = dbr.read_string();
                SString::trim_inplace(difficultyName);
                std::string audioFileName = dbr.read_string();

                auto md5hash = dbr.read_hash();
                bool overrides_found = false;
                MapOverrides override;
                {
                    Sync::shared_lock lock(this->peppy_overrides_mtx);
                    auto overrides = this->peppy_overrides.find(md5hash);
                    overrides_found = overrides != this->peppy_overrides.end();
                    if(overrides_found) {
                        override = overrides->second;
                    }
                }
                std::string osuFileName = dbr.read_string();
                /*unsigned char rankedStatus = */ dbr.skip<u8>();
                auto numCircles = dbr.read<u16>();
                auto numSliders = dbr.read<u16>();
                auto numSpinners = dbr.read<u16>();
                i64 lastModificationTime = dbr.read<u64>();

                f32 AR, CS, HP, OD;
                if(osu_db_version < 20140609) {
                    AR = dbr.read<u8>();
                    CS = dbr.read<u8>();
                    HP = dbr.read<u8>();
                    OD = dbr.read<u8>();
                } else {
                    AR = dbr.read<f32>();
                    CS = dbr.read<f32>();
                    HP = dbr.read<f32>();
                    OD = dbr.read<f32>();
                }

                auto sliderMultiplier = dbr.read<f64>();

                f32 nomod_star_rating = 0.0f;
                if(osu_db_version >= 20140609) {
                    // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                    const u32 sr_field_size = osu_db_version < 20250108 ? sizeof(f64) : sizeof(f32);

                    const auto num_std_star_ratings = dbr.read<u32>();
                    for(u64 s = 0; s < num_std_star_ratings; s++) {
                        dbr.skip<u8>();  // 0x08 ObjType
                        auto mods = dbr.read<u32>();
                        dbr.skip<u8>();  // 0x0c ObjType

                        f32 sr;
                        if(!dbr.read_bytes(reinterpret_cast<u8 *>(&sr), sr_field_size)) {
                            debugLog("WARNING: failed to read SR for {}", md5hash.string());
                        } else {
                            if(mods == 0) nomod_star_rating = sr;
                        }
                    }

                    // taiko/ctb/mania are here only to skip the correct amount of bytes
                    const u32 minigame_skip_bytes =
                        sizeof(u8) /*ObjType*/ + sizeof(u32) /*mods*/ + sizeof(u8) /*ObjType*/ + sr_field_size;
                    for(auto _ : {1 /*taiko*/, 2 /*ctb*/, 3 /*mania*/}) {
                        const auto num_minigame_star_ratings = dbr.read<u32>();
                        for(u32 s = 0; s < num_minigame_star_ratings; s++) {
                            dbr.skip_bytes(minigame_skip_bytes);
                        }
                    }
                }

                /*unsigned int drainTime = */ dbr.skip<u32>();  // seconds
                int duration = dbr.read<u32>();                 // milliseconds
                duration = duration >= 0 ? duration : 0;        // sanity clamp
                int previewTime = dbr.read<u32>();

                BPMInfo bpm;
                auto nb_timing_points = dbr.read<u32>();
                if(overrides_found) {
                    dbr.skip_bytes(sizeof(DB_TIMINGPOINT) * nb_timing_points);
                    bpm.min = override.min_bpm;
                    bpm.max = override.max_bpm;
                    bpm.most_common = override.avg_bpm;
                } else if(nb_timing_points > 0) {
                    timing_points_buffer.resize(nb_timing_points);
                    if(dbr.read_bytes((u8 *)timing_points_buffer.data(), sizeof(DB_TIMINGPOINT) * nb_timing_points) !=
                       sizeof(DB_TIMINGPOINT) * nb_timing_points) {
                        debugLog("WARNING: failed to read timing points from beatmap {:d} !", (i + 1));
                    }
                    bpm = getBPM(timing_points_buffer, bpm_calculation_buffer);
                }

                int beatmapID =
                    dbr.read<i32>();  // fucking bullshit, this is NOT an unsigned integer as is described on
                                      // the wiki, it can and is -1 sometimes
                int beatmapSetID = dbr.read<i32>();  // same here
                /*unsigned int threadID = */ dbr.skip<u32>();

                /*unsigned char osuStandardGrade = */ dbr.skip<u8>();
                /*unsigned char taikoGrade = */ dbr.skip<u8>();
                /*unsigned char ctbGrade = */ dbr.skip<u8>();
                /*unsigned char maniaGrade = */ dbr.skip<u8>();

                auto localOffset = dbr.read<u16>();
                auto stackLeniency = dbr.read<f32>();
                auto mode = dbr.read<u8>();

                auto songSource = dbr.read_string();
                auto songTags = dbr.read_string();
                SString::trim_inplace(songSource);
                SString::trim_inplace(songTags);

                auto onlineOffset = dbr.read<u16>();
                dbr.skip_string();  // song title font
                /*bool unplayed = */ dbr.skip<u8>();
                /*i64 lastTimePlayed = */ dbr.skip<u64>();
                /*bool isOsz2 = */ dbr.skip<u8>();

                // somehow, some beatmaps may have spaces at the start/end of their
                // path, breaking the Windows API (e.g. https://osu.ppy.sh/s/215347)
                auto path = dbr.read_string();
                SString::trim_inplace(path);

                /*i64 lastOnlineCheck = */ dbr.skip<u64>();

                /*bool ignoreBeatmapSounds = */ dbr.skip<u8>();
                /*bool ignoreBeatmapSkin = */ dbr.skip<u8>();
                /*bool disableStoryboard = */ dbr.skip<u8>();
                /*bool disableVideo = */ dbr.skip<u8>();
                /*bool visualOverride = */ dbr.skip<u8>();

                if(osu_db_version < 20140609) {
                    // https://github.com/ppy/osu/wiki/Legacy-database-file-structure defines it as "Unknown"
                    dbr.skip<u16>();
                }

                /*int lastEditTime = */ dbr.skip<u32>();
                /*unsigned char maniaScrollSpeed = */ dbr.skip<u8>();

                // skip invalid/corrupt entries
                // the good way would be to check if the .osu file actually exists on disk, but that is slow af, ain't
                // nobody got time for that so, since I've seen some concrete examples of what happens in such cases, we
                // just exclude those
                if(artistName.length() < 1 && songTitle.length() < 1 && creatorName.length() < 1 &&
                   difficultyName.length() < 1 && md5hash == MD5Hash{})
                    continue;

                if(mode != 0) continue;

                // it can happen that nested beatmaps are stored in the
                // database, and that osu! stores that filepath with a backslash (because windows)
                // so replace them with / to normalize
                std::ranges::replace(path, '\\', '/');

                // build beatmap & diffs from all the data
                std::string beatmapPath = songFolder;
                beatmapPath.append(path.c_str());
                beatmapPath.push_back('/');
                std::string fullFilePath = beatmapPath;
                fullFilePath.append(osuFileName);

                // special case: legacy fallback behavior for invalid beatmapSetID, try to parse the ID from the path
                if(beatmapSetID < 1 && path.length() > 0) {
                    size_t slash = path.find('/');
                    std::string candidate = (slash != std::string::npos) ? path.substr(0, slash) : path;

                    if(!candidate.empty() && std::isdigit(static_cast<unsigned char>(candidate[0]))) {
                        if(!Parsing::parse(candidate.c_str(), &beatmapSetID)) {
                            beatmapSetID = -1;
                        }
                    }
                }

                // fill diff with data
                auto *map =
                    new DatabaseBeatmap(fullFilePath, beatmapPath, DatabaseBeatmap::BeatmapType::PEPPY_DIFFICULTY);
                {
                    map->sTitle = songTitle;
                    map->sTitleUnicode = songTitleUnicode;
                    if(SString::is_wspace_only(map->sTitleUnicode)) {
                        map->bEmptyTitleUnicode = true;
                    }
                    map->sAudioFileName = audioFileName;
                    map->iLengthMS = duration;

                    map->fStackLeniency = stackLeniency;

                    map->sArtist = artistName;
                    map->sArtistUnicode = artistNameUnicode;
                    if(SString::is_wspace_only(map->sArtistUnicode)) {
                        map->bEmptyArtistUnicode = true;
                    }
                    map->sCreator = creatorName;
                    map->sDifficultyName = difficultyName;
                    map->sSource = songSource;
                    map->sTags = songTags;
                    map->writeMD5(md5hash);
                    map->iID = beatmapID;
                    map->iSetID = beatmapSetID;

                    map->fAR = AR;
                    map->fCS = CS;
                    map->fHP = HP;
                    map->fOD = OD;
                    map->fSliderMultiplier = sliderMultiplier;

                    // map->sBackgroundImageFileName = "";

                    map->iPreviewTime = previewTime;
                    map->last_modification_time = lastModificationTime;

                    map->sFullSoundFilePath = beatmapPath;
                    map->sFullSoundFilePath.append(map->sAudioFileName);
                    map->iNumObjects = numCircles + numSliders + numSpinners;
                    map->iNumCircles = numCircles;
                    map->iNumSliders = numSliders;
                    map->iNumSpinners = numSpinners;
                    map->iMinBPM = bpm.min;
                    map->iMaxBPM = bpm.max;
                    map->iMostCommonBPM = bpm.most_common;
                }

                // (the diff is now fully built)
                Sync::unique_lock lock(this->beatmap_difficulties_mtx);
                this->beatmap_difficulties[md5hash] = map;

                // now, search if the current set (to which this diff would belong) already exists and add it there, or
                // if it doesn't exist then create the set
                const auto result = setIDToIndex.find(beatmapSetID);
                const bool beatmapSetExists = (result != setIDToIndex.end());
                bool diff_already_added = false;
                if(beatmapSetExists) {
                    for(const auto &existing_diff : *beatmapSets[result->second].diffs2) {
                        if(existing_diff->getMD5() == map->getMD5()) {
                            diff_already_added = true;
                            break;
                        }
                    }
                    if(!diff_already_added) {
                        beatmapSets[result->second].diffs2->push_back(map);
                    }
                } else {
                    setIDToIndex[beatmapSetID] = beatmapSets.size();

                    Beatmap_Set s;
                    s.setID = beatmapSetID;
                    s.diffs2 = new std::vector<DatabaseBeatmap *>();
                    s.diffs2->push_back(map);
                    beatmapSets.push_back(s);
                }

                if(!diff_already_added) {
                    bool loudness_found = false;
                    if(overrides_found) {
                        map->iLocalOffset = override.local_offset;
                        map->iOnlineOffset = override.online_offset;
                        map->fStarsNomod = override.star_rating;
                        map->loudness = override.loudness;
                        map->draw_background = override.draw_background;
                        map->sBackgroundImageFileName = override.background_image_filename;
                        if(override.loudness != 0.f) {
                            loudness_found = true;
                        }
                    } else {
                        if(nomod_star_rating <= 0.f) {
                            nomod_star_rating *= -1.f;
                            this->maps_to_recalc.push_back(map);
                        }

                        map->iLocalOffset = localOffset;
                        map->iOnlineOffset = onlineOffset;
                        map->fStarsNomod = nomod_star_rating;
                        map->draw_background = true;
                    }

                    if(!loudness_found) {
                        this->loudness_to_calc.push_back(map);
                    }
                } else {
                    SAFE_DELETE(map);  // we never added this diff to any container, so we have to free it here
                }

                nb_peppy_maps++;
            }

            Sync::unique_lock lock(this->beatmap_difficulties_mtx);

            // build beatmap sets
            for(const auto &beatmapSet : beatmapSets) {
                if(this->load_interrupted.load(std::memory_order_acquire)) {  // cancellation point
                    // clean up remaining unprocessed diffs2 vectors and their contents
                    for(size_t i = &beatmapSet - &beatmapSets[0]; i < beatmapSets.size(); i++) {
                        if(beatmapSets[i].diffs2) {
                            for(DatabaseBeatmap *diff : *beatmapSets[i].diffs2) {
                                this->beatmap_difficulties.erase(diff->getMD5());

                                // remove from loudness_to_calc
                                std::erase_if(this->loudness_to_calc,
                                              [diff](const auto &loudness_diff) { return loudness_diff == diff; });
                                // remove from maps_to_recalc
                                std::erase_if(this->maps_to_recalc,
                                              [diff](const auto &recalc_diff) { return recalc_diff == diff; });

                                delete diff;
                            }
                            delete beatmapSets[i].diffs2;
                        }
                    }
                    break;
                }

                if(beatmapSet.diffs2->empty()) {  // sanity check
                    // clean up empty diffs2 vector
                    delete beatmapSet.diffs2;
                    continue;
                }

                if(beatmapSet.setID > 0) {
                    auto *set = new BeatmapSet(beatmapSet.diffs2, DatabaseBeatmap::BeatmapType::PEPPY_BEATMAPSET);
                    this->temp_loading_beatmapsets.push_back(set);
                    // beatmapSet.diffs2 ownership transferred to BeatmapSet
                } else {
                    // set with invalid ID: treat all its diffs separately. we'll group the diffs by title+artist.
                    std::unordered_map<std::string, std::vector<DatabaseBeatmap *> *> titleArtistToBeatmap;
                    for(const auto &diff : (*beatmapSet.diffs2)) {
                        std::string titleArtist = diff->getTitleLatin();
                        titleArtist.append("|");
                        titleArtist.append(diff->getArtistLatin());

                        auto it = titleArtistToBeatmap.find(titleArtist);
                        if(it == titleArtistToBeatmap.end()) {
                            titleArtistToBeatmap[titleArtist] = new std::vector<DatabaseBeatmap *>();
                        }

                        titleArtistToBeatmap[titleArtist]->push_back(diff);
                    }

                    for(const auto &scuffed_set : titleArtistToBeatmap) {
                        auto *set = new BeatmapSet(scuffed_set.second, DatabaseBeatmap::BeatmapType::PEPPY_BEATMAPSET);
                        this->temp_loading_beatmapsets.push_back(set);
                    }

                    // clean up the original diffs2 vector (ownership of diffs transferred to new vectors)
                    delete beatmapSet.diffs2;
                }
            }
        }
        this->bytes_processed += dbr.total_size;
    }
    this->beatmapsets = std::move(this->temp_loading_beatmapsets);
    this->temp_loading_beatmapsets.clear();

    this->importTimer->update();
    debugLog("peppy+neosu maps: loading took {:f} seconds ({:d} peppy, {:d} neosu, {:d} maps total)",
             this->importTimer->getElapsedTime(), nb_peppy_maps, nb_neosu_maps, nb_peppy_maps + nb_neosu_maps);
    debugLog("Found {:d} overrides; {:d} maps need star recalc, {:d} maps need loudness recalc", nb_overrides,
             this->maps_to_recalc.size(), this->loudness_to_calc.size());
}

void Database::saveMaps() {
    if(this->beatmapsets.empty() || this->isLoading()) {
        return;
    }

    debugLog("Osu: Saving maps ...");
    if(!this->neosu_maps_loaded) {
        debugLog("Cannot save maps since they weren't loaded properly first!");
        return;
    }

    Timer t;
    t.start();

    const auto neosu_maps_db = getDBPath(DatabaseType::NEOSU_MAPS);

    ByteBufferedFile::Writer maps(neosu_maps_db);
    if(!maps.good()) {
        debugLog("Cannot save maps to {}: {}", neosu_maps_db, maps.error());
        return;
    }

    // collect neosu-only sets here
    std::vector<BeatmapSet *> temp_neosu_sets;
    for(const auto &beatmap : this->beatmapsets) {
        if(beatmap->type == DatabaseBeatmap::BeatmapType::NEOSU_BEATMAPSET) {
            temp_neosu_sets.push_back(beatmap);
        }
    }

    maps.write<u32>(NEOSU_MAPS_DB_VERSION);

    // Save neosu-downloaded maps
    u32 nb_diffs_saved = 0;
    maps.write<u32>(temp_neosu_sets.size());
    for(BeatmapSet *beatmap : temp_neosu_sets) {
        maps.write<i32>(beatmap->getSetID());
        maps.write<u16>(beatmap->getDifficulties().size());

        for(BeatmapDifficulty *diff : beatmap->getDifficulties()) {
            maps.write_string(env->getFileNameFromFilePath(diff->sFilePath).c_str());
            maps.write<i32>(diff->iID);
            maps.write_string(diff->sTitle.c_str());
            maps.write_string(diff->sAudioFileName.c_str());
            maps.write<i32>(diff->iLengthMS);
            maps.write<f32>(diff->fStackLeniency);
            maps.write_string(diff->sArtist.c_str());
            maps.write_string(diff->sCreator.c_str());
            maps.write_string(diff->sDifficultyName.c_str());
            maps.write_string(diff->sSource.c_str());
            maps.write_string(diff->sTags.c_str());
            maps.write_hash(diff->getMD5());
            maps.write<f32>(diff->fAR);
            maps.write<f32>(diff->fCS);
            maps.write<f32>(diff->fHP);
            maps.write<f32>(diff->fOD);
            maps.write<f64>(diff->fSliderMultiplier);
            maps.write<u32>(diff->iPreviewTime);
            maps.write<u64>(diff->last_modification_time);
            maps.write<i16>(diff->iLocalOffset);
            maps.write<i16>(diff->iOnlineOffset);
            maps.write<u16>(diff->iNumCircles);
            maps.write<u16>(diff->iNumSliders);
            maps.write<u16>(diff->iNumSpinners);
            maps.write<f64>(diff->fStarsNomod);
            maps.write<i32>(diff->iMinBPM);
            maps.write<i32>(diff->iMaxBPM);
            maps.write<i32>(diff->iMostCommonBPM);
            maps.write<u8>(diff->draw_background);
            maps.write<f32>(diff->loudness.load(std::memory_order_acquire));
            maps.write_string(diff->sTitleUnicode.c_str());
            maps.write_string(diff->sArtistUnicode.c_str());
            maps.write_string(diff->sBackgroundImageFileName.c_str());

            nb_diffs_saved++;
        }
    }

    // We want to save settings we applied on peppy-imported maps

    // When calculating loudness we don't call update_overrides() for performance reasons
    {
        Sync::unique_lock lock(this->peppy_overrides_mtx);
        for(const auto &map : this->loudness_to_calc) {
            if(map->type != DatabaseBeatmap::BeatmapType::PEPPY_DIFFICULTY) continue;
            if(map->loudness.load(std::memory_order_acquire) == 0.f) continue;
            this->peppy_overrides[map->getMD5()] = map->get_overrides();
        }
    }

    u32 nb_overrides = 0;
    {
        // only need read lock here
        Sync::shared_lock lock(this->peppy_overrides_mtx);
        maps.write<u32>(this->peppy_overrides.size());
        for(const auto &[hash, override] : this->peppy_overrides) {
            maps.write_hash(hash);
            maps.write<i16>(override.local_offset);
            maps.write<i16>(override.online_offset);
            maps.write<f32>(override.star_rating);
            maps.write<f32>(override.loudness);
            maps.write<i32>(override.min_bpm);
            maps.write<i32>(override.max_bpm);
            maps.write<i32>(override.avg_bpm);
            maps.write<u8>(override.draw_background);
            maps.write_string(override.background_image_filename.c_str());

            nb_overrides++;
        }
    }

    t.update();
    debugLog("Saved {:d} maps (+ {:d} overrides) in {:f} seconds.", nb_diffs_saved, nb_overrides, t.getElapsedTime());
}

void Database::findDatabases() {
    this->bytes_processed = 0;
    this->total_bytes = 0;
    this->database_files.clear();
    this->external_databases.clear();

    using enum DatabaseType;
    this->database_files.emplace(STABLE_SCORES, getDBPath(STABLE_SCORES));
    this->database_files.emplace(NEOSU_SCORES, getDBPath(NEOSU_SCORES));
    this->database_files.emplace(MCNEOSU_SCORES, getDBPath(MCNEOSU_SCORES));  // mcneosu database

    // ignore if explicitly disabled
    if(cv::database_enabled.getBool()) {
        this->database_files.emplace(STABLE_MAPS, getDBPath(STABLE_MAPS));
    }

    this->database_files.emplace(NEOSU_MAPS, getDBPath(NEOSU_MAPS));

    this->database_files.emplace(STABLE_COLLECTIONS, getDBPath(STABLE_COLLECTIONS));
    this->database_files.emplace(MCNEOSU_COLLECTIONS, getDBPath(MCNEOSU_COLLECTIONS));

    for(const auto &db_path : this->extern_db_paths_to_import_async_copy) {
        auto db_type = getDBType(db_path);
        if(db_type != INVALID_DB) {
            debugLog("adding external DB {} (type {}) for import", db_path, static_cast<u8>(db_type));
            const auto &[_, added] = this->external_databases.emplace(db_type, db_path);
            if(!added) {
                debugLog("NOTE: ignored duplicate database {}", db_path);
            }
        } else {
            debugLog("invalid external database: {}", db_path);
        }
    }

    for(const auto &[type, pathstr] : this->database_files) {
        std::error_code ec;
        auto db_filesize = std::filesystem::file_size(File::getFsPath(pathstr), ec);
        if(!ec && db_filesize > 0) {
            this->total_bytes += db_filesize;
        }
    }

    for(const auto &[type, pathstr] : this->external_databases) {
        std::error_code ec;
        auto db_filesize = std::filesystem::file_size(File::getFsPath(pathstr), ec);
        if(!ec && db_filesize > 0) {
            this->total_bytes += db_filesize;
        }
    }
}

// Detects what type of database it is, then imports it
bool Database::importDatabase(const std::pair<DatabaseType, std::string> &db_pair) {
    using enum DatabaseType;
    auto db_type = db_pair.first;
    auto db_path = db_pair.second;
    switch(db_type) {
        case INVALID_DB:
            return false;
        case NEOSU_SCORES: {
            this->loadScores(db_path);
            return true;
        }
        case MCNEOSU_SCORES: {
            this->loadOldMcNeosuScores(db_path);
            return true;
        }
        case MCNEOSU_COLLECTIONS:
            return Collections::load_mcneosu(db_path);
        case NEOSU_MAPS: {
            debugLog("tried to import external neosu_maps db {}, not supported", db_path);
            return false;
        }
        case STABLE_SCORES: {
            this->loadPeppyScores(db_path);
            return true;
        }
        case STABLE_COLLECTIONS:
            return Collections::load_peppy(db_path);
        case STABLE_MAPS: {
            debugLog("tried to import external stable maps db {}, not supported", db_path);
            return false;
        }
    }

    std::unreachable();
}

void Database::loadScores(std::string_view dbPath) {
    ByteBufferedFile::Reader dbr(dbPath);
    if(dbr.total_size == 0) {
        this->bytes_processed += dbr.total_size;
        return;
    }

    u32 nb_neosu_scores = 0;
    u8 magic_bytes[6] = {0};
    if(dbr.read_bytes(magic_bytes, 5) != 5 || memcmp(magic_bytes, "NEOSC", 5) != 0) {
        osu->getNotificationOverlay()->addToast(u"Failed to load neosu_scores.db!", ERROR_TOAST);
        this->bytes_processed += dbr.total_size;
        return;
    }

    u32 db_version = dbr.read<u32>();
    if(db_version > NEOSU_SCORE_DB_VERSION) {
        debugLog("neosu_scores.db version is newer than current neosu version!");
        this->bytes_processed += dbr.total_size;
        return;
    } else if(db_version < NEOSU_SCORE_DB_VERSION) {
        // Reading from older database version: backup just in case
        auto backup_path = fmt::format("{}.{}", dbPath, db_version);
        ByteBufferedFile::copy(dbPath, backup_path);
    }

    u32 nb_beatmaps = dbr.read<u32>();
    u32 nb_scores = dbr.read<u32>();
    this->scores.reserve(nb_beatmaps);

    for(u32 b = 0; b < nb_beatmaps; b++) {
        MD5Hash beatmap_hash = dbr.read_hash();
        u32 nb_beatmap_scores = dbr.read<u32>();

        for(u32 s = 0; s < nb_beatmap_scores; s++) {
            FinishedScore sc;

            sc.mods = Replay::Mods::unpack(dbr);
            sc.score = dbr.read<u64>();
            sc.spinner_bonus = dbr.read<u64>();
            sc.unixTimestamp = dbr.read<u64>();
            sc.player_id = dbr.read<i32>();
            sc.playerName = dbr.read_string();
            sc.grade = (ScoreGrade)dbr.read<u8>();

            sc.client = dbr.read_string();
            sc.server = dbr.read_string();
            sc.bancho_score_id = dbr.read<i64>();
            sc.peppy_replay_tms = dbr.read<u64>();

            sc.num300s = dbr.read<u16>();
            sc.num100s = dbr.read<u16>();
            sc.num50s = dbr.read<u16>();
            sc.numGekis = dbr.read<u16>();
            sc.numKatus = dbr.read<u16>();
            sc.numMisses = dbr.read<u16>();
            sc.comboMax = dbr.read<u16>();

            sc.ppv2_version = dbr.read<u32>();
            sc.ppv2_score = dbr.read<f32>();
            sc.ppv2_total_stars = dbr.read<f32>();
            sc.ppv2_aim_stars = dbr.read<f32>();
            sc.ppv2_speed_stars = dbr.read<f32>();

            sc.numSliderBreaks = dbr.read<u16>();
            sc.unstableRate = dbr.read<f32>();
            sc.hitErrorAvgMin = dbr.read<f32>();
            sc.hitErrorAvgMax = dbr.read<f32>();
            sc.maxPossibleCombo = dbr.read<u32>();
            sc.numHitObjects = dbr.read<u32>();
            sc.numCircles = dbr.read<u32>();

            sc.beatmap_hash = beatmap_hash;

            this->addScoreRaw(sc);
            nb_neosu_scores++;
        }

        u32 progress_bytes = this->bytes_processed + dbr.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
        this->loading_progress = std::clamp(progress_float, 0.01, 0.99);
    }

    if(nb_neosu_scores != nb_scores) {
        debugLog("Inconsistency in neosu_scores.db! Expected {:d} scores, found {:d}!", nb_scores, nb_neosu_scores);
    }

    debugLog("Loaded {:d} neosu scores", nb_neosu_scores);
    this->bytes_processed += dbr.total_size;
}

// import scores from mcosu, or old neosu (before we started saving replays)
void Database::loadOldMcNeosuScores(std::string_view dbPath) {
    ByteBufferedFile::Reader dbr(dbPath);

    u32 db_version = dbr.read<u32>();
    if(dbr.total_size == 0 || db_version == 0) {
        this->bytes_processed += dbr.total_size;
        return;
    }

    u32 nb_imported = 0;
    bool is_mcosu = (db_version == 20210106 || db_version == 20210108 || db_version == 20210110);
    bool is_neosu = !is_mcosu;

    if(is_neosu) {
        u32 nb_beatmaps = dbr.read<u32>();
        for(u32 b = 0; b < nb_beatmaps; b++) {
            u32 progress_bytes = this->bytes_processed + dbr.total_pos;
            f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
            this->loading_progress = std::clamp(progress_float, 0.01, 0.99);

            auto md5hash = dbr.read_hash();
            u32 nb_scores = dbr.read<u32>();

            for(u32 s = 0; s < nb_scores; s++) {
                dbr.skip<u8>();   // gamemode (always 0)
                dbr.skip<u32>();  // score version

                FinishedScore sc;
                sc.unixTimestamp = dbr.read<u64>();
                sc.playerName = dbr.read_string();
                sc.num300s = dbr.read<u16>();
                sc.num100s = dbr.read<u16>();
                sc.num50s = dbr.read<u16>();
                sc.numGekis = dbr.read<u16>();
                sc.numKatus = dbr.read<u16>();
                sc.numMisses = dbr.read<u16>();
                sc.score = dbr.read<u64>();
                sc.comboMax = dbr.read<u16>();
                sc.mods = Replay::Mods::from_legacy(dbr.read<LegacyFlags>());
                sc.numSliderBreaks = dbr.read<u16>();
                sc.ppv2_version = 20220902;
                sc.ppv2_score = dbr.read<f32>();
                sc.unstableRate = dbr.read<f32>();
                sc.hitErrorAvgMin = dbr.read<f32>();
                sc.hitErrorAvgMax = dbr.read<f32>();
                sc.ppv2_total_stars = dbr.read<f32>();
                sc.ppv2_aim_stars = dbr.read<f32>();
                sc.ppv2_speed_stars = dbr.read<f32>();
                sc.mods.speed = dbr.read<f32>();
                sc.mods.cs_override = dbr.read<f32>();
                sc.mods.ar_override = dbr.read<f32>();
                sc.mods.od_override = dbr.read<f32>();
                sc.mods.hp_override = dbr.read<f32>();
                sc.maxPossibleCombo = dbr.read<u32>();
                sc.numHitObjects = dbr.read<u32>();
                sc.numCircles = dbr.read<u32>();
                sc.bancho_score_id = dbr.read<u32>();
                sc.client = "neosu-win64-release-35.10";  // we don't know the actual version
                sc.server = dbr.read_string();

                std::string experimentalModsConVars = dbr.read_string();
                auto experimentalMods = SString::split(experimentalModsConVars, ';');
                for(const auto mod : experimentalMods) {
                    if(mod == "") continue;
                    if(mod == "fposu_mod_strafing") sc.mods.flags |= ModFlags::FPoSu_Strafing;
                    if(mod == "osu_mod_wobble") sc.mods.flags |= ModFlags::Wobble1;
                    if(mod == "osu_mod_wobble2") sc.mods.flags |= ModFlags::Wobble2;
                    if(mod == "osu_mod_arwobble") sc.mods.flags |= ModFlags::ARWobble;
                    if(mod == "osu_mod_timewarp") sc.mods.flags |= ModFlags::Timewarp;
                    if(mod == "osu_mod_artimewarp") sc.mods.flags |= ModFlags::ARTimewarp;
                    if(mod == "osu_mod_minimize") sc.mods.flags |= ModFlags::Minimize;
                    if(mod == "osu_mod_fadingcursor") sc.mods.flags |= ModFlags::FadingCursor;
                    if(mod == "osu_mod_fps") sc.mods.flags |= ModFlags::FPS;
                    if(mod == "osu_mod_jigsaw1") sc.mods.flags |= ModFlags::Jigsaw1;
                    if(mod == "osu_mod_jigsaw2") sc.mods.flags |= ModFlags::Jigsaw2;
                    if(mod == "osu_mod_fullalternate") sc.mods.flags |= ModFlags::FullAlternate;
                    if(mod == "osu_mod_reverse_sliders") sc.mods.flags |= ModFlags::ReverseSliders;
                    if(mod == "osu_mod_no50s") sc.mods.flags |= ModFlags::No50s;
                    if(mod == "osu_mod_no100s") sc.mods.flags |= ModFlags::No100s;
                    if(mod == "osu_mod_ming3012") sc.mods.flags |= ModFlags::Ming3012;
                    if(mod == "osu_mod_halfwindow") sc.mods.flags |= ModFlags::HalfWindow;
                    if(mod == "osu_mod_millhioref") sc.mods.flags |= ModFlags::Millhioref;
                    if(mod == "osu_mod_mafham") sc.mods.flags |= ModFlags::Mafham;
                    if(mod == "osu_mod_strict_tracking") sc.mods.flags |= ModFlags::StrictTracking;
                    if(mod == "osu_playfield_mirror_horizontal") sc.mods.flags |= ModFlags::MirrorHorizontal;
                    if(mod == "osu_playfield_mirror_vertical") sc.mods.flags |= ModFlags::MirrorVertical;
                    if(mod == "osu_mod_shirone") sc.mods.flags |= ModFlags::Shirone;
                    if(mod == "osu_mod_approach_different") sc.mods.flags |= ModFlags::ApproachDifferent;
                }

                sc.beatmap_hash = md5hash;
                sc.perfect = sc.comboMax >= sc.maxPossibleCombo;
                sc.grade = sc.calculate_grade();

                if(this->addScoreRaw(sc)) {
                    nb_imported++;
                }
            }
        }

        debugLog("Loaded {} old-neosu scores", nb_imported);
    } else {  // mcosu (this is copy-pasted from mcosu-ng)
        const int numBeatmaps = dbr.read<int32_t>();
        debugLog("McOsu scores: version = {}, numBeatmaps = {}", db_version, numBeatmaps);

        for(int b = 0; b < numBeatmaps; b++) {
            u32 progress_bytes = this->bytes_processed + dbr.total_pos;
            f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
            this->loading_progress = std::clamp(progress_float, 0.01, 0.99);

            const auto md5hash = dbr.read_hash();
            const int numScores = dbr.read<int32_t>();

            if(md5hash.length() < 32) {
                debugLog("WARNING: Invalid score on beatmap {} with md5hash.length() = {}!", b, md5hash.length());
                continue;
            } else if(md5hash.length() > 32) {
                debugLog("ERROR: Corrupt score database/entry detected, stopping.");
                break;
            }

            logIfCV(debug_db, "Beatmap[{}]: md5hash = {:s}, numScores = {}", b, md5hash.string(), numScores);

            for(u32 s = 0; s < numScores; s++) {
                const auto gamemode = dbr.read<uint8_t>();  // NOTE: abused as isImportedLegacyScore flag (because I
                                                            // forgot to add a version cap to old builds)
                const int scoreVersion = dbr.read<int32_t>();
                if(db_version == 20210103 && scoreVersion > 20190103) {
                    /* isImportedLegacyScore = */ dbr.skip<uint8_t>();  // too lazy to handle this logic
                }
                const auto unixTimestamp = dbr.read<uint64_t>();
                if(this->isScoreAlreadyInDB(unixTimestamp, md5hash) >= 0) {
                    dbr.skip_string();  // playerName
                    u32 bytesToSkipUntilNextScore = 0;
                    bytesToSkipUntilNextScore +=
                        (sizeof(uint16_t) * 8) + (sizeof(int64_t)) + (sizeof(int32_t)) + (sizeof(f32) * 12);
                    if(scoreVersion > 20180722) {
                        // maxPossibleCombos
                        bytesToSkipUntilNextScore += sizeof(int32_t) * 3;
                    }
                    dbr.skip_bytes(bytesToSkipUntilNextScore);
                    dbr.skip_string();  // experimentalMods
                    logIfCV(debug_db, "skipped score {} (already loaded from neosu_scores.db)", md5hash.string());
                    continue;
                }

                // default
                const std::string playerName{dbr.read_string()};

                const auto num300s = dbr.read<uint16_t>();
                const auto num100s = dbr.read<uint16_t>();
                const auto num50s = dbr.read<uint16_t>();
                const auto numGekis = dbr.read<uint16_t>();
                const auto numKatus = dbr.read<uint16_t>();
                const auto numMisses = dbr.read<uint16_t>();

                const auto score = dbr.read<int64_t>();
                const auto maxCombo = dbr.read<uint16_t>();
                const auto mods = Replay::Mods::from_legacy(dbr.read<LegacyFlags>());

                // custom
                const auto numSliderBreaks = dbr.read<uint16_t>();
                const auto pp = dbr.read<f32>();
                const auto unstableRate = dbr.read<f32>();
                const auto hitErrorAvgMin = dbr.read<f32>();
                const auto hitErrorAvgMax = dbr.read<f32>();
                const auto starsTomTotal = dbr.read<f32>();
                const auto starsTomAim = dbr.read<f32>();
                const auto starsTomSpeed = dbr.read<f32>();
                const auto speedMultiplier = dbr.read<f32>();
                const auto CS = dbr.read<f32>();
                const auto AR = dbr.read<f32>();
                const auto OD = dbr.read<f32>();
                const auto HP = dbr.read<f32>();

                int maxPossibleCombo = -1;
                int numHitObjects = -1;
                int numCircles = -1;
                if(scoreVersion > 20180722) {
                    maxPossibleCombo = dbr.read<int32_t>();
                    numHitObjects = dbr.read<int32_t>();
                    numCircles = dbr.read<int32_t>();
                }

                std::string experimentalModsConVars = dbr.read_string();
                auto experimentalMods = SString::split(experimentalModsConVars, ';');

                if(gamemode == 0x0 || (db_version > 20210103 &&
                                       scoreVersion > 20190103))  // gamemode filter (osu!standard) // HACKHACK: for
                                                                  // explanation see hackIsImportedLegacyScoreFlag
                {
                    FinishedScore sc;

                    sc.unixTimestamp = unixTimestamp;

                    // default
                    sc.playerName = playerName;

                    sc.num300s = num300s;
                    sc.num100s = num100s;
                    sc.num50s = num50s;
                    sc.numGekis = numGekis;
                    sc.numKatus = numKatus;
                    sc.numMisses = numMisses;
                    sc.score = score;
                    sc.comboMax = maxCombo;
                    sc.perfect = (maxPossibleCombo > 0 && sc.comboMax > 0 && sc.comboMax >= maxPossibleCombo);
                    sc.mods = mods;

                    // custom
                    sc.numSliderBreaks = numSliderBreaks;
                    sc.ppv2_version = 20220902;
                    sc.ppv2_score = pp;
                    sc.unstableRate = unstableRate;
                    sc.hitErrorAvgMin = hitErrorAvgMin;
                    sc.hitErrorAvgMax = hitErrorAvgMax;
                    sc.ppv2_total_stars = starsTomTotal;
                    sc.ppv2_aim_stars = starsTomAim;
                    sc.ppv2_speed_stars = starsTomSpeed;
                    sc.mods.speed = speedMultiplier;
                    sc.mods.cs_override = CS;
                    sc.mods.ar_override = AR;
                    sc.mods.od_override = OD;
                    sc.mods.hp_override = HP;
                    sc.maxPossibleCombo = maxPossibleCombo;
                    sc.numHitObjects = numHitObjects;
                    sc.numCircles = numCircles;
                    for(const auto mod : experimentalMods) {
                        if(mod == "") continue;
                        if(mod == "fposu_mod_strafing") sc.mods.flags |= ModFlags::FPoSu_Strafing;
                        if(mod == "osu_mod_wobble") sc.mods.flags |= ModFlags::Wobble1;
                        if(mod == "osu_mod_wobble2") sc.mods.flags |= ModFlags::Wobble2;
                        if(mod == "osu_mod_arwobble") sc.mods.flags |= ModFlags::ARWobble;
                        if(mod == "osu_mod_timewarp") sc.mods.flags |= ModFlags::Timewarp;
                        if(mod == "osu_mod_artimewarp") sc.mods.flags |= ModFlags::ARTimewarp;
                        if(mod == "osu_mod_minimize") sc.mods.flags |= ModFlags::Minimize;
                        if(mod == "osu_mod_fadingcursor") sc.mods.flags |= ModFlags::FadingCursor;
                        if(mod == "osu_mod_fps") sc.mods.flags |= ModFlags::FPS;
                        if(mod == "osu_mod_jigsaw1") sc.mods.flags |= ModFlags::Jigsaw1;
                        if(mod == "osu_mod_jigsaw2") sc.mods.flags |= ModFlags::Jigsaw2;
                        if(mod == "osu_mod_fullalternate") sc.mods.flags |= ModFlags::FullAlternate;
                        if(mod == "osu_mod_reverse_sliders") sc.mods.flags |= ModFlags::ReverseSliders;
                        if(mod == "osu_mod_no50s") sc.mods.flags |= ModFlags::No50s;
                        if(mod == "osu_mod_no100s") sc.mods.flags |= ModFlags::No100s;
                        if(mod == "osu_mod_ming3012") sc.mods.flags |= ModFlags::Ming3012;
                        if(mod == "osu_mod_halfwindow") sc.mods.flags |= ModFlags::HalfWindow;
                        if(mod == "osu_mod_millhioref") sc.mods.flags |= ModFlags::Millhioref;
                        if(mod == "osu_mod_mafham") sc.mods.flags |= ModFlags::Mafham;
                        if(mod == "osu_mod_strict_tracking") sc.mods.flags |= ModFlags::StrictTracking;
                        if(mod == "osu_playfield_mirror_horizontal") sc.mods.flags |= ModFlags::MirrorHorizontal;
                        if(mod == "osu_playfield_mirror_vertical") sc.mods.flags |= ModFlags::MirrorVertical;
                        if(mod == "osu_mod_shirone") sc.mods.flags |= ModFlags::Shirone;
                        if(mod == "osu_mod_approach_different") sc.mods.flags |= ModFlags::ApproachDifferent;
                    }

                    sc.beatmap_hash = md5hash;
                    sc.perfect = sc.comboMax >= sc.maxPossibleCombo;
                    sc.grade = sc.calculate_grade();
                    sc.client = fmt::format("mcosu-{}", scoreVersion);

                    if(this->addScoreRaw(sc)) {
                        nb_imported++;
                    }
                }
            }
        }
        debugLog("Loaded {} McOsu scores", nb_imported);
    }

    this->bytes_processed += dbr.total_size;
}

void Database::loadPeppyScores(std::string_view dbPath) {
    ByteBufferedFile::Reader dbr(dbPath);
    int nb_imported = 0;

    u32 db_version = dbr.read<u32>();
    u32 nb_beatmaps = dbr.read<u32>();
    if(dbr.total_size == 0 || db_version == 0) {
        this->bytes_processed += dbr.total_size;
        return;
    }

    debugLog("osu!stable scores.db: version = {:d}, nb_beatmaps = {:d}", db_version, nb_beatmaps);

    char client_str[15] = "peppy-YYYYMMDD";
    for(u32 b = 0; b < nb_beatmaps; b++) {
        std::string md5hash_str = dbr.read_string();
        if(md5hash_str.length() < 32) {
            debugLog("WARNING: Invalid score on beatmap {:d} with md5hash_str.length() = {:d}!", b,
                     md5hash_str.length());
            continue;
        } else if(md5hash_str.length() > 32) {
            debugLog("ERROR: Corrupt score database/entry detected, stopping.");
            break;
        }

        MD5Hash md5hash{md5hash_str.c_str()};

        u32 nb_scores = dbr.read<u32>();

        for(u32 s = 0; s < nb_scores; s++) {
            FinishedScore sc;

            u8 gamemode = dbr.read<u8>();

            u32 score_version = dbr.read<u32>();
            snprintf(client_str, 14, "peppy-%d", score_version);
            sc.client = client_str;

            sc.server = "ppy.sh";
            dbr.skip_string();  // beatmap hash (already have it)
            sc.playerName = dbr.read_string();
            dbr.skip_string();  // replay hash (unused)

            sc.num300s = dbr.read<u16>();
            sc.num100s = dbr.read<u16>();
            sc.num50s = dbr.read<u16>();
            sc.numGekis = dbr.read<u16>();
            sc.numKatus = dbr.read<u16>();
            sc.numMisses = dbr.read<u16>();

            i32 score = dbr.read<i32>();
            sc.score = (score < 0 ? 0 : score);

            sc.comboMax = dbr.read<u16>();
            sc.perfect = dbr.read<u8>();
            sc.mods = Replay::Mods::from_legacy(dbr.read<LegacyFlags>());

            dbr.skip_string();  // hp graph

            u64 full_tms = dbr.read<u64>();
            sc.unixTimestamp = (full_tms - 621355968000000000) / 10000000;
            sc.peppy_replay_tms = full_tms - 504911232000000000;

            // Always -1, but let's skip it properly just in case
            i32 old_replay_size = dbr.read<i32>();
            if(old_replay_size > 0) {
                dbr.skip_bytes(old_replay_size);
            }

            if(score_version >= 20131110) {
                sc.bancho_score_id = dbr.read<i64>();
            } else if(score_version >= 20121008) {
                sc.bancho_score_id = dbr.read<i32>();
            } else {
                sc.bancho_score_id = 0;
            }

            if(sc.mods.has(ModFlags::Target)) {
                dbr.skip<f64>();  // total accuracy
            }

            if(gamemode == 0 && sc.bancho_score_id != 0) {
                sc.beatmap_hash = md5hash;
                sc.grade = sc.calculate_grade();

                if(this->addScoreRaw(sc)) {
                    nb_imported++;
                }
            }
        }

        u32 progress_bytes = this->bytes_processed + dbr.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
        this->loading_progress = std::clamp(progress_float, 0.01, 0.99);
    }

    debugLog("Loaded {:d} osu!stable scores", nb_imported);
    this->bytes_processed += dbr.total_size;
}

void Database::saveScores() {
    debugLog("Osu: Saving scores ...");
    if(!this->scores_loaded) {
        debugLog("Cannot save scores since they weren't loaded properly first!");
        return;
    }

    const double startTime = Timing::getTimeReal();

    const auto neosu_scores_db = getDBPath(DatabaseType::NEOSU_SCORES);

    ByteBufferedFile::Writer dbr(neosu_scores_db);

    if(!dbr.good()) {
        debugLog("Cannot save scores to {}: {}", neosu_scores_db, dbr.error());
        return;
    }

    dbr.write_bytes((u8 *)"NEOSC", 5);
    dbr.write<u32>(NEOSU_SCORE_DB_VERSION);

    u32 nb_beatmaps = 0;
    u32 nb_scores = 0;

    Sync::shared_lock lock(this->scores_mtx);  // only need read lock here
    for(const auto &[_, scorevec] : this->scores) {
        u32 beatmap_scores = scorevec.size();
        if(beatmap_scores > 0) {
            nb_beatmaps++;
            nb_scores += beatmap_scores;
        }
    }
    dbr.write<u32>(nb_beatmaps);
    dbr.write<u32>(nb_scores);

    for(const auto &[hash, scorevec] : this->scores) {
        if(scorevec.empty()) continue;
        if(!dbr.good()) {
            break;
        }

        dbr.write_hash(hash);
        dbr.write<u32>(scorevec.size());

        for(const auto &score : scorevec) {
            assert(!score.is_online_score);
            if(!dbr.good()) {
                break;
            }

            Replay::Mods::pack_and_write(dbr, score.mods);
            dbr.write<u64>(score.score);
            dbr.write<u64>(score.spinner_bonus);
            dbr.write<u64>(score.unixTimestamp);
            dbr.write<i32>(score.player_id);
            dbr.write_string(score.playerName);
            dbr.write<u8>((u8)score.grade);

            dbr.write_string(score.client);
            dbr.write_string(score.server);
            dbr.write<i64>(score.bancho_score_id);
            dbr.write<u64>(score.peppy_replay_tms);

            dbr.write<u16>(score.num300s);
            dbr.write<u16>(score.num100s);
            dbr.write<u16>(score.num50s);
            dbr.write<u16>(score.numGekis);
            dbr.write<u16>(score.numKatus);
            dbr.write<u16>(score.numMisses);
            dbr.write<u16>(score.comboMax);

            dbr.write<u32>(score.ppv2_version);
            dbr.write<f32>(score.ppv2_score);
            dbr.write<f32>(score.ppv2_total_stars);
            dbr.write<f32>(score.ppv2_aim_stars);
            dbr.write<f32>(score.ppv2_speed_stars);

            dbr.write<u16>(score.numSliderBreaks);
            dbr.write<f32>(score.unstableRate);
            dbr.write<f32>(score.hitErrorAvgMin);
            dbr.write<f32>(score.hitErrorAvgMax);
            dbr.write<u32>(score.maxPossibleCombo);
            dbr.write<u32>(score.numHitObjects);
            dbr.write<u32>(score.numCircles);
        }
    }

    debugLog("Saved {:d} scores in {:f} seconds.", nb_scores, (Timing::getTimeReal() - startTime));
}

BeatmapSet *Database::loadRawBeatmap(const std::string &beatmapPath) {
    logIfCV(debug_db, "beatmap path: {:s}", beatmapPath);

    // try loading all diffs
    auto *diffs2 = new std::vector<BeatmapDifficulty *>();
    std::vector<std::string> beatmapFiles = env->getFilesInFolder(beatmapPath);
    for(const auto &beatmapFile : beatmapFiles) {
        std::string ext = env->getFileExtensionFromFilePath(beatmapFile);
        if(ext.compare("osu") != 0) continue;

        std::string fullFilePath = beatmapPath;
        fullFilePath.append(beatmapFile);

        auto *map = new BeatmapDifficulty(fullFilePath, beatmapPath, DatabaseBeatmap::BeatmapType::NEOSU_DIFFICULTY);
        if(map->loadMetadata()) {
            diffs2->push_back(map);
        } else {
            logIfCV(debug_db, "Couldn't loadMetadata(), deleting object.");
            SAFE_DELETE(map);
        }
    }

    BeatmapSet *set = nullptr;
    if(diffs2->empty()) {
        delete diffs2;
    } else {
        set = new BeatmapSet(diffs2, DatabaseBeatmap::BeatmapType::NEOSU_BEATMAPSET);
    }

    return set;
}
