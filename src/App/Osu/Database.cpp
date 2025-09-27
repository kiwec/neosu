// Copyright (c) 2016, PG, All rights reserved.
#include "Database.h"

#include "Bancho.h"
#include "SString.h"
#include "MD5Hash.h"
#include "ByteBufferedFile.h"
#include "Collections.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "File.h"
#include "LegacyReplay.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "SongBrowser/LeaderboardPPCalcThread.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/MapCalcThread.h"
#include "SongBrowser/ScoreConverterThread.h"
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

namespace {  // static namespace
bool sortScoreByScore(FinishedScore const &a, FinishedScore const &b) {
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool sortScoreByCombo(FinishedScore const &a, FinishedScore const &b) {
    if(a.comboMax != b.comboMax) return a.comboMax > b.comboMax;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool sortScoreByDate(FinishedScore const &a, FinishedScore const &b) {
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool sortScoreByMisses(FinishedScore const &a, FinishedScore const &b) {
    if(a.numMisses != b.numMisses) return a.numMisses < b.numMisses;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool sortScoreByAccuracy(FinishedScore const &a, FinishedScore const &b) {
    auto a_acc = LiveScore::calculateAccuracy(a.num300s, a.num100s, a.num50s, a.numMisses);
    auto b_acc = LiveScore::calculateAccuracy(b.num300s, b.num100s, b.num50s, b.numMisses);
    if(a_acc != b_acc) return a_acc > b_acc;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

bool sortScoreByPP(FinishedScore const &a, FinishedScore const &b) {
    auto a_pp = std::max(a.get_pp() * 1000.0, 0.0);
    auto b_pp = std::max(b.get_pp() * 1000.0, 0.0);
    if(a_pp != b_pp) return a_pp > b_pp;
    if(a.score != b.score) return a.score > b.score;
    if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
    if(a.player_id != b.player_id) return a.player_id > b.player_id;
    if(a.play_time_ms != b.play_time_ms) return a.play_time_ms > b.play_time_ms;
    return false;  // equivalent
}

}  // namespace

// run after at least one engine frame (due to resourceManager->update() in Engine::onUpdate())
void Database::AsyncDBLoader::init() {
    if(!db) return;  // don't crash when exiting while loading db

    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) start");

    if(db->bNeedRawLoad) {
        db->scheduleLoadRaw();
    } else {
        MapCalcThread::start_calc(db->maps_to_recalc);
        VolNormalization::start_calc(db->loudness_to_calc);
        sct_calc(db->scores);
    }

    // signal that we are done
    db->fLoadingProgress = 1.0f;
    this->bReady = true;

    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) done");
}

// run immediately on a separate thread when resourceManager->loadResource() is called
void Database::AsyncDBLoader::initAsync() {
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) start");
    assert(db != nullptr);

    std::string peppy_scores_path = cv::osu_folder.getString();
    peppy_scores_path.append("/scores.db");

    db->findDatabases();
    if(db->bInterruptLoad.load()) goto done;
    db->loadScores(db->database_files["neosu_scores.db"]);
    if(db->bInterruptLoad.load()) goto done;
    db->loadOldMcNeosuScores(db->database_files["scores.db"]);
    if(db->bInterruptLoad.load()) goto done;
    db->loadPeppyScores(db->database_files[peppy_scores_path]);
    db->bScoresLoaded = true;
    if(db->bInterruptLoad.load()) goto done;

    db->loadMaps();
    if(db->bInterruptLoad.load()) goto done;

    // .db files that were dropped on the main window
    for(const auto &db_path : db->dbPathsToImport) {
        db->importDatabase(db_path);
        if(db->bInterruptLoad.load()) goto done;
    }

    if(!db->bNeedRawLoad) {
        load_collections();
    }

done:
    db->dbPathsToImport.clear();

    this->bAsyncReady = true;
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("(AsyncDBLoader) done");
}

void Database::startLoader() {
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("start");
    this->destroyLoader();

    // stop threads that rely on database content
    sct_abort();
    lct_set_map(nullptr);
    MapCalcThread::abort();
    VolNormalization::abort();

    // only clear diffs/sets for full reloads (only handled for raw re-loading atm)
    const bool lastLoadWasRaw{this->bNeedRawLoad};

    this->bNeedRawLoad =
        (!env->fileExists(fmt::format("{}/osu!.db", cv::osu_folder.getString())) || !cv::database_enabled.getBool());

    const bool nextLoadIsRaw{this->bNeedRawLoad};

    if(!lastLoadWasRaw || !nextLoadIsRaw) {
        db->loudness_to_calc.clear();
        db->maps_to_recalc.clear();
        {
            Sync::scoped_lock lock(this->beatmap_difficulties_mtx);
            this->beatmap_difficulties.clear();
        }
        for(auto &beatmapset : db->beatmapsets) {
            SAFE_DELETE(beatmapset);
        }
        db->beatmapsets.clear();
    }

    this->loader = new AsyncDBLoader();
    resourceManager->requestNextLoadAsync();
    resourceManager->loadResource(this->loader);

    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("done");
}

void Database::destroyLoader() {
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("start");
    if(this->loader) {
        resourceManager->destroyResource(this->loader, ResourceManager::DestroyMode::FORCE_BLOCKING);  // force blocking
        this->loader = nullptr;
    }
    if(cv::debug_db.getBool() || cv::debug_async_db.getBool()) debugLog("done");
}

Database::Database() {
    // vars
    this->importTimer = new Timer();
    this->bIsFirstLoad = true;
    this->bFoundChanges = true;

    this->iNumBeatmapsToLoad = 0;
    this->fLoadingProgress = 0.0f;
    this->bInterruptLoad = false;

    this->iVersion = 0;
    this->iFolderCount = 0;

    this->prevPlayerStats.pp = 0.0f;
    this->prevPlayerStats.accuracy = 0.0f;
    this->prevPlayerStats.numScoresWithPP = 0;
    this->prevPlayerStats.level = 0;
    this->prevPlayerStats.percentToNextLevel = 0.0f;
    this->prevPlayerStats.totalScore = 0;

    this->scoreSortingMethods = {{{.name = "By accuracy", .comparator = sortScoreByAccuracy},
                                  {.name = "By combo", .comparator = sortScoreByCombo},
                                  {.name = "By date", .comparator = sortScoreByDate},
                                  {.name = "By misses", .comparator = sortScoreByMisses},
                                  {.name = "By score", .comparator = sortScoreByScore},
                                  {.name = "By pp", .comparator = sortScoreByPP}}};
}

Database::~Database() {
    this->destroyLoader();

    SAFE_DELETE(this->importTimer);

    sct_abort();
    lct_set_map(nullptr);
    VolNormalization::abort();
    this->loudness_to_calc.clear();

    MapCalcThread::abort();
    this->maps_to_recalc.clear();

    {
        Sync::scoped_lock lock(this->beatmap_difficulties_mtx);
        this->beatmap_difficulties.clear();
    }
    for(auto &beatmapset : this->beatmapsets) {
        SAFE_DELETE(beatmapset);
    }
    this->beatmapsets.clear();

    unload_collections();
}

void Database::update() {
    // loadRaw() logic
    if(this->bRawBeatmapLoadScheduled) {
        Timer t;

        while(t.getElapsedTime() < 0.033f) {
            if(this->bInterruptLoad.load()) break;  // cancellation point

            if(this->rawLoadBeatmapFolders.size() > 0 &&
               this->iCurRawBeatmapLoadIndex < this->rawLoadBeatmapFolders.size()) {
                std::string curBeatmap = this->rawLoadBeatmapFolders[this->iCurRawBeatmapLoadIndex++];
                this->rawBeatmapFolders.push_back(
                    curBeatmap);  // for future incremental loads, so that we know what's been loaded already

                std::string fullBeatmapPath = this->sRawBeatmapLoadOsuSongFolder;
                fullBeatmapPath.append(curBeatmap);
                fullBeatmapPath.append("/");

                this->addBeatmapSet(fullBeatmapPath);
            }

            // update progress
            this->fLoadingProgress = (float)this->iCurRawBeatmapLoadIndex / (float)this->iNumBeatmapsToLoad;

            // check if we are finished
            if(this->iCurRawBeatmapLoadIndex >= this->iNumBeatmapsToLoad ||
               std::cmp_greater(this->iCurRawBeatmapLoadIndex, (this->rawLoadBeatmapFolders.size() - 1))) {
                this->rawLoadBeatmapFolders.clear();
                this->bRawBeatmapLoadScheduled = false;
                this->importTimer->update();

                debugLog("Refresh finished, added {} beatmaps in {:f} seconds.", this->beatmapsets.size(),
                         this->importTimer->getElapsedTime());

                load_collections();

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

                this->fLoadingProgress = 1.0f;

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
    this->bInterruptLoad = false;
    this->fLoadingProgress = 0.0f;

    // reset scheduled logic
    this->bRawBeatmapLoadScheduled = false;

    this->startLoader();
}

void Database::cancel() {
    this->bInterruptLoad = true;
    this->fLoadingProgress = 1.0f;  // force finished
    this->bFoundChanges = true;
}

void Database::save() {
    save_collections();
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

    this->beatmapsets.push_back(beatmap);

    this->beatmap_difficulties_mtx.lock();
    for(const auto &diff : beatmap->getDifficulties()) {
        this->beatmap_difficulties[diff->getMD5Hash()] = diff;
    }
    this->beatmap_difficulties_mtx.unlock();

    osu->getSongBrowser()->addBeatmapSet(beatmap);

    // XXX: Very slow
    osu->getSongBrowser()->onSortChangeInt(cv::songbrowser_sortingtype.getString().c_str());

    return beatmap;
}

int Database::addScore(const FinishedScore &score) {
    this->addScoreRaw(score);
    this->sortScores(score.beatmap_hash);

    this->bDidScoresChangeForStats = true;

    if(cv::scores_save_immediately.getBool()) this->saveScores();

    // @PPV3: use new replay format

    // XXX: this is blocking main thread
    auto compressed_replay = LegacyReplay::compress_frames(score.replay);
    if(!compressed_replay.empty()) {
        auto replay_path = fmt::format(NEOSU_REPLAYS_PATH "/{:d}.replay.lzma", score.unixTimestamp);

        FILE *replay_file = fopen(replay_path.c_str(), "wb");
        if(replay_file != nullptr) {
            fwrite(compressed_replay.data(), compressed_replay.size(), 1, replay_file);
            fclose(replay_file);
        }
    }

    // return sorted index
    Sync::scoped_lock lock(this->scores_mtx);
    for(int i = 0; i < this->scores[score.beatmap_hash].size(); i++) {
        if(this->scores[score.beatmap_hash][i].unixTimestamp == score.unixTimestamp) return i;
    }

    return -1;
}

int Database::isScoreAlreadyInDB(u64 unix_timestamp, const MD5Hash &map_hash) {
    Sync::scoped_lock lock(this->scores_mtx);

    for(int existing_pos = -1; const auto &existing : this->scores[map_hash]) {
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
            Sync::scoped_lock lock(this->scores_mtx);
            // otherwise check if the old one doesn't have a replay
            // if it has one, don't overwrite it
            overwrite = !this->scores[score.beatmap_hash][existing_pos].has_possible_replay();
        }

        if(!overwrite) {
            return false;
        }
        // otherwise overwrite it
    }

    Sync::scoped_lock lock(this->scores_mtx);

    if(overwrite) {
        this->scores[score.beatmap_hash][existing_pos] = score;
    } else {
        // new score
        this->scores[score.beatmap_hash].push_back(score);
    }

    return true;
}

void Database::deleteScore(MD5Hash beatmapMD5Hash, u64 scoreUnixTimestamp) {
    Sync::scoped_lock lock(this->scores_mtx);
    for(int i = 0; i < this->scores[beatmapMD5Hash].size(); i++) {
        if(this->scores[beatmapMD5Hash][i].unixTimestamp == scoreUnixTimestamp) {
            this->scores[beatmapMD5Hash].erase(this->scores[beatmapMD5Hash].begin() + i);
            this->bDidScoresChangeForStats = true;
            break;
        }
    }
}

void Database::sortScoresInPlace(std::vector<FinishedScore> &scores) {
    if(scores.size() < 2) return;

    const auto &sortTypeString{cv::songbrowser_scores_sortingtype.getString()};
    for(const auto &sortMethod : this->scoreSortingMethods) {
        if(sortTypeString == sortMethod.name) {
            std::ranges::sort(scores, sortMethod.comparator);
            return;
        }
    }

    // Fallback
    cv::songbrowser_scores_sortingtype.setValue("By pp");
    std::ranges::sort(scores, sortScoreByPP);
}

void Database::sortScores(MD5Hash beatmapMD5Hash) {
    Sync::scoped_lock lock(this->scores_mtx);
    this->sortScoresInPlace(this->scores[beatmapMD5Hash]);
}

std::vector<UString> Database::getPlayerNamesWithPPScores() {
    Sync::scoped_lock lock(this->scores_mtx);
    std::vector<MD5Hash> keys;
    keys.reserve(this->scores.size());

    for(const auto &[hash, _] : this->scores) {
        keys.push_back(hash);
    }

    std::unordered_set<std::string> tempNames;
    for(const auto &key : keys) {
        for(const auto &name :
            this->scores[key] | std::views::transform([](const auto &score) -> auto & { return score.playerName; })) {
            tempNames.insert(name);
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

std::vector<UString> Database::getPlayerNamesWithScoresForUserSwitcher() {
    Sync::scoped_lock lock(this->scores_mtx);
    std::unordered_set<std::string> tempNames;
    for(const auto &[hash, _] : this->scores) {
        for(const auto &name :
            this->scores[hash] | std::views::transform([](const auto &score) -> auto & { return score.playerName; })) {
            tempNames.insert(name);
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
    Sync::scoped_lock lock(this->scores_mtx);
    PlayerPPScores ppScores;
    ppScores.totalScore = 0;
    if(this->getProgress() < 1.0f) return ppScores;

    std::vector<FinishedScore *> scores;

    // collect all scores with pp data
    std::vector<MD5Hash> keys;
    keys.reserve(this->scores.size());

    for(const auto &kv : this->scores) {
        keys.push_back(kv.first);
    }

    unsigned long long totalScore = 0;
    for(const auto &key : keys) {
        if(this->scores[key].size() == 0) continue;

        FinishedScore *tempScore = &this->scores[key][0];

        // only add highest pp score per diff
        bool foundValidScore = false;
        float prevPP = -1.0f;
        for(auto &score : this->scores[key]) {
            auto uses_rx_or_ap = (ModMasks::eq(score.mods.flags, Replay::ModFlags::Relax) ||
                                  (ModMasks::eq(score.mods.flags, Replay::ModFlags::Autopilot)));
            if(uses_rx_or_ap && !cv::user_include_relax_and_autopilot_for_stats.getBool()) continue;

            if(score.playerName != playerName) continue;

            foundValidScore = true;
            totalScore += score.score;

            if(score.get_pp() > prevPP || prevPP < 0.0f) {
                prevPP = score.get_pp();
                tempScore = &score;
            }
        }

        if(foundValidScore) scores.push_back(tempScore);
    }

    // sort by pp
    // for some reason this was originally backwards from sortScoreByPP, so negating it here
    std::ranges::sort(scores, [](FinishedScore *a, FinishedScore *b) -> bool { return !sortScoreByPP(*a, *b); });

    ppScores.ppScores = std::move(scores);
    ppScores.totalScore = totalScore;

    return ppScores;
}

Database::PlayerStats Database::calculatePlayerStats(const std::string &playerName) {
    if(!this->bDidScoresChangeForStats.load() && playerName == this->prevPlayerStats.name.utf8View())
        return this->prevPlayerStats;

    const PlayerPPScores ps = this->getPlayerPPScores(playerName);

    // delay caching until we actually have scores loaded
    Sync::scoped_lock lock(this->scores_mtx);
    if(ps.ppScores.size() > 0 || db->isFinished()) this->bDidScoresChangeForStats = false;

    // "If n is the amount of scores giving more pp than a given score, then the score's weight is 0.95^n"
    // "Total pp = PP[1] * 0.95^0 + PP[2] * 0.95^1 + PP[3] * 0.95^2 + ... + PP[n] * 0.95^(n-1)"
    // also, total accuracy is apparently weighted the same as pp

    float pp = 0.0f;
    float acc = 0.0f;
    for(size_t i = 0; i < ps.ppScores.size(); i++) {
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

        const unsigned long long requiredScoreForCurrentLevel = getRequiredScoreForLevel(this->prevPlayerStats.level);
        const unsigned long long requiredScoreForNextLevel = getRequiredScoreForLevel(this->prevPlayerStats.level + 1);

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

unsigned long long Database::getRequiredScoreForLevel(int level) {
    // https://zxq.co/ripple/ocl/src/branch/master/level.go
    if(level <= 100) {
        if(level > 1)
            return (u64)std::floor(5000 / 3 * (4 * pow(level, 3) - 3 * pow(level, 2) - level) +
                                   std::floor(1.25 * pow(1.8, (double)(level - 60))));

        return 1;
    }

    return (u64)26931190829 + (u64)100000000000 * (u64)(level - 100);
}

int Database::getLevelForScore(unsigned long long score, int maxLevel) {
    // https://zxq.co/ripple/ocl/src/branch/master/level.go
    int i = 0;
    while(true) {
        if(maxLevel > 0 && i >= maxLevel) return i;

        const unsigned long long lScore = getRequiredScoreForLevel(i);

        if(score < lScore) return (i - 1);

        i++;
    }
}

DatabaseBeatmap *Database::getBeatmapDifficulty(const MD5Hash &md5hash) {
    if(this->isLoading()) return nullptr;

    Sync::scoped_lock lock(this->beatmap_difficulties_mtx);
    auto it = this->beatmap_difficulties.find(md5hash);
    if(it == this->beatmap_difficulties.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

DatabaseBeatmap *Database::getBeatmapDifficulty(i32 map_id) {
    if(this->isLoading()) return nullptr;

    Sync::scoped_lock lock(this->beatmap_difficulties_mtx);
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
    this->sRawBeatmapLoadOsuSongFolder = Database::getOsuSongsFolder();

    debugLog("Database: sRawBeatmapLoadOsuSongFolder = {:s}", this->sRawBeatmapLoadOsuSongFolder);

    this->rawLoadBeatmapFolders = env->getFoldersInFolder(this->sRawBeatmapLoadOsuSongFolder);
    this->iNumBeatmapsToLoad = this->rawLoadBeatmapFolders.size();

    // if this isn't the first load, only load the differences
    if(!this->bIsFirstLoad) {
        std::vector<std::string> toLoad;
        for(int i = 0; i < this->iNumBeatmapsToLoad; i++) {
            bool alreadyLoaded = false;
            for(const auto &rawBeatmapFolder : this->rawBeatmapFolders) {
                if(this->rawLoadBeatmapFolders[i] == rawBeatmapFolder) {
                    alreadyLoaded = true;
                    break;
                }
            }

            if(!alreadyLoaded) toLoad.push_back(this->rawLoadBeatmapFolders[i]);
        }

        // only load differences
        this->rawLoadBeatmapFolders = toLoad;
        this->iNumBeatmapsToLoad = this->rawLoadBeatmapFolders.size();

        debugLog("Database: Found {} new/changed beatmaps.", this->iNumBeatmapsToLoad);

        this->bFoundChanges = this->iNumBeatmapsToLoad > 0;
        if(this->bFoundChanges)
            osu->getNotificationOverlay()->addNotification(
                UString::format(this->iNumBeatmapsToLoad == 1 ? "Adding %i new beatmap." : "Adding %i new beatmaps.",
                                this->iNumBeatmapsToLoad),
                0xff00ff00);
        else
            osu->getNotificationOverlay()->addNotification(
                UString::format("No new beatmaps detected.", this->iNumBeatmapsToLoad), 0xff00ff00);
    }

    debugLog("Database: Building beatmap database ...");
    debugLog("Database: Found {} folders to load.", this->rawLoadBeatmapFolders.size());

    // only start loading if we have something to load
    if(this->rawLoadBeatmapFolders.size() > 0) {
        this->fLoadingProgress = 0.0f;
        this->iCurRawBeatmapLoadIndex = 0;

        this->bRawBeatmapLoadScheduled = true;
        this->importTimer->start();
    } else
        this->fLoadingProgress = 1.0f;

    this->bIsFirstLoad = false;
}

void Database::loadMaps() {
    Sync::scoped_lock lock(this->beatmap_difficulties_mtx);
    this->peppy_overrides_mtx.lock();

    std::string peppy_maps_path = cv::osu_folder.getString();
    peppy_maps_path.append("/osu!.db");
    const auto &peppy_db_path = this->database_files[peppy_maps_path];
    const auto &neosu_maps_path = this->database_files["neosu_maps.db"];

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
    // We don't want to reload it, ever. Would cause too much jank.
    static bool first_load = true;
    {
        ByteBufferedFile::Reader neosu_maps(neosu_maps_path);
        if(first_load && neosu_maps.total_size > 0) {
            first_load = false;

            u32 version = neosu_maps.read<u32>();
            if(version < NEOSU_MAPS_DB_VERSION) {
                // Reading from older database version: backup just in case
                auto backup_path = fmt::format("neosu_maps.db.{}", version);
                ByteBufferedFile::copy("neosu_maps.db", backup_path);
            }

            u32 nb_sets = neosu_maps.read<u32>();
            for(u32 i = 0; i < nb_sets; i++) {
                if(this->bInterruptLoad.load()) break;  // cancellation point

                u32 progress_bytes = this->bytes_processed + neosu_maps.total_pos;
                f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
                this->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);

                i32 set_id = neosu_maps.read<i32>();
                u16 nb_diffs = neosu_maps.read<u16>();

                std::string mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);

                auto *diffs = new std::vector<DatabaseBeatmap *>();
                for(u16 j = 0; j < nb_diffs; j++) {
                    if(this->bInterruptLoad.load()) {  // cancellation point
                        // clean up partially loaded diffs in current set
                        for(DatabaseBeatmap *diff : *diffs) {
                            this->beatmap_difficulties.erase(diff->getMD5Hash());
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
                    diff->sMD5Hash = neosu_maps.read_hash();
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
                        neosu_maps.skip_bytes(sizeof(Database::TIMINGPOINT) * nb_timing_points);
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

                    if(SString::whitespace_only(diff->sTitleUnicode)) {
                        diff->bEmptyTitleUnicode = true;
                    }
                    if(SString::whitespace_only(diff->sArtistUnicode)) {
                        diff->bEmptyArtistUnicode = true;
                    }

                    this->beatmap_difficulties[diff->sMD5Hash] = diff;
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
                    this->beatmapsets.push_back(set);

                    // NOTE: Don't add neosu sets to beatmapSets since they're already processed
                    // Adding them would create duplicate ownership of the diffs vector
                }
            }

            if(version >= 20240812) {
                nb_overrides = neosu_maps.read<u32>();
                for(u32 i = 0; i < nb_overrides; i++) {
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
                    this->peppy_overrides[map_md5] = over;
                }
            }
        }
        this->bytes_processed += neosu_maps.total_size;
        this->neosu_maps_loaded = true;
    }

    if(!this->bNeedRawLoad) {
        ByteBufferedFile::Reader db(peppy_db_path);
        bool should_read_peppy_database = db.total_size > 0;
        if(should_read_peppy_database) {
            // read header
            this->iVersion = db.read<u32>();
            this->iFolderCount = db.read<u32>();
            db.skip<u8>();
            db.skip<u64>() /* timestamp */;
            auto playerName = db.read_string();
            this->iNumBeatmapsToLoad = db.read<u32>();

            debugLog("Database: version = {:d}, folderCount = {:d}, playerName = {:s}, numDiffs = {:d}", this->iVersion,
                     this->iFolderCount, playerName.c_str(), this->iNumBeatmapsToLoad);

            // hard cap upper db version
            if(this->iVersion > cv::database_version.getInt() && !cv::database_ignore_version.getBool()) {
                osu->getNotificationOverlay()->addToast(
                    UString::format("osu!.db version unknown (%i), osu!stable maps will not get loaded.",
                                    this->iVersion),
                    ERROR_TOAST);
                should_read_peppy_database = false;
            }
        }

        if(should_read_peppy_database) {
            zarray<BPMTuple> bpm_calculation_buffer;
            zarray<Database::TIMINGPOINT> timing_points_buffer;

            for(int i = 0; i < this->iNumBeatmapsToLoad; i++) {
                if(this->bInterruptLoad.load()) break;  // cancellation point

                if(cv::debug_db.getBool())
                    debugLog("Database: Reading beatmap {:d}/{:d} ...", (i + 1), this->iNumBeatmapsToLoad);

                // update progress (another thread checks if progress >= 1.f to know when we're done)
                u32 progress_bytes = this->bytes_processed + db.total_pos;
                f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
                this->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);

                // NOTE: This is documented wrongly in many places.
                //       This int was added in 20160408 and removed in 20191106
                //       https://osu.ppy.sh/home/changelog/stable40/20160408.3
                //       https://osu.ppy.sh/home/changelog/cuttingedge/20191106
                if(this->iVersion >= 20160408 && this->iVersion < 20191106) {
                    // size in bytes of the beatmap entry
                    db.skip<u32>();
                }

                std::string artistName = db.read_string();
                SString::trim(&artistName);
                std::string artistNameUnicode = db.read_string();
                std::string songTitle = db.read_string();
                SString::trim(&songTitle);
                std::string songTitleUnicode = db.read_string();
                std::string creatorName = db.read_string();
                SString::trim(&creatorName);
                std::string difficultyName = db.read_string();
                SString::trim(&difficultyName);
                std::string audioFileName = db.read_string();

                auto md5hash = db.read_hash();
                auto overrides = this->peppy_overrides.find(md5hash);
                bool overrides_found = overrides != this->peppy_overrides.end();

                std::string osuFileName = db.read_string();
                /*unsigned char rankedStatus = */ db.skip<u8>();
                auto numCircles = db.read<u16>();
                auto numSliders = db.read<u16>();
                auto numSpinners = db.read<u16>();
                long long lastModificationTime = db.read<u64>();

                f32 AR, CS, HP, OD;
                if(this->iVersion < 20140609) {
                    AR = db.read<u8>();
                    CS = db.read<u8>();
                    HP = db.read<u8>();
                    OD = db.read<u8>();
                } else {
                    AR = db.read<f32>();
                    CS = db.read<f32>();
                    HP = db.read<f32>();
                    OD = db.read<f32>();
                }

                auto sliderMultiplier = db.read<f64>();

                f32 nomod_star_rating = 0.0f;
                if(this->iVersion >= 20140609) {
                    auto numOsuStandardStarRatings = db.read<u32>();
                    for(int s = 0; s < numOsuStandardStarRatings; s++) {
                        db.skip<u8>();  // 0x08
                        auto mods = db.read<u32>();
                        db.skip<u8>();  // 0x0c

                        f32 sr = 0.f;

                        // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                        if(this->iVersion >= 20250108) {
                            sr = db.read<f32>();
                        } else {
                            sr = db.read<f64>();
                        }

                        if(mods == 0) nomod_star_rating = sr;
                    }

                    auto numTaikoStarRatings = db.read<u32>();
                    for(int s = 0; s < numTaikoStarRatings; s++) {
                        db.skip<u8>();  // 0x08
                        db.skip<u32>();
                        db.skip<u8>();  // 0x0c

                        // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                        if(this->iVersion >= 20250108) {
                            db.skip<f32>();
                        } else {
                            db.skip<f64>();
                        }
                    }

                    auto numCtbStarRatings = db.read<u32>();
                    for(int s = 0; s < numCtbStarRatings; s++) {
                        db.skip<u8>();  // 0x08
                        db.skip<u32>();
                        db.skip<u8>();  // 0x0c

                        // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                        if(this->iVersion >= 20250108) {
                            db.skip<f32>();
                        } else {
                            db.skip<f64>();
                        }
                    }

                    auto numManiaStarRatings = db.read<u32>();
                    for(int s = 0; s < numManiaStarRatings; s++) {
                        db.skip<u8>();  // 0x08
                        db.skip<u32>();
                        db.skip<u8>();  // 0x0c

                        // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                        if(this->iVersion >= 20250108) {
                            db.skip<f32>();
                        } else {
                            db.skip<f64>();
                        }
                    }
                }

                /*unsigned int drainTime = */ db.skip<u32>();  // seconds
                int duration = db.read<u32>();                 // milliseconds
                duration = duration >= 0 ? duration : 0;       // sanity clamp
                int previewTime = db.read<u32>();

                BPMInfo bpm;
                auto nb_timing_points = db.read<u32>();
                if(overrides_found) {
                    db.skip_bytes(sizeof(Database::TIMINGPOINT) * nb_timing_points);
                    bpm.min = overrides->second.min_bpm;
                    bpm.max = overrides->second.max_bpm;
                    bpm.most_common = overrides->second.avg_bpm;
                } else if(nb_timing_points > 0) {
                    timing_points_buffer.resize(nb_timing_points);
                    if(db.read_bytes((u8 *)timing_points_buffer.data(),
                                     sizeof(Database::TIMINGPOINT) * nb_timing_points) !=
                       sizeof(Database::TIMINGPOINT) * nb_timing_points) {
                        debugLog("WARNING: failed to read timing points from beatmap {:d} !", (i + 1));
                    }
                    bpm = getBPM(timing_points_buffer, bpm_calculation_buffer);
                }

                int beatmapID = db.read<i32>();  // fucking bullshit, this is NOT an unsigned integer as is described on
                                                 // the wiki, it can and is -1 sometimes
                int beatmapSetID = db.read<i32>();  // same here
                /*unsigned int threadID = */ db.skip<u32>();

                /*unsigned char osuStandardGrade = */ db.skip<u8>();
                /*unsigned char taikoGrade = */ db.skip<u8>();
                /*unsigned char ctbGrade = */ db.skip<u8>();
                /*unsigned char maniaGrade = */ db.skip<u8>();

                short localOffset = db.read<u16>();
                auto stackLeniency = db.read<f32>();
                auto mode = db.read<u8>();

                auto songSource = db.read_string();
                auto songTags = db.read_string();
                SString::trim(&songSource);
                SString::trim(&songTags);

                short onlineOffset = db.read<u16>();
                db.skip_string();  // song title font
                /*bool unplayed = */ db.skip<u8>();
                /*long long lastTimePlayed = */ db.skip<u64>();
                /*bool isOsz2 = */ db.skip<u8>();

                // somehow, some beatmaps may have spaces at the start/end of their
                // path, breaking the Windows API (e.g. https://osu.ppy.sh/s/215347)
                auto path = db.read_string();
                SString::trim(&path);

                /*long long lastOnlineCheck = */ db.skip<u64>();

                /*bool ignoreBeatmapSounds = */ db.skip<u8>();
                /*bool ignoreBeatmapSkin = */ db.skip<u8>();
                /*bool disableStoryboard = */ db.skip<u8>();
                /*bool disableVideo = */ db.skip<u8>();
                /*bool visualOverride = */ db.skip<u8>();

                if(this->iVersion < 20140609) {
                    // https://github.com/ppy/osu/wiki/Legacy-database-file-structure defines it as "Unknown"
                    db.skip<u16>();
                }

                /*int lastEditTime = */ db.skip<u32>();
                /*unsigned char maniaScrollSpeed = */ db.skip<u8>();

                // HACKHACK: workaround for linux and macos: it can happen that nested beatmaps are stored in the
                // database, and that osu! stores that filepath with a backslash (because windows)
                if constexpr(!Env::cfg(OS::WINDOWS)) {
                    for(int c = 0; c < path.length(); c++) {
                        if(path[c] == '\\') {
                            path[c] = '/';
                        }
                    }
                }

                // build beatmap & diffs from all the data
                std::string beatmapPath = songFolder;
                beatmapPath.append(path.c_str());
                beatmapPath.append("/");
                std::string fullFilePath = beatmapPath;
                fullFilePath.append(osuFileName);

                // skip invalid/corrupt entries
                // the good way would be to check if the .osu file actually exists on disk, but that is slow af, ain't
                // nobody got time for that so, since I've seen some concrete examples of what happens in such cases, we
                // just exclude those
                if(artistName.length() < 1 && songTitle.length() < 1 && creatorName.length() < 1 &&
                   difficultyName.length() < 1 && md5hash.hash[0] == 0)
                    continue;

                // fill diff with data
                if(mode != 0) continue;

                auto *map =
                    new DatabaseBeatmap(fullFilePath, beatmapPath, DatabaseBeatmap::BeatmapType::PEPPY_DIFFICULTY);
                {
                    map->sTitle = songTitle;
                    map->sTitleUnicode = songTitleUnicode;
                    if(SString::whitespace_only(map->sTitleUnicode)) {
                        map->bEmptyTitleUnicode = true;
                    }
                    map->sAudioFileName = audioFileName;
                    map->iLengthMS = duration;

                    map->fStackLeniency = stackLeniency;

                    map->sArtist = artistName;
                    map->sArtistUnicode = artistNameUnicode;
                    if(SString::whitespace_only(map->sArtistUnicode)) {
                        map->bEmptyArtistUnicode = true;
                    }
                    map->sCreator = creatorName;
                    map->sDifficultyName = difficultyName;
                    map->sSource = songSource;
                    map->sTags = songTags;
                    map->sMD5Hash = md5hash;
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

                // special case: legacy fallback behavior for invalid beatmapSetID, try to parse the ID from the path
                if(beatmapSetID < 1 && path.length() > 0) {
                    auto upath = UString(path.c_str());
                    const std::vector<UString> pathTokens =
                        upath.split("\\");  // NOTE: this is hardcoded to backslash since osu is windows only
                    if(pathTokens.size() > 0 && pathTokens[0].length() > 0) {
                        const std::vector<UString> spaceTokens = pathTokens[0].split(" ");
                        if(spaceTokens.size() > 0 && spaceTokens[0].length() > 0) {
                            beatmapSetID = spaceTokens[0].toInt();
                            if(beatmapSetID == 0) beatmapSetID = -1;
                        }
                    }
                }

                // (the diff is now fully built)
                this->beatmap_difficulties[md5hash] = map;

                // now, search if the current set (to which this diff would belong) already exists and add it there, or
                // if it doesn't exist then create the set
                const auto result = setIDToIndex.find(beatmapSetID);
                const bool beatmapSetExists = (result != setIDToIndex.end());
                bool diff_already_added = false;
                if(beatmapSetExists) {
                    for(const auto &existing_diff : *beatmapSets[result->second].diffs2) {
                        if(existing_diff->getMD5Hash() == map->getMD5Hash()) {
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
                        MapOverrides over = overrides->second;
                        map->iLocalOffset = over.local_offset;
                        map->iOnlineOffset = over.online_offset;
                        map->fStarsNomod = over.star_rating;
                        map->loudness = over.loudness;
                        map->draw_background = over.draw_background;

                        if(over.loudness != 0.f) {
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

            // build beatmap sets
            for(const auto &beatmapSet : beatmapSets) {
                if(this->bInterruptLoad.load()) {  // cancellation point
                    // clean up remaining unprocessed diffs2 vectors and their contents
                    for(size_t i = &beatmapSet - &beatmapSets[0]; i < beatmapSets.size(); i++) {
                        if(beatmapSets[i].diffs2) {
                            for(DatabaseBeatmap *diff : *beatmapSets[i].diffs2) {
                                this->beatmap_difficulties.erase(diff->getMD5Hash());

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
                    this->beatmapsets.push_back(set);
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
                        this->beatmapsets.push_back(set);
                    }

                    // clean up the original diffs2 vector (ownership of diffs transferred to new vectors)
                    delete beatmapSet.diffs2;
                }
            }
        }
        this->bytes_processed += db.total_size;
    }
    this->importTimer->update();
    debugLog("peppy+neosu maps: loading took {:f} seconds ({:d} peppy, {:d} neosu, {:d} maps total)",
             this->importTimer->getElapsedTime(), nb_peppy_maps, nb_neosu_maps, nb_peppy_maps + nb_neosu_maps);
    debugLog("Found {:d} overrides; {:d} maps need star recalc, {:d} maps need loudness recalc", nb_overrides,
             this->maps_to_recalc.size(), this->loudness_to_calc.size());

    this->peppy_overrides_mtx.unlock();
}

void Database::saveMaps() {
    debugLog("Osu: Saving maps ...");
    if(!this->neosu_maps_loaded) {
        debugLog("Cannot save maps since they weren't loaded properly first!");
        return;
    }

    Timer t;
    t.start();

    // collect neosu-only sets here
    std::vector<BeatmapSet *> temp_neosu_sets;
    for(const auto &beatmap : this->beatmapsets) {
        if(beatmap->type == DatabaseBeatmap::BeatmapType::NEOSU_BEATMAPSET) {
            temp_neosu_sets.push_back(beatmap);
        }
    }

    ByteBufferedFile::Writer maps("neosu_maps.db");
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
            maps.write_hash(diff->sMD5Hash);
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
            maps.write<f32>(diff->loudness.load());
            maps.write_string(diff->sTitleUnicode.c_str());
            maps.write_string(diff->sArtistUnicode.c_str());

            nb_diffs_saved++;
        }
    }

    // We want to save settings we applied on peppy-imported maps
    this->peppy_overrides_mtx.lock();

    // When calculating loudness we don't call update_overrides() for performance reasons
    for(const auto &map : this->loudness_to_calc) {
        if(map->type != DatabaseBeatmap::BeatmapType::PEPPY_DIFFICULTY) continue;
        if(map->loudness.load() == 0.f) continue;
        this->peppy_overrides[map->getMD5Hash()] = map->get_overrides();
    }

    u32 nb_overrides = 0;
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

        nb_overrides++;
    }
    this->peppy_overrides_mtx.unlock();

    t.update();
    debugLog("Saved {:d} maps (+ {:d} overrides) in {:f} seconds.", nb_diffs_saved, nb_overrides, t.getElapsedTime());
}

void Database::findDatabases() {
    this->bytes_processed = 0;
    this->total_bytes = 0;
    this->database_files.clear();

    std::string peppy_scores_path = cv::osu_folder.getString();
    peppy_scores_path.append("/scores.db");
    this->database_files.emplace(peppy_scores_path, peppy_scores_path);
    this->database_files.emplace("neosu_scores.db", "neosu_scores.db");
    this->database_files.emplace("scores.db", "scores.db");  // mcneosu database

    // ignore if explicitly disabled
    if(cv::database_enabled.getBool()) {
        std::string peppy_maps_path = cv::osu_folder.getString();
        peppy_maps_path.append("/osu!.db");
        this->database_files.emplace(peppy_maps_path, peppy_maps_path);
    }

    this->database_files.emplace("neosu_maps.db", "neosu_maps.db");

    std::string peppy_collections_path = cv::osu_folder.getString();
    peppy_collections_path.append("/collection.db");
    this->database_files.emplace(peppy_collections_path, peppy_collections_path);
    this->database_files.emplace("collections.db", "collections.db");

    for(const auto &db_path : this->dbPathsToImport) {
        this->database_files.emplace(db_path, db_path);
    }

    for(const auto &[_, pathstr] : this->database_files) {
        std::error_code ec;
        auto db_filesize = std::filesystem::file_size(File::getFsPath(pathstr), ec);
        if(!ec && db_filesize > 0) {
            this->total_bytes += db_filesize;
        }
    }
}

// Detects what type of database it is, then imports it
bool Database::importDatabase(std::string_view db_path) {
    std::string db_name = env->getFileNameFromFilePath(db_path);
    auto &db = this->database_files[std::string{db_path}];

    if(db_name == "collection.db") {
        // osu! collections
        return load_peppy_collections(db);
    }

    if(db_name == "collections.db") {
        // mcosu/neosu collections
        return load_mcneosu_collections(db);
    }

    if(db_name == "neosu_scores.db") {
        // neosu!
        this->loadScores(db);
        return true;
    }

    if(db_name == "scores.db") {
        ByteBufferedFile::Reader score_db{db_path};
        u32 db_version = score_db.read<u32>();
        if(!score_db.good() || db_version == 0) {
            return false;
        }

        if(db_version == 20210106 || db_version == 20210108 || db_version == 20210110) {
            // McOsu 100%!
            this->loadOldMcNeosuScores(db);
            return true;
        } else {
            // We need to do some heuristics to detect whether this is an old neosu or a peppy database.
            u32 nb_beatmaps = score_db.read<u32>();
            for(u32 i = 0; i < nb_beatmaps; i++) {
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
                        this->loadOldMcNeosuScores(db);
                        return true;
                    } else {
                        // peppy 100%!
                        this->loadPeppyScores(db);
                        return true;
                    }

                    // unreachable
                }
            }

            // 0 maps or 0 scores
            return false;
        }
    }

    return false;
}

void Database::loadScores(std::string_view dbPath) {
    ByteBufferedFile::Reader db(dbPath);
    if(db.total_size == 0) {
        this->bytes_processed += db.total_size;
        return;
    }

    // helper
    // to be removed from here when BanchoPacket is refactored to use ByteBufferedFile
    static auto read_mods = [](ByteBufferedFile::Reader &dbr) -> Replay::Mods {
        Replay::Mods mods;

        mods.flags = dbr.read<u64>();
        mods.speed = dbr.read<f32>();
        mods.notelock_type = dbr.read<i32>();
        mods.ar_override = dbr.read<f32>();
        mods.ar_overridenegative = dbr.read<f32>();
        mods.cs_override = dbr.read<f32>();
        mods.cs_overridenegative = dbr.read<f32>();
        mods.hp_override = dbr.read<f32>();
        mods.od_override = dbr.read<f32>();
        using namespace ModMasks;
        using namespace Replay::ModFlags;
        if(eq(mods.flags, Autopilot)) {
            mods.autopilot_lenience = dbr.read<f32>();
        }
        if(eq(mods.flags, Timewarp)) {
            mods.timewarp_multiplier = dbr.read<f32>();
        }
        if(eq(mods.flags, Minimize)) {
            mods.minimize_multiplier = dbr.read<f32>();
        }
        if(eq(mods.flags, ARTimewarp)) {
            mods.artimewarp_multiplier = dbr.read<f32>();
        }
        if(eq(mods.flags, ARWobble)) {
            mods.arwobble_strength = dbr.read<f32>();
            mods.arwobble_interval = dbr.read<f32>();
        }
        if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
            mods.wobble_strength = dbr.read<f32>();
            mods.wobble_frequency = dbr.read<f32>();
            mods.wobble_rotation_speed = dbr.read<f32>();
        }
        if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
            mods.jigsaw_followcircle_radius_factor = dbr.read<f32>();
        }
        if(eq(mods.flags, Shirone)) {
            mods.shirone_combo = dbr.read<f32>();
        }

        return mods;
    };

    u32 nb_neosu_scores = 0;
    u8 magic_bytes[6] = {0};
    if(db.read_bytes(magic_bytes, 5) != 5 || memcmp(magic_bytes, "NEOSC", 5) != 0) {
        osu->getNotificationOverlay()->addToast("Failed to load neosu_scores.db!", ERROR_TOAST);
        this->bytes_processed += db.total_size;
        return;
    }

    u32 db_version = db.read<u32>();
    if(db_version > NEOSU_SCORE_DB_VERSION) {
        debugLog("neosu_scores.db version is newer than current neosu version!");
        this->bytes_processed += db.total_size;
        return;
    } else if(db_version < NEOSU_SCORE_DB_VERSION) {
        // Reading from older database version: backup just in case
        auto backup_path = fmt::format("neosu_scores.db.{}", db_version);
        ByteBufferedFile::copy("neosu_scores.db", backup_path);
    }

    u32 nb_beatmaps = db.read<u32>();
    u32 nb_scores = db.read<u32>();
    this->scores.reserve(nb_beatmaps);

    for(u32 b = 0; b < nb_beatmaps; b++) {
        MD5Hash beatmap_hash = db.read_hash();
        u32 nb_beatmap_scores = db.read<u32>();

        for(u32 s = 0; s < nb_beatmap_scores; s++) {
            FinishedScore sc;

            sc.mods = read_mods(db);
            sc.score = db.read<u64>();
            sc.spinner_bonus = db.read<u64>();
            sc.unixTimestamp = db.read<u64>();
            sc.player_id = db.read<i32>();
            sc.playerName = db.read_string();
            sc.grade = (FinishedScore::Grade)db.read<u8>();

            sc.client = db.read_string();
            sc.server = db.read_string();
            sc.bancho_score_id = db.read<i64>();
            sc.peppy_replay_tms = db.read<u64>();

            sc.num300s = db.read<u16>();
            sc.num100s = db.read<u16>();
            sc.num50s = db.read<u16>();
            sc.numGekis = db.read<u16>();
            sc.numKatus = db.read<u16>();
            sc.numMisses = db.read<u16>();
            sc.comboMax = db.read<u16>();

            sc.ppv2_version = db.read<u32>();
            sc.ppv2_score = db.read<f32>();
            sc.ppv2_total_stars = db.read<f32>();
            sc.ppv2_aim_stars = db.read<f32>();
            sc.ppv2_speed_stars = db.read<f32>();

            sc.numSliderBreaks = db.read<u16>();
            sc.unstableRate = db.read<f32>();
            sc.hitErrorAvgMin = db.read<f32>();
            sc.hitErrorAvgMax = db.read<f32>();
            sc.maxPossibleCombo = db.read<u32>();
            sc.numHitObjects = db.read<u32>();
            sc.numCircles = db.read<u32>();

            sc.beatmap_hash = beatmap_hash;

            this->addScoreRaw(sc);
            nb_neosu_scores++;
        }

        u32 progress_bytes = this->bytes_processed + db.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
        this->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);
    }

    if(nb_neosu_scores != nb_scores) {
        debugLog("Inconsistency in neosu_scores.db! Expected {:d} scores, found {:d}!", nb_scores, nb_neosu_scores);
    }

    debugLog("Loaded {:d} neosu scores", nb_neosu_scores);
    this->bytes_processed += db.total_size;
}

// import scores from mcosu, or old neosu (before we started saving replays)
void Database::loadOldMcNeosuScores(std::string_view dbPath) {
    ByteBufferedFile::Reader db(dbPath);

    u32 db_version = db.read<u32>();
    if(db.total_size == 0 || db_version == 0) {
        this->bytes_processed += db.total_size;
        return;
    }

    u32 nb_imported = 0;
    bool is_mcosu = (db_version == 20210106 || db_version == 20210108 || db_version == 20210110);
    bool is_neosu = !is_mcosu;

    if(is_neosu) {
        u32 nb_beatmaps = db.read<u32>();
        for(u32 b = 0; b < nb_beatmaps; b++) {
            u32 progress_bytes = this->bytes_processed + db.total_pos;
            f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
            this->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);

            auto md5hash = db.read_hash();
            u32 nb_scores = db.read<u32>();

            for(u32 s = 0; s < nb_scores; s++) {
                db.skip<u8>();   // gamemode (always 0)
                db.skip<u32>();  // score version

                FinishedScore sc;
                sc.unixTimestamp = db.read<u64>();
                sc.playerName = db.read_string();
                sc.num300s = db.read<u16>();
                sc.num100s = db.read<u16>();
                sc.num50s = db.read<u16>();
                sc.numGekis = db.read<u16>();
                sc.numKatus = db.read<u16>();
                sc.numMisses = db.read<u16>();
                sc.score = db.read<u64>();
                sc.comboMax = db.read<u16>();
                sc.mods = Replay::Mods::from_legacy(db.read<u32>());
                sc.numSliderBreaks = db.read<u16>();
                sc.ppv2_version = 20220902;
                sc.ppv2_score = db.read<f32>();
                sc.unstableRate = db.read<f32>();
                sc.hitErrorAvgMin = db.read<f32>();
                sc.hitErrorAvgMax = db.read<f32>();
                sc.ppv2_total_stars = db.read<f32>();
                sc.ppv2_aim_stars = db.read<f32>();
                sc.ppv2_speed_stars = db.read<f32>();
                sc.mods.speed = db.read<f32>();
                sc.mods.cs_override = db.read<f32>();
                sc.mods.ar_override = db.read<f32>();
                sc.mods.od_override = db.read<f32>();
                sc.mods.hp_override = db.read<f32>();
                sc.maxPossibleCombo = db.read<u32>();
                sc.numHitObjects = db.read<u32>();
                sc.numCircles = db.read<u32>();
                sc.bancho_score_id = db.read<u32>();
                sc.client = "neosu-win64-release-35.10";  // we don't know the actual version
                sc.server = db.read_string();

                std::string experimentalModsConVars = db.read_string();
                auto experimentalMods = SString::split(experimentalModsConVars, ";");
                for(const auto &mod : experimentalMods) {
                    if(mod == "") continue;
                    if(mod == "fposu_mod_strafing") sc.mods.flags |= Replay::ModFlags::FPoSu_Strafing;
                    if(mod == "osu_mod_wobble") sc.mods.flags |= Replay::ModFlags::Wobble1;
                    if(mod == "osu_mod_wobble2") sc.mods.flags |= Replay::ModFlags::Wobble2;
                    if(mod == "osu_mod_arwobble") sc.mods.flags |= Replay::ModFlags::ARWobble;
                    if(mod == "osu_mod_timewarp") sc.mods.flags |= Replay::ModFlags::Timewarp;
                    if(mod == "osu_mod_artimewarp") sc.mods.flags |= Replay::ModFlags::ARTimewarp;
                    if(mod == "osu_mod_minimize") sc.mods.flags |= Replay::ModFlags::Minimize;
                    if(mod == "osu_mod_fadingcursor") sc.mods.flags |= Replay::ModFlags::FadingCursor;
                    if(mod == "osu_mod_fps") sc.mods.flags |= Replay::ModFlags::FPS;
                    if(mod == "osu_mod_jigsaw1") sc.mods.flags |= Replay::ModFlags::Jigsaw1;
                    if(mod == "osu_mod_jigsaw2") sc.mods.flags |= Replay::ModFlags::Jigsaw2;
                    if(mod == "osu_mod_fullalternate") sc.mods.flags |= Replay::ModFlags::FullAlternate;
                    if(mod == "osu_mod_reverse_sliders") sc.mods.flags |= Replay::ModFlags::ReverseSliders;
                    if(mod == "osu_mod_no50s") sc.mods.flags |= Replay::ModFlags::No50s;
                    if(mod == "osu_mod_no100s") sc.mods.flags |= Replay::ModFlags::No100s;
                    if(mod == "osu_mod_ming3012") sc.mods.flags |= Replay::ModFlags::Ming3012;
                    if(mod == "osu_mod_halfwindow") sc.mods.flags |= Replay::ModFlags::HalfWindow;
                    if(mod == "osu_mod_millhioref") sc.mods.flags |= Replay::ModFlags::Millhioref;
                    if(mod == "osu_mod_mafham") sc.mods.flags |= Replay::ModFlags::Mafham;
                    if(mod == "osu_mod_strict_tracking") sc.mods.flags |= Replay::ModFlags::StrictTracking;
                    if(mod == "osu_playfield_mirror_horizontal") sc.mods.flags |= Replay::ModFlags::MirrorHorizontal;
                    if(mod == "osu_playfield_mirror_vertical") sc.mods.flags |= Replay::ModFlags::MirrorVertical;
                    if(mod == "osu_mod_shirone") sc.mods.flags |= Replay::ModFlags::Shirone;
                    if(mod == "osu_mod_approach_different") sc.mods.flags |= Replay::ModFlags::ApproachDifferent;
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
        const int numBeatmaps = db.read<int32_t>();
        debugLog("McOsu scores: version = {}, numBeatmaps = {}", db_version, numBeatmaps);

        for(int b = 0; b < numBeatmaps; b++) {
            u32 progress_bytes = this->bytes_processed + db.total_pos;
            f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
            this->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);

            const auto md5hash = db.read_hash();
            const int numScores = db.read<int32_t>();

            if(md5hash.length() < 32) {
                debugLog("WARNING: Invalid score on beatmap {} with md5hash.length() = {}!", b, md5hash.length());
                continue;
            } else if(md5hash.length() > 32) {
                debugLog("ERROR: Corrupt score database/entry detected, stopping.");
                break;
            }

            if(cv::debug_db.getBool())
                debugLog("Beatmap[{}]: md5hash = {:s}, numScores = {}", b, md5hash.string(), numScores);

            for(int s = 0; s < numScores; s++) {
                const auto gamemode = db.read<uint8_t>();  // NOTE: abused as isImportedLegacyScore flag (because I
                                                           // forgot to add a version cap to old builds)
                const int scoreVersion = db.read<int32_t>();
                if(db_version == 20210103 && scoreVersion > 20190103) {
                    /* isImportedLegacyScore = */ db.skip<uint8_t>();  // too lazy to handle this logic
                }
                const auto unixTimestamp = db.read<uint64_t>();
                if(this->isScoreAlreadyInDB(unixTimestamp, md5hash) >= 0) {
                    db.skip_string();  // playerName
                    u32 bytesToSkipUntilNextScore = 0;
                    bytesToSkipUntilNextScore +=
                        (sizeof(uint16_t) * 8) + (sizeof(int64_t)) + (sizeof(int32_t)) + (sizeof(f32) * 12);
                    if(scoreVersion > 20180722) {
                        // maxPossibleCombos
                        bytesToSkipUntilNextScore += sizeof(int32_t) * 3;
                    }
                    db.skip_bytes(bytesToSkipUntilNextScore);
                    db.skip_string();  // experimentalMods
                    if(cv::debug_db.getBool()) {
                        debugLog("skipped score {} (already loaded from neosu_scores.db)", md5hash.string());
                    }
                    continue;
                }

                // default
                const std::string playerName{db.read_string()};

                const auto num300s = db.read<uint16_t>();
                const auto num100s = db.read<uint16_t>();
                const auto num50s = db.read<uint16_t>();
                const auto numGekis = db.read<uint16_t>();
                const auto numKatus = db.read<uint16_t>();
                const auto numMisses = db.read<uint16_t>();

                const auto score = db.read<int64_t>();
                const auto maxCombo = db.read<uint16_t>();
                const auto mods = Replay::Mods::from_legacy(db.read<uint32_t>());

                // custom
                const auto numSliderBreaks = db.read<uint16_t>();
                const auto pp = db.read<f32>();
                const auto unstableRate = db.read<f32>();
                const auto hitErrorAvgMin = db.read<f32>();
                const auto hitErrorAvgMax = db.read<f32>();
                const auto starsTomTotal = db.read<f32>();
                const auto starsTomAim = db.read<f32>();
                const auto starsTomSpeed = db.read<f32>();
                const auto speedMultiplier = db.read<f32>();
                const auto CS = db.read<f32>();
                const auto AR = db.read<f32>();
                const auto OD = db.read<f32>();
                const auto HP = db.read<f32>();

                int maxPossibleCombo = -1;
                int numHitObjects = -1;
                int numCircles = -1;
                if(scoreVersion > 20180722) {
                    maxPossibleCombo = db.read<int32_t>();
                    numHitObjects = db.read<int32_t>();
                    numCircles = db.read<int32_t>();
                }

                std::string experimentalModsConVars = db.read_string();
                auto experimentalMods = SString::split(experimentalModsConVars, ";");

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
                    for(const auto &mod : experimentalMods) {
                        if(mod == "") continue;
                        if(mod == "fposu_mod_strafing") sc.mods.flags |= Replay::ModFlags::FPoSu_Strafing;
                        if(mod == "osu_mod_wobble") sc.mods.flags |= Replay::ModFlags::Wobble1;
                        if(mod == "osu_mod_wobble2") sc.mods.flags |= Replay::ModFlags::Wobble2;
                        if(mod == "osu_mod_arwobble") sc.mods.flags |= Replay::ModFlags::ARWobble;
                        if(mod == "osu_mod_timewarp") sc.mods.flags |= Replay::ModFlags::Timewarp;
                        if(mod == "osu_mod_artimewarp") sc.mods.flags |= Replay::ModFlags::ARTimewarp;
                        if(mod == "osu_mod_minimize") sc.mods.flags |= Replay::ModFlags::Minimize;
                        if(mod == "osu_mod_fadingcursor") sc.mods.flags |= Replay::ModFlags::FadingCursor;
                        if(mod == "osu_mod_fps") sc.mods.flags |= Replay::ModFlags::FPS;
                        if(mod == "osu_mod_jigsaw1") sc.mods.flags |= Replay::ModFlags::Jigsaw1;
                        if(mod == "osu_mod_jigsaw2") sc.mods.flags |= Replay::ModFlags::Jigsaw2;
                        if(mod == "osu_mod_fullalternate") sc.mods.flags |= Replay::ModFlags::FullAlternate;
                        if(mod == "osu_mod_reverse_sliders") sc.mods.flags |= Replay::ModFlags::ReverseSliders;
                        if(mod == "osu_mod_no50s") sc.mods.flags |= Replay::ModFlags::No50s;
                        if(mod == "osu_mod_no100s") sc.mods.flags |= Replay::ModFlags::No100s;
                        if(mod == "osu_mod_ming3012") sc.mods.flags |= Replay::ModFlags::Ming3012;
                        if(mod == "osu_mod_halfwindow") sc.mods.flags |= Replay::ModFlags::HalfWindow;
                        if(mod == "osu_mod_millhioref") sc.mods.flags |= Replay::ModFlags::Millhioref;
                        if(mod == "osu_mod_mafham") sc.mods.flags |= Replay::ModFlags::Mafham;
                        if(mod == "osu_mod_strict_tracking") sc.mods.flags |= Replay::ModFlags::StrictTracking;
                        if(mod == "osu_playfield_mirror_horizontal")
                            sc.mods.flags |= Replay::ModFlags::MirrorHorizontal;
                        if(mod == "osu_playfield_mirror_vertical") sc.mods.flags |= Replay::ModFlags::MirrorVertical;
                        if(mod == "osu_mod_shirone") sc.mods.flags |= Replay::ModFlags::Shirone;
                        if(mod == "osu_mod_approach_different") sc.mods.flags |= Replay::ModFlags::ApproachDifferent;
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

    this->bytes_processed += db.total_size;
}

void Database::loadPeppyScores(std::string_view dbPath) {
    ByteBufferedFile::Reader db(dbPath);
    int nb_imported = 0;

    u32 db_version = db.read<u32>();
    u32 nb_beatmaps = db.read<u32>();
    if(db.total_size == 0 || db_version == 0) {
        this->bytes_processed += db.total_size;
        return;
    }

    debugLog("osu!stable scores.db: version = {:d}, nb_beatmaps = {:d}", db_version, nb_beatmaps);

    char client_str[15] = "peppy-YYYYMMDD";
    for(int b = 0; b < nb_beatmaps; b++) {
        std::string md5hash_str = db.read_string();
        if(md5hash_str.length() < 32) {
            debugLog("WARNING: Invalid score on beatmap {:d} with md5hash_str.length() = {:d}!", b,
                     md5hash_str.length());
            continue;
        } else if(md5hash_str.length() > 32) {
            debugLog("ERROR: Corrupt score database/entry detected, stopping.");
            break;
        }

        MD5Hash md5hash;

        memcpy(md5hash.string(), md5hash_str.c_str(), 32);
        md5hash.hash[32] = '\0';
        u32 nb_scores = db.read<u32>();

        for(int s = 0; s < nb_scores; s++) {
            FinishedScore sc;

            u8 gamemode = db.read<u8>();

            u32 score_version = db.read<u32>();
            snprintf(client_str, 14, "peppy-%d", score_version);
            sc.client = client_str;

            sc.server = "ppy.sh";
            db.skip_string();  // beatmap hash (already have it)
            sc.playerName = db.read_string();
            db.skip_string();  // replay hash (unused)

            sc.num300s = db.read<u16>();
            sc.num100s = db.read<u16>();
            sc.num50s = db.read<u16>();
            sc.numGekis = db.read<u16>();
            sc.numKatus = db.read<u16>();
            sc.numMisses = db.read<u16>();

            i32 score = db.read<i32>();
            sc.score = (score < 0 ? 0 : score);

            sc.comboMax = db.read<u16>();
            sc.perfect = db.read<u8>();
            sc.mods = Replay::Mods::from_legacy(db.read<u32>());

            db.skip_string();  // hp graph

            u64 full_tms = db.read<u64>();
            sc.unixTimestamp = (full_tms - 621355968000000000) / 10000000;
            sc.peppy_replay_tms = full_tms - 504911232000000000;

            // Always -1, but let's skip it properly just in case
            i32 old_replay_size = db.read<i32>();
            if(old_replay_size > 0) {
                db.skip_bytes(old_replay_size);
            }

            if(score_version >= 20131110) {
                sc.bancho_score_id = db.read<i64>();
            } else if(score_version >= 20121008) {
                sc.bancho_score_id = db.read<i32>();
            } else {
                sc.bancho_score_id = 0;
            }

            if(ModMasks::eq(sc.mods.flags, Replay::ModFlags::Target)) {
                db.skip<f64>();  // total accuracy
            }

            if(gamemode == 0 && sc.bancho_score_id != 0) {
                sc.beatmap_hash = md5hash;
                sc.grade = sc.calculate_grade();

                if(this->addScoreRaw(sc)) {
                    nb_imported++;
                }
            }
        }

        u32 progress_bytes = this->bytes_processed + db.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)this->total_bytes;
        this->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);
    }

    debugLog("Loaded {:d} osu!stable scores", nb_imported);
    this->bytes_processed += db.total_size;
}

void Database::saveScores() {
    debugLog("Osu: Saving scores ...");
    if(!this->bScoresLoaded) {
        debugLog("Cannot save scores since they weren't loaded properly first!");
        return;
    }

    // helper
    // to be removed from here when BanchoPacket is refactored to use ByteBufferedFile
    static auto write_mods = [](ByteBufferedFile::Writer &dbr, const Replay::Mods &mods) -> void {
        if(!dbr.good()) {
            return;
        }

        dbr.write<u64>(mods.flags);
        dbr.write<f32>(mods.speed);
        dbr.write<i32>(mods.notelock_type);
        dbr.write<f32>(mods.ar_override);
        dbr.write<f32>(mods.ar_overridenegative);
        dbr.write<f32>(mods.cs_override);
        dbr.write<f32>(mods.cs_overridenegative);
        dbr.write<f32>(mods.hp_override);
        dbr.write<f32>(mods.od_override);
        using namespace ModMasks;
        using namespace Replay::ModFlags;
        if(eq(mods.flags, Autopilot)) {
            dbr.write<f32>(mods.autopilot_lenience);
        }
        if(eq(mods.flags, Timewarp)) {
            dbr.write<f32>(mods.timewarp_multiplier);
        }
        if(eq(mods.flags, Minimize)) {
            dbr.write<f32>(mods.minimize_multiplier);
        }
        if(eq(mods.flags, ARTimewarp)) {
            dbr.write<f32>(mods.artimewarp_multiplier);
        }
        if(eq(mods.flags, ARWobble)) {
            dbr.write<f32>(mods.arwobble_strength);
            dbr.write<f32>(mods.arwobble_interval);
        }
        if(eq(mods.flags, Wobble1) || eq(mods.flags, Wobble2)) {
            dbr.write<f32>(mods.wobble_strength);
            dbr.write<f32>(mods.wobble_frequency);
            dbr.write<f32>(mods.wobble_rotation_speed);
        }
        if(eq(mods.flags, Jigsaw1) || eq(mods.flags, Jigsaw2)) {
            dbr.write<f32>(mods.jigsaw_followcircle_radius_factor);
        }
        if(eq(mods.flags, Shirone)) {
            dbr.write<f32>(mods.shirone_combo);
        }
    };

    const double startTime = Timing::getTimeReal();

    Sync::scoped_lock lock(this->scores_mtx);
    ByteBufferedFile::Writer db("neosu_scores.db");
    db.write_bytes((u8 *)"NEOSC", 5);
    db.write<u32>(NEOSU_SCORE_DB_VERSION);

    u32 nb_beatmaps = 0;
    u32 nb_scores = 0;
    for(const auto &[_, scorevec] : this->scores) {
        u32 beatmap_scores = scorevec.size();
        if(beatmap_scores > 0) {
            nb_beatmaps++;
            nb_scores += beatmap_scores;
        }
    }
    db.write<u32>(nb_beatmaps);
    db.write<u32>(nb_scores);

    for(const auto &[hash, scorevec] : this->scores) {
        if(scorevec.empty()) continue;

        db.write_hash(hash);
        db.write<u32>(scorevec.size());

        for(const auto &score : scorevec) {
            assert(!score.is_online_score);

            write_mods(db, score.mods);
            db.write<u64>(score.score);
            db.write<u64>(score.spinner_bonus);
            db.write<u64>(score.unixTimestamp);
            db.write<i32>(score.player_id);
            db.write_string(score.playerName);
            db.write<u8>((u8)score.grade);

            db.write_string(score.client);
            db.write_string(score.server);
            db.write<i64>(score.bancho_score_id);
            db.write<u64>(score.peppy_replay_tms);

            db.write<u16>(score.num300s);
            db.write<u16>(score.num100s);
            db.write<u16>(score.num50s);
            db.write<u16>(score.numGekis);
            db.write<u16>(score.numKatus);
            db.write<u16>(score.numMisses);
            db.write<u16>(score.comboMax);

            db.write<u32>(score.ppv2_version);
            db.write<f32>(score.ppv2_score);
            db.write<f32>(score.ppv2_total_stars);
            db.write<f32>(score.ppv2_aim_stars);
            db.write<f32>(score.ppv2_speed_stars);

            db.write<u16>(score.numSliderBreaks);
            db.write<f32>(score.unstableRate);
            db.write<f32>(score.hitErrorAvgMin);
            db.write<f32>(score.hitErrorAvgMax);
            db.write<u32>(score.maxPossibleCombo);
            db.write<u32>(score.numHitObjects);
            db.write<u32>(score.numCircles);
        }
    }

    debugLog("Saved {:d} scores in {:f} seconds.", nb_scores, (Timing::getTimeReal() - startTime));
}

BeatmapSet *Database::loadRawBeatmap(const std::string &beatmapPath) {
    if(cv::debug_db.getBool()) debugLog("BeatmapDatabase::loadRawBeatmap() : {:s}", beatmapPath.c_str());

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
            if(cv::debug_db.getBool()) {
                debugLog("BeatmapDatabase::loadRawBeatmap() : Couldn't loadMetadata(), deleting object.");
            }
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
