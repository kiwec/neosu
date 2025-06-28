#include "Database.h"

#include <string.h>

#include <fstream>

#include "Bancho.h"  // md5
#include "BanchoFile.h"
#include "BanchoNetworking.h"
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
#include "score.h"

Database *db = NULL;

struct SortScoreByScore : public Database::SCORE_SORTING_COMPARATOR {
    ~SortScoreByScore() override { ; }
    bool operator()(FinishedScore const &a, FinishedScore const &b) const override {
        if(a.score != b.score) return a.score > b.score;
        if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
        return &a > &b;
    }
};

struct SortScoreByCombo : public Database::SCORE_SORTING_COMPARATOR {
    ~SortScoreByCombo() override { ; }
    bool operator()(FinishedScore const &a, FinishedScore const &b) const override {
        if(a.comboMax != b.comboMax) return a.comboMax > b.comboMax;
        if(a.score != b.score) return a.score > b.score;
        if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
        return &a > &b;
    }
};

struct SortScoreByDate : public Database::SCORE_SORTING_COMPARATOR {
    ~SortScoreByDate() override { ; }
    bool operator()(FinishedScore const &a, FinishedScore const &b) const override {
        if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
        return &a > &b;
    }
};

struct SortScoreByMisses : public Database::SCORE_SORTING_COMPARATOR {
    ~SortScoreByMisses() override { ; }
    bool operator()(FinishedScore const &a, FinishedScore const &b) const override {
        if(a.numMisses != b.numMisses) return a.numMisses < b.numMisses;
        if(a.score != b.score) return a.score > b.score;
        if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
        return &a > &b;
    }
};

struct SortScoreByAccuracy : public Database::SCORE_SORTING_COMPARATOR {
    ~SortScoreByAccuracy() override { ; }
    bool operator()(FinishedScore const &a, FinishedScore const &b) const override {
        auto a_acc = LiveScore::calculateAccuracy(a.num300s, a.num100s, a.num50s, a.numMisses);
        auto b_acc = LiveScore::calculateAccuracy(b.num300s, b.num100s, b.num50s, b.numMisses);
        if(a_acc != b_acc) return a_acc > b_acc;
        if(a.score != b.score) return a.score > b.score;
        if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
        return &a > &b;
    }
};

struct SortScoreByPP : public Database::SCORE_SORTING_COMPARATOR {
    ~SortScoreByPP() override { ; }
    bool operator()(FinishedScore const &a, FinishedScore const &b) const override {
        auto a_pp = max(a.get_pp() * 1000.0, 0.0);
        auto b_pp = max(b.get_pp() * 1000.0, 0.0);
        if(a_pp != b_pp) return a_pp > b_pp;
        if(a.score != b.score) return a.score > b.score;
        if(a.unixTimestamp != b.unixTimestamp) return a.unixTimestamp > b.unixTimestamp;
        return &a > &b;
    }
};

class DatabaseLoader : public Resource {
   public:
    DatabaseLoader(Database *db) : Resource() {
        this->db = db;

        this->bAsyncReady = false;
        this->bReady = false;
    };

    [[nodiscard]] Type getResType() const override { return APPDEFINED; } // TODO: handle this better?

   protected:
    void init() override {
        this->bReady = true;

        delete this;  // commit sudoku
    }

    void initAsync() override {
        debugLog("DatabaseLoader::initAsync()\n");

        // load scores
        sct_abort();
        this->db->loadScores();

        // load database
        lct_set_map(NULL);
        loct_abort();
        MapCalcThread::abort();
        this->db->beatmapsets.clear();  // TODO @kiwec: this just leaks memory?
        this->db->loadDB();

        this->bAsyncReady = true;
    }

    void destroy() override { ; }

   private:
    Database *db;
};

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

    this->bDidScoresChangeForStats = true;

    this->prevPlayerStats.pp = 0.0f;
    this->prevPlayerStats.accuracy = 0.0f;
    this->prevPlayerStats.numScoresWithPP = 0;
    this->prevPlayerStats.level = 0;
    this->prevPlayerStats.percentToNextLevel = 0.0f;
    this->prevPlayerStats.totalScore = 0;

    this->scoreSortingMethods.push_back({"Sort by accuracy", new SortScoreByAccuracy()});
    this->scoreSortingMethods.push_back({"Sort by combo", new SortScoreByCombo()});
    this->scoreSortingMethods.push_back({"Sort by date", new SortScoreByDate()});
    this->scoreSortingMethods.push_back({"Sort by misses", new SortScoreByMisses()});
    this->scoreSortingMethods.push_back({"Sort by score", new SortScoreByScore()});
    this->scoreSortingMethods.push_back({"Sort by pp", new SortScoreByPP()});
}

Database::~Database() {
    SAFE_DELETE(this->importTimer);

    sct_abort();
    lct_set_map(NULL);
    loct_abort();
    MapCalcThread::abort();
    for(int i = 0; i < this->beatmapsets.size(); i++) {
        delete this->beatmapsets[i];
    }

    for(int i = 0; i < this->scoreSortingMethods.size(); i++) {
        delete this->scoreSortingMethods[i].comparator;
    }

    unload_collections();
}

void Database::load() {
    this->bInterruptLoad = false;
    this->fLoadingProgress = 0.0f;

    DatabaseLoader *loader = new DatabaseLoader(this);  // (deletes itself after finishing)

    resourceManager->requestNextLoadAsync();
    resourceManager->loadResource(loader);
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

BeatmapSet *Database::addBeatmapSet(std::string beatmapFolderPath, i32 set_id_override) {
    BeatmapSet *beatmap = this->loadRawBeatmap(beatmapFolderPath);
    if(beatmap == NULL) return NULL;

    // Some beatmaps don't provide beatmap/beatmapset IDs in the .osu files
    // But we know the beatmapset ID because we just downloaded it!
    if(set_id_override != -1) {
        beatmap->iSetID = set_id_override;
        for(auto &diff : beatmap->getDifficulties()) {
            diff->iSetID = set_id_override;
        }
    }

    this->beatmapsets.push_back(beatmap);
    this->neosu_sets.push_back(beatmap);

    this->beatmap_difficulties_mtx.lock();
    for(auto diff : beatmap->getDifficulties()) {
        this->beatmap_difficulties[diff->getMD5Hash()] = diff;
    }
    this->beatmap_difficulties_mtx.unlock();

    osu->songBrowser2->addBeatmapSet(beatmap);

    // XXX: Very slow
    osu->songBrowser2->onSortChangeInt(cv_songbrowser_sortingtype.getString(), false);

    return beatmap;
}

int Database::addScore(FinishedScore score) {
    this->addScoreRaw(score);
    this->sortScores(score.beatmap_hash);

    this->bDidScoresChangeForStats = true;

    if(cv_scores_save_immediately.getBool()) this->saveScores();

    // @PPV3: use new replay format

    // XXX: this is blocking main thread
    u8 *compressed_replay = NULL;
    size_t s_compressed_replay = 0;
    LegacyReplay::compress_frames(score.replay, &compressed_replay, &s_compressed_replay);
    if(s_compressed_replay > 0) {
        auto replay_path = UString::format(MCENGINE_DATA_DIR "replays/%d.replay.lzma", score.unixTimestamp);
        FILE *replay_file = fopen(replay_path.toUtf8(), "wb");
        if(replay_file != NULL) {
            fwrite(compressed_replay, s_compressed_replay, 1, replay_file);
            fclose(replay_file);
        }
    }

    // return sorted index
    std::lock_guard<std::mutex> lock(this->scores_mtx);
    for(int i = 0; i < this->scores[score.beatmap_hash].size(); i++) {
        if(this->scores[score.beatmap_hash][i].unixTimestamp == score.unixTimestamp) return i;
    }

    return -1;
}

bool Database::addScoreRaw(const FinishedScore &score) {
    std::lock_guard<std::mutex> lock(this->scores_mtx);
    for(auto other : this->scores[score.beatmap_hash]) {
        if(other.unixTimestamp == score.unixTimestamp) {
            // Score has already been added
            return false;
        }
    }

    this->scores[score.beatmap_hash].push_back(score);
    if(score.hitdeltas.empty()) {
        this->scores_to_convert.push_back(score);
    }

    return true;
}

void Database::deleteScore(MD5Hash beatmapMD5Hash, u64 scoreUnixTimestamp) {
    std::lock_guard<std::mutex> lock(this->scores_mtx);
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

    if(cv_songbrowser_scores_sortingtype.getString() == UString("Online Leaderboard")) {
        // Online scores are already sorted
        return;
    }

    for(int i = 0; i < this->scoreSortingMethods.size(); i++) {
        if(cv_songbrowser_scores_sortingtype.getString() == this->scoreSortingMethods[i].name) {
            struct COMPARATOR_WRAPPER {
                SCORE_SORTING_COMPARATOR *comp;
                bool operator()(FinishedScore const &a, FinishedScore const &b) const {
                    return this->comp->operator()(a, b);
                }
            };
            COMPARATOR_WRAPPER comparatorWrapper;
            comparatorWrapper.comp = this->scoreSortingMethods[i].comparator;

            std::sort(scores.begin(), scores.end(), comparatorWrapper);
            return;
        }
    }

    if(cv_debug.getBool()) {
        debugLog("ERROR: Invalid score sortingtype \"%s\"\n", cv_songbrowser_scores_sortingtype.getString().toUtf8());
    }
}

void Database::sortScores(MD5Hash beatmapMD5Hash) {
    std::lock_guard<std::mutex> lock(this->scores_mtx);
    this->sortScoresInPlace(this->scores[beatmapMD5Hash]);
}

std::vector<UString> Database::getPlayerNamesWithPPScores() {
    std::lock_guard<std::mutex> lock(this->scores_mtx);
    std::vector<MD5Hash> keys;
    keys.reserve(this->scores.size());

    for(auto kv : this->scores) {
        keys.push_back(kv.first);
    }

    std::unordered_set<std::string> tempNames;
    for(auto &key : keys) {
        for(auto &score : this->scores[key]) {
            tempNames.insert(score.playerName);
        }
    }

    // always add local user, even if there were no scores
    auto local_name = cv_name.getString();
    tempNames.insert(std::string(local_name.toUtf8()));

    std::vector<UString> names;
    names.reserve(tempNames.size());
    for(auto k : tempNames) {
        if(k.length() > 0) names.emplace_back(k.c_str());
    }

    return names;
}

std::vector<UString> Database::getPlayerNamesWithScoresForUserSwitcher() {
    std::lock_guard<std::mutex> lock(this->scores_mtx);
    std::unordered_set<std::string> tempNames;
    for(auto kv : this->scores) {
        const MD5Hash &key = kv.first;
        for(auto &score : this->scores[key]) {
            tempNames.insert(score.playerName);
        }
    }

    // always add local user, even if there were no scores
    auto local_name = cv_name.getString();
    tempNames.insert(std::string(local_name.toUtf8()));

    std::vector<UString> names;
    names.reserve(tempNames.size());
    for(auto k : tempNames) {
        if(k.length() > 0) names.emplace_back(k.c_str());
    }

    return names;
}

Database::PlayerPPScores Database::getPlayerPPScores(UString playerName) {
    std::lock_guard<std::mutex> lock(this->scores_mtx);
    PlayerPPScores ppScores;
    ppScores.totalScore = 0;
    if(this->getProgress() < 1.0f) return ppScores;

    std::vector<FinishedScore *> scores;

    // collect all scores with pp data
    std::vector<MD5Hash> keys;
    keys.reserve(this->scores.size());

    for(auto kv : this->scores) {
        keys.push_back(kv.first);
    }

    struct ScoreSortComparator {
        bool operator()(FinishedScore const *a, FinishedScore const *b) const {
            auto ppa = a->get_pp();
            auto ppb = b->get_pp();
            if(ppa != ppb) return ppa < ppb;
            return a < b;
        }
    };

    unsigned long long totalScore = 0;
    for(auto &key : keys) {
        if(this->scores[key].size() == 0) continue;

        FinishedScore *tempScore = &this->scores[key][0];

        // only add highest pp score per diff
        bool foundValidScore = false;
        float prevPP = -1.0f;
        for(auto &score : this->scores[key]) {
            UString uName = UString(score.playerName.c_str());

            auto uses_rx_or_ap =
                (score.mods.flags & Replay::ModFlags::Relax) || (score.mods.flags & Replay::ModFlags::Autopilot);
            if(uses_rx_or_ap && !cv_user_include_relax_and_autopilot_for_stats.getBool()) continue;

            if(uName != playerName) continue;

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
    std::sort(scores.begin(), scores.end(), ScoreSortComparator());

    ppScores.ppScores = std::move(scores);
    ppScores.totalScore = totalScore;

    return ppScores;
}

Database::PlayerStats Database::calculatePlayerStats(UString playerName) {
    if(!this->bDidScoresChangeForStats && playerName == this->prevPlayerStats.name) return this->prevPlayerStats;

    const PlayerPPScores ps = this->getPlayerPPScores(playerName);

    // delay caching until we actually have scores loaded
    if(ps.ppScores.size() > 0) this->bDidScoresChangeForStats = false;

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
    if(cv_scores_bonus_pp.getBool()) pp += getBonusPPForNumScores(ps.ppScores.size());

    // normalize accuracy
    if(ps.ppScores.size() > 0) acc /= (20.0f * (1.0f - getWeightForIndex(ps.ppScores.size())));

    // fill stats
    this->prevPlayerStats.name = playerName;
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

float Database::getWeightForIndex(int i) { return pow(0.95, (double)i); }

float Database::getBonusPPForNumScores(size_t numScores) {
    return (417.0 - 1.0 / 3.0) * (1.0 - pow(0.995, min(1000.0, (f64)numScores)));
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
    if(this->isLoading()) return NULL;

    std::lock_guard<std::mutex> lock(this->beatmap_difficulties_mtx);
    auto it = this->beatmap_difficulties.find(md5hash);
    if(it == this->beatmap_difficulties.end()) {
        return NULL;
    } else {
        return it->second;
    }
}

DatabaseBeatmap *Database::getBeatmapDifficulty(i32 map_id) {
    if(this->isLoading()) return NULL;

    std::lock_guard<std::mutex> lock(this->beatmap_difficulties_mtx);
    for(auto pair : this->beatmap_difficulties) {
        if(pair.second->getID() == map_id) {
            return pair.second;
        }
    }

    return NULL;
}

DatabaseBeatmap *Database::getBeatmapSet(i32 set_id) {
    if(this->isLoading()) return NULL;

    for(size_t i = 0; i < this->beatmapsets.size(); i++) {
        DatabaseBeatmap *beatmap = this->beatmapsets[i];
        if(beatmap->getSetID() == set_id) {
            return beatmap;
        }
    }

    return NULL;
}

std::string Database::getOsuSongsFolder() {
    std::string songs_folder;

    if(cv_songs_folder.getString().findChar(':') == -1) {
        // Relative path (yes, the check is Windows-only)
        songs_folder = cv_osu_folder.getString().toUtf8();
        songs_folder.append("/");
        songs_folder.append(cv_songs_folder.getString().toUtf8());
    } else {
        // Absolute path
        songs_folder = cv_songs_folder.getString().toUtf8();
    }

    songs_folder.append("/");
    return songs_folder;
}

void Database::loadDB() {
    std::lock_guard<std::mutex> lock(this->beatmap_difficulties_mtx);
    this->peppy_overrides_mtx.lock();

    std::string osuDbFilePath = cv_osu_folder.getString().toUtf8();
    osuDbFilePath.append("/osu!.db");
    BanchoFileReader db(osuDbFilePath.c_str());
    BanchoFileReader neosu_maps("neosu_maps.db");

    u32 db_size_sum = neosu_maps.total_size + db.total_size;

    std::string songFolder = this->getOsuSongsFolder();
    debugLog("Database: songFolder = %s\n", songFolder.c_str());

    this->importTimer->start();

    u32 nb_neosu_maps = 0;
    u32 nb_peppy_maps = 0;
    u32 nb_overrides = 0;

    // read beatmapInfos, and also build two hashmaps (diff hash -> BeatmapDifficulty, diff hash -> Beatmap)
    struct Beatmap_Set {
        int setID;
        std::vector<DatabaseBeatmap *> *diffs2 = NULL;
    };
    std::vector<Beatmap_Set> beatmapSets;
    std::unordered_map<int, size_t> setIDToIndex;

    // Load neosu map database
    // We don't want to reload it, ever. Would cause too much jank.
    static bool first_load = true;
    if(first_load && neosu_maps.total_size > 0) {
        first_load = false;

        u32 version = neosu_maps.read<u32>();
        if(version < NEOSU_MAPS_DB_VERSION) {
            // Reading from older database version: backup just in case
            auto backup_path = UString::format("neosu_maps.db.%d", version);
            copy("neosu_maps.db", backup_path.toUtf8());
        }

        u32 nb_sets = neosu_maps.read<u32>();
        for(u32 i = 0; i < nb_sets; i++) {
            u32 db_pos_sum = db.total_pos + neosu_maps.total_pos;
            float progress = (float)db_pos_sum / (float)db_size_sum;
            if(progress == 0.f) progress = 0.01f;
            if(progress >= 1.f) progress = 0.99f;
            this->fLoadingProgress = progress;

            i32 set_id = neosu_maps.read<i32>();
            u16 nb_diffs = neosu_maps.read<u16>();

            std::string mapset_path = MCENGINE_DATA_DIR "maps/";
            mapset_path.append(std::to_string(set_id));
            mapset_path.append("/");

            std::vector<BeatmapDifficulty *> *diffs = new std::vector<DatabaseBeatmap *>();
            for(u16 j = 0; j < nb_diffs; j++) {
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
                    neosu_maps.skip_bytes(sizeof(TIMINGPOINT) * nb_timing_points);
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

                this->beatmap_difficulties[diff->sMD5Hash] = diff;
                diffs->push_back(diff);
                nb_neosu_maps++;
            }

            // NOTE: Ignoring mapsets with ID -1, since we most likely saved them in the correct folder,
            //       but mistakenly set their ID to -1 (because the ID was missing from the .osu file).
            if(diffs->empty() || set_id == -1) {
                delete diffs;
            } else {
                auto set = new BeatmapSet(diffs, DatabaseBeatmap::BeatmapType::NEOSU_BEATMAPSET);
                this->neosu_sets.push_back(set);

                setIDToIndex[set_id] = beatmapSets.size();
                Beatmap_Set s;
                s.setID = set_id;
                s.diffs2 = diffs;
                beatmapSets.push_back(s);
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
    this->neosu_maps_loaded = true;

    bool should_read_peppy_database = db.total_size > 0;
    if(should_read_peppy_database) {
        // read header
        this->iVersion = db.read<u32>();
        this->iFolderCount = db.read<u32>();
        db.read<u8>();
        db.read<u64>() /* timestamp */;
        auto playerName = db.read_string();
        this->iNumBeatmapsToLoad = db.read<u32>();

        debugLog("Database: version = %i, folderCount = %i, playerName = %s, numDiffs = %i\n", this->iVersion,
                 this->iFolderCount, playerName.c_str(), this->iNumBeatmapsToLoad);

        // hard cap upper db version
        if(this->iVersion > cv_database_version.getInt() && !cv_database_ignore_version.getBool()) {
            osu->getNotificationOverlay()->addToast(
                UString::format("osu!.db version unknown (%i), osu!stable maps will not get loaded.", this->iVersion),
                0xffffff00);
            should_read_peppy_database = false;
        }
    }

    if(should_read_peppy_database) {
        for(int i = 0; i < this->iNumBeatmapsToLoad; i++) {
            if(this->bInterruptLoad.load()) break;  // cancellation point

            if(cv_debug.getBool()) debugLog("Database: Reading beatmap %i/%i ...\n", (i + 1), this->iNumBeatmapsToLoad);

            // update progress (another thread checks if progress >= 1.f to know when we're done)
            u32 db_pos_sum = db.total_pos + neosu_maps.total_pos;
            float progress = (float)db_pos_sum / (float)db_size_sum;
            if(progress == 0.f) progress = 0.01f;
            if(progress >= 1.f) progress = 0.99f;
            this->fLoadingProgress = progress;

            // NOTE: This is documented wrongly in many places.
            //       This int was added in 20160408 and removed in 20191106
            //       https://osu.ppy.sh/home/changelog/stable40/20160408.3
            //       https://osu.ppy.sh/home/changelog/cuttingedge/20191106
            if(this->iVersion >= 20160408 && this->iVersion < 20191106) {
                // size in bytes of the beatmap entry
                db.read<u32>();
            }

            std::string artistName = db.read_string();
            trim(&artistName);
            std::string artistNameUnicode = db.read_string();
            std::string songTitle = db.read_string();
            trim(&songTitle);
            std::string songTitleUnicode = db.read_string();
            std::string creatorName = db.read_string();
            trim(&creatorName);
            std::string difficultyName = db.read_string();
            trim(&difficultyName);
            std::string audioFileName = db.read_string();

            auto md5hash = db.read_hash();
            auto overrides = this->peppy_overrides.find(md5hash);
            bool overrides_found = overrides != this->peppy_overrides.end();

            std::string osuFileName = db.read_string();
            /*unsigned char rankedStatus = */ db.read<u8>();
            unsigned short numCircles = db.read<u16>();
            unsigned short numSliders = db.read<u16>();
            unsigned short numSpinners = db.read<u16>();
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

            double sliderMultiplier = db.read<f64>();

            f32 nomod_star_rating = 0.0f;
            if(this->iVersion >= 20140609) {
                unsigned int numOsuStandardStarRatings = db.read<u32>();
                for(int s = 0; s < numOsuStandardStarRatings; s++) {
                    db.read<u8>();  // 0x08
                    unsigned int mods = db.read<u32>();
                    db.read<u8>();  // 0x0c

                    f32 sr = 0.f;

                    // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                    if(this->iVersion >= 20250108) {
                        sr = db.read<f32>();
                    } else {
                        sr = db.read<f64>();
                    }

                    if(mods == 0) nomod_star_rating = sr;
                }

                unsigned int numTaikoStarRatings = db.read<u32>();
                for(int s = 0; s < numTaikoStarRatings; s++) {
                    db.read<u8>();  // 0x08
                    db.read<u32>();
                    db.read<u8>();  // 0x0c

                    // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                    if(this->iVersion >= 20250108) {
                        db.read<f32>();
                    } else {
                        db.read<f64>();
                    }
                }

                unsigned int numCtbStarRatings = db.read<u32>();
                for(int s = 0; s < numCtbStarRatings; s++) {
                    db.read<u8>();  // 0x08
                    db.read<u32>();
                    db.read<u8>();  // 0x0c

                    // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                    if(this->iVersion >= 20250108) {
                        db.read<f32>();
                    } else {
                        db.read<f64>();
                    }
                }

                unsigned int numManiaStarRatings = db.read<u32>();
                for(int s = 0; s < numManiaStarRatings; s++) {
                    db.read<u8>();  // 0x08
                    db.read<u32>();
                    db.read<u8>();  // 0x0c

                    // https://osu.ppy.sh/home/changelog/stable40/20250108.3
                    if(this->iVersion >= 20250108) {
                        db.read<f32>();
                    } else {
                        db.read<f64>();
                    }
                }
            }

            /*unsigned int drainTime = */ db.read<u32>();  // seconds
            int duration = db.read<u32>();                 // milliseconds
            duration = duration >= 0 ? duration : 0;       // sanity clamp
            int previewTime = db.read<u32>();

            BPMInfo bpm;
            unsigned int nb_timing_points = db.read<u32>();
            if(overrides_found) {
                db.skip_bytes(sizeof(TIMINGPOINT) * nb_timing_points);
                bpm.min = overrides->second.min_bpm;
                bpm.max = overrides->second.max_bpm;
                bpm.most_common = overrides->second.avg_bpm;
            } else {
                zarray<TIMINGPOINT> timingPoints(nb_timing_points);
                db.read_bytes((u8 *)timingPoints.data(), sizeof(TIMINGPOINT) * nb_timing_points);
                bpm = getBPM(timingPoints);
            }

            int beatmapID = db.read<i32>();  // fucking bullshit, this is NOT an unsigned integer as is described on the
                                             // wiki, it can and is -1 sometimes
            int beatmapSetID = db.read<i32>();  // same here
            /*unsigned int threadID = */ db.read<u32>();

            /*unsigned char osuStandardGrade = */ db.read<u8>();
            /*unsigned char taikoGrade = */ db.read<u8>();
            /*unsigned char ctbGrade = */ db.read<u8>();
            /*unsigned char maniaGrade = */ db.read<u8>();

            short localOffset = db.read<u16>();
            float stackLeniency = db.read<f32>();
            unsigned char mode = db.read<u8>();

            auto songSource = db.read_string();
            auto songTags = db.read_string();
            trim(&songSource);
            trim(&songTags);

            short onlineOffset = db.read<u16>();
            db.skip_string();  // song title font
            /*bool unplayed = */ db.read<u8>();
            /*long long lastTimePlayed = */ db.read<u64>();
            /*bool isOsz2 = */ db.read<u8>();

            // somehow, some beatmaps may have spaces at the start/end of their
            // path, breaking the Windows API (e.g. https://osu.ppy.sh/s/215347)
            auto path = db.read_string();
            trim(&path);

            /*long long lastOnlineCheck = */ db.read<u64>();

            /*bool ignoreBeatmapSounds = */ db.read<u8>();
            /*bool ignoreBeatmapSkin = */ db.read<u8>();
            /*bool disableStoryboard = */ db.read<u8>();
            /*bool disableVideo = */ db.read<u8>();
            /*bool visualOverride = */ db.read<u8>();

            if(this->iVersion < 20140609) {
                // https://github.com/ppy/osu/wiki/Legacy-database-file-structure defines it as "Unknown"
                db.read<u16>();
            }

            /*int lastEditTime = */ db.read<u32>();
            /*unsigned char maniaScrollSpeed = */ db.read<u8>();

            // HACKHACK: workaround for linux and macos: it can happen that nested beatmaps are stored in the database,
            // and that osu! stores that filepath with a backslash (because windows)
            if(env->getOS() == Environment::OS::LINUX || env->getOS() == Environment::OS::MACOS) {
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

            DatabaseBeatmap *diff2 =
                new DatabaseBeatmap(fullFilePath, beatmapPath, DatabaseBeatmap::BeatmapType::PEPPY_DIFFICULTY);
            {
                diff2->sTitle = songTitle;
                diff2->sAudioFileName = audioFileName;
                diff2->iLengthMS = duration;

                diff2->fStackLeniency = stackLeniency;

                diff2->sArtist = artistName;
                diff2->sCreator = creatorName;
                diff2->sDifficultyName = difficultyName;
                diff2->sSource = songSource;
                diff2->sTags = songTags;
                diff2->sMD5Hash = md5hash;
                diff2->iID = beatmapID;
                diff2->iSetID = beatmapSetID;

                diff2->fAR = AR;
                diff2->fCS = CS;
                diff2->fHP = HP;
                diff2->fOD = OD;
                diff2->fSliderMultiplier = sliderMultiplier;

                // diff2->sBackgroundImageFileName = "";

                diff2->iPreviewTime = previewTime;
                diff2->last_modification_time = lastModificationTime;

                diff2->sFullSoundFilePath = beatmapPath;
                diff2->sFullSoundFilePath.append(diff2->sAudioFileName);
                diff2->iNumObjects = numCircles + numSliders + numSpinners;
                diff2->iNumCircles = numCircles;
                diff2->iNumSliders = numSliders;
                diff2->iNumSpinners = numSpinners;
                diff2->iMinBPM = bpm.min;
                diff2->iMaxBPM = bpm.max;
                diff2->iMostCommonBPM = bpm.most_common;

                bool loudness_found = false;
                if(overrides_found) {
                    MapOverrides over = overrides->second;
                    diff2->iLocalOffset = over.local_offset;
                    diff2->iOnlineOffset = over.online_offset;
                    diff2->fStarsNomod = over.star_rating;
                    diff2->loudness = over.loudness;
                    diff2->draw_background = over.draw_background;

                    if(over.loudness != 0.f) {
                        loudness_found = true;
                    }
                } else {
                    if(nomod_star_rating <= 0.f) {
                        nomod_star_rating *= -1.f;
                        this->maps_to_recalc.push_back(diff2);
                    }

                    diff2->iLocalOffset = localOffset;
                    diff2->iOnlineOffset = (long)onlineOffset;
                    diff2->fStarsNomod = nomod_star_rating;
                    diff2->draw_background = 1;
                }

                if(!loudness_found) {
                    this->loudness_to_calc.push_back(diff2);
                }
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
            this->beatmap_difficulties[md5hash] = diff2;

            // now, search if the current set (to which this diff would belong) already exists and add it there, or if
            // it doesn't exist then create the set
            const auto result = setIDToIndex.find(beatmapSetID);
            const bool beatmapSetExists = (result != setIDToIndex.end());
            if(beatmapSetExists) {
                bool diff_already_added = false;
                for(auto existing_diff : *beatmapSets[result->second].diffs2) {
                    if(existing_diff->getMD5Hash() == diff2->getMD5Hash()) {
                        diff_already_added = true;
                        break;
                    }
                }
                if(!diff_already_added) {
                    beatmapSets[result->second].diffs2->push_back(diff2);
                }
            } else {
                setIDToIndex[beatmapSetID] = beatmapSets.size();

                Beatmap_Set s;
                s.setID = beatmapSetID;
                s.diffs2 = new std::vector<DatabaseBeatmap *>();
                s.diffs2->push_back(diff2);
                beatmapSets.push_back(s);
            }

            nb_peppy_maps++;
        }

        // build beatmap sets
        for(int i = 0; i < beatmapSets.size(); i++) {
            if(this->bInterruptLoad.load()) break;        // cancellation point
            if(beatmapSets[i].diffs2->empty()) continue;  // sanity check

            if(beatmapSets[i].setID > 0) {
                BeatmapSet *set = new BeatmapSet(beatmapSets[i].diffs2, DatabaseBeatmap::BeatmapType::PEPPY_BEATMAPSET);
                this->beatmapsets.push_back(set);
            } else {
                // set with invalid ID: treat all its diffs separately. we'll group the diffs by title+artist.
                std::unordered_map<std::string, std::vector<DatabaseBeatmap *> *> titleArtistToBeatmap;
                for(auto diff : (*beatmapSets[i].diffs2)) {
                    std::string titleArtist = diff->getTitle();
                    titleArtist.append("|");
                    titleArtist.append(diff->getArtist());

                    auto it = titleArtistToBeatmap.find(titleArtist);
                    if(it == titleArtistToBeatmap.end()) {
                        titleArtistToBeatmap[titleArtist] = new std::vector<DatabaseBeatmap *>();
                    }

                    titleArtistToBeatmap[titleArtist]->push_back(diff);
                }

                for(auto scuffed_set : titleArtistToBeatmap) {
                    BeatmapSet *set =
                        new BeatmapSet(scuffed_set.second, DatabaseBeatmap::BeatmapType::PEPPY_BEATMAPSET);
                    this->beatmapsets.push_back(set);
                }
            }
        }
    }

    this->importTimer->update();
    debugLog("peppy+neosu maps: loading took %f seconds (%d peppy, %d neosu, %d maps total)\n",
             this->importTimer->getElapsedTime(), nb_peppy_maps, nb_neosu_maps, nb_peppy_maps + nb_neosu_maps);
    debugLog("Found %d overrides; %d maps need star recalc, %d maps need loudness recalc\n", nb_overrides,
             this->maps_to_recalc.size(), this->loudness_to_calc.size());
    MapCalcThread::start_calc(this->maps_to_recalc);
    loct_calc(this->loudness_to_calc);

    sct_calc(this->scores_to_convert);
    // @PPV3: save neosu_scores.db once all scores are converted

    load_collections();

    // signal that we are done
    this->fLoadingProgress = 1.0f;
    this->peppy_overrides_mtx.unlock();
}

void Database::saveMaps() {
    debugLog("Osu: Saving maps ...\n");
    if(!this->neosu_maps_loaded) {
        debugLog("Cannot save maps since they weren't loaded properly first!\n");
        return;
    }

    Timer t;
    t.start();

    BanchoFileWriter maps("neosu_maps.db");
    maps.write<u32>(NEOSU_MAPS_DB_VERSION);

    // Save neosu-downloaded maps
    u32 nb_diffs_saved = 0;
    maps.write<u32>(this->neosu_sets.size());
    for(BeatmapSet *beatmap : this->neosu_sets) {
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

            nb_diffs_saved++;
        }
    }

    // We want to save settings we applied on peppy-imported maps
    this->peppy_overrides_mtx.lock();

    // When calculating loudness we don't call update_overrides() for performance reasons
    for(auto diff2 : this->loudness_to_calc) {
        if(diff2->type != DatabaseBeatmap::BeatmapType::PEPPY_DIFFICULTY) continue;
        if(diff2->loudness.load() == 0.f) continue;
        this->peppy_overrides[diff2->getMD5Hash()] = diff2->get_overrides();
    }

    u32 nb_overrides = 0;
    maps.write<u32>(this->peppy_overrides.size());
    for(auto &pair : this->peppy_overrides) {
        maps.write_hash(pair.first);
        maps.write<i16>(pair.second.local_offset);
        maps.write<i16>(pair.second.online_offset);
        maps.write<f32>(pair.second.star_rating);
        maps.write<f32>(pair.second.loudness);
        maps.write<i32>(pair.second.min_bpm);
        maps.write<i32>(pair.second.max_bpm);
        maps.write<i32>(pair.second.avg_bpm);
        maps.write<u8>(pair.second.draw_background);

        nb_overrides++;
    }
    this->peppy_overrides_mtx.unlock();

    t.update();
    debugLog("Saved %d maps (+ %d overrides) in %f seconds.\n", nb_diffs_saved, nb_overrides, t.getElapsedTime());
}

void Database::loadScores() {
    debugLog("Database::loadScores()\n");

    this->scores_to_convert.clear();

    u32 nb_neosu_scores = 0;
    BanchoFileReader db("neosu_scores.db");
    if(!this->bScoresLoaded && db.total_size > 0) {
        u8 magic_bytes[6] = {0};
        db.read_bytes(magic_bytes, 5);
        if(memcmp(magic_bytes, "NEOSC", 5) != 0) {
            osu->getNotificationOverlay()->addToast("Failed to load neosu_scores.db!", 0xffff0000);
            return;
        }

        u32 db_version = db.read<u32>();
        if(db_version > NEOSU_SCORE_DB_VERSION) {
            debugLog("neosu_scores.db version is newer than current neosu version!\n");
            return;
        } else if(db_version < NEOSU_SCORE_DB_VERSION) {
            // Reading from older database version: backup just in case
            auto backup_path = UString::format("neosu_scores.db.%d", db_version);
            copy("neosu_scores.db", backup_path.toUtf8());
        }

        u32 nb_beatmaps = db.read<u32>();
        u32 nb_scores = db.read<u32>();
        this->scores.reserve(nb_beatmaps);

        for(u32 b = 0; b < nb_beatmaps; b++) {
            MD5Hash beatmap_hash = db.read_hash();
            u32 nb_beatmap_scores = db.read<u32>();

            for(u32 s = 0; s < nb_beatmap_scores; s++) {
                FinishedScore sc;

                sc.mods.flags = db.read<u64>();
                sc.mods.speed = db.read<f32>();
                sc.mods.notelock_type = db.read<i32>();
                sc.mods.ar_override = db.read<f32>();
                sc.mods.ar_overridenegative = db.read<f32>();
                sc.mods.cs_override = db.read<f32>();
                sc.mods.cs_overridenegative = db.read<f32>();
                sc.mods.hp_override = db.read<f32>();
                sc.mods.od_override = db.read<f32>();
                if(sc.mods.flags & Replay::ModFlags::Autopilot) {
                    sc.mods.autopilot_lenience = db.read<f32>();
                }
                if(sc.mods.flags & Replay::ModFlags::Timewarp) {
                    sc.mods.timewarp_multiplier = db.read<f32>();
                }
                if(sc.mods.flags & Replay::ModFlags::Minimize) {
                    sc.mods.minimize_multiplier = db.read<f32>();
                }
                if(sc.mods.flags & Replay::ModFlags::ARTimewarp) {
                    sc.mods.artimewarp_multiplier = db.read<f32>();
                }
                if(sc.mods.flags & Replay::ModFlags::ARWobble) {
                    sc.mods.arwobble_strength = db.read<f32>();
                    sc.mods.arwobble_interval = db.read<f32>();
                }
                if(sc.mods.flags & (Replay::ModFlags::Wobble1 | Replay::ModFlags::Wobble2)) {
                    sc.mods.wobble_strength = db.read<f32>();
                    sc.mods.wobble_frequency = db.read<f32>();
                    sc.mods.wobble_rotation_speed = db.read<f32>();
                }
                if(sc.mods.flags & (Replay::ModFlags::Jigsaw1 | Replay::ModFlags::Jigsaw2)) {
                    sc.mods.jigsaw_followcircle_radius_factor = db.read<f32>();
                }
                if(sc.mods.flags & Replay::ModFlags::Shirone) {
                    sc.mods.shirone_combo = db.read<f32>();
                }

                sc.score = db.read<u64>();
                sc.spinner_bonus = db.read<u64>();
                sc.unixTimestamp = db.read<u64>();
                sc.player_id = db.read<u32>();
                sc.playerName = db.read_string();
                sc.grade = (FinishedScore::Grade)db.read<u8>();

                sc.client = db.read_string();
                sc.server = db.read_string();
                sc.bancho_score_id = db.read<u64>();
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
        }

        if(nb_neosu_scores != nb_scores) {
            debugLog("Inconsistency in neosu_scores.db! Expected %d scores, found %d!\n", nb_scores, nb_neosu_scores);
        }
    }

    u32 nb_old_neosu_imported = this->importOldNeosuScores();
    u32 nb_peppy_imported = this->importPeppyScores();
    this->bScoresLoaded = true;

    u32 scores_total = nb_old_neosu_imported + nb_neosu_scores + nb_peppy_imported;
    debugLog("Loaded %i scores (%d old neosu, %d neosu, %d peppy)\n", scores_total, nb_old_neosu_imported,
             nb_neosu_scores, nb_peppy_imported);
}

u32 Database::importOldNeosuScores() {
    BanchoFileReader db("scores.db");

    u32 db_version = db.read<u32>();
    u32 nb_beatmaps = db.read<u32>();
    if(db_version == 0) {
        // scores.db doesn't exist
        return 0;
    }

    // 20240412 is the only scores.db version in which replays are saved.
    // Don't bother importing if we don't have replays.
    if(db_version != 20240412) {
        debugLog("Unsupported scores.db version.\n");
        return 0;
    }

    int nb_imported = 0;
    for(u32 b = 0; b < nb_beatmaps; b++) {
        auto md5hash = db.read_hash();
        u32 nb_scores = db.read<u32>();

        for(u32 s = 0; s < nb_scores; s++) {
            db.read<u8>();   // gamemode (always 0)
            db.read<u32>();  // score version

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
            auto cvrs = UString(experimentalModsConVars.c_str());
            auto experimentalMods = cvrs.split(";");
            for(auto mod : experimentalMods) {
                if(mod == UString("")) continue;
                if(mod == UString("fposu_mod_strafing")) sc.mods.flags |= Replay::ModFlags::FPoSu_Strafing;
                if(mod == UString("osu_mod_wobble")) sc.mods.flags |= Replay::ModFlags::Wobble1;
                if(mod == UString("osu_mod_wobble2")) sc.mods.flags |= Replay::ModFlags::Wobble2;
                if(mod == UString("osu_mod_arwobble")) sc.mods.flags |= Replay::ModFlags::ARWobble;
                if(mod == UString("osu_mod_timewarp")) sc.mods.flags |= Replay::ModFlags::Timewarp;
                if(mod == UString("osu_mod_artimewarp")) sc.mods.flags |= Replay::ModFlags::ARTimewarp;
                if(mod == UString("osu_mod_minimize")) sc.mods.flags |= Replay::ModFlags::Minimize;
                if(mod == UString("osu_mod_fadingcursor")) sc.mods.flags |= Replay::ModFlags::FadingCursor;
                if(mod == UString("osu_mod_fps")) sc.mods.flags |= Replay::ModFlags::FPS;
                if(mod == UString("osu_mod_jigsaw1")) sc.mods.flags |= Replay::ModFlags::Jigsaw1;
                if(mod == UString("osu_mod_jigsaw2")) sc.mods.flags |= Replay::ModFlags::Jigsaw2;
                if(mod == UString("osu_mod_fullalternate")) sc.mods.flags |= Replay::ModFlags::FullAlternate;
                if(mod == UString("osu_mod_reverse_sliders")) sc.mods.flags |= Replay::ModFlags::ReverseSliders;
                if(mod == UString("osu_mod_no50s")) sc.mods.flags |= Replay::ModFlags::No50s;
                if(mod == UString("osu_mod_no100s")) sc.mods.flags |= Replay::ModFlags::No100s;
                if(mod == UString("osu_mod_ming3012")) sc.mods.flags |= Replay::ModFlags::Ming3012;
                if(mod == UString("osu_mod_halfwindow")) sc.mods.flags |= Replay::ModFlags::HalfWindow;
                if(mod == UString("osu_mod_millhioref")) sc.mods.flags |= Replay::ModFlags::Millhioref;
                if(mod == UString("osu_mod_mafham")) sc.mods.flags |= Replay::ModFlags::Mafham;
                if(mod == UString("osu_mod_strict_tracking")) sc.mods.flags |= Replay::ModFlags::StrictTracking;
                if(mod == UString("osu_playfield_mirror_horizontal"))
                    sc.mods.flags |= Replay::ModFlags::MirrorHorizontal;
                if(mod == UString("osu_playfield_mirror_vertical")) sc.mods.flags |= Replay::ModFlags::MirrorVertical;
                if(mod == UString("osu_mod_shirone")) sc.mods.flags |= Replay::ModFlags::Shirone;
                if(mod == UString("osu_mod_approach_different")) sc.mods.flags |= Replay::ModFlags::ApproachDifferent;
            }

            sc.beatmap_hash = md5hash;
            sc.perfect = sc.comboMax >= sc.maxPossibleCombo;
            sc.grade = sc.calculate_grade();

            if(this->addScoreRaw(sc)) {
                nb_imported++;
            }
        }
    }

    return nb_imported;
}

u32 Database::importPeppyScores() {
    int nb_imported = 0;

    std::string scoresPath = cv_osu_folder.getString().toUtf8();
    scoresPath.append("/scores.db");
    BanchoFileReader db(scoresPath.c_str());

    u32 db_version = db.read<u32>();
    u32 nb_beatmaps = db.read<u32>();

    if(db_version == 0) {
        // scores.db doesn't exist
        return 0;
    }

    debugLog("osu!stable scores.db: version = %i, nb_beatmaps = %i\n", db_version, nb_beatmaps);

    char client_str[15] = "peppy-YYYYMMDD";
    for(int b = 0; b < nb_beatmaps; b++) {
        std::string md5hash_str = db.read_string();
        if(md5hash_str.length() < 32) {
            debugLog("WARNING: Invalid score on beatmap %i with md5hash_str.length() = %i!\n", b, md5hash_str.length());
            continue;
        } else if(md5hash_str.length() > 32) {
            debugLog("ERROR: Corrupt score database/entry detected, stopping.\n");
            break;
        }

        MD5Hash md5hash;

        memcpy(md5hash.hash, md5hash_str.c_str(), 32);
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
                sc.bancho_score_id = db.read<u64>();
            } else if(score_version >= 20121008) {
                sc.bancho_score_id = db.read<i32>();
            } else {
                sc.bancho_score_id = 0;
            }

            if(sc.mods.flags & Replay::ModFlags::Target) {
                db.read<f64>();  // total accuracy
            }

            if(gamemode == 0 && sc.bancho_score_id != 0) {
                sc.beatmap_hash = md5hash;
                sc.grade = sc.calculate_grade();

                if(this->addScoreRaw(sc)) {
                    nb_imported++;
                }
            }
        }
    }

    debugLog("Imported %i scores from osu!stable.\n", nb_imported);
    return nb_imported;
}

void Database::saveScores() {
    debugLog("Osu: Saving scores ...\n");
    if(!this->bScoresLoaded) {
        debugLog("Cannot save scores since they weren't loaded properly first!\n");
        return;
    }

    const double startTime = Timing::getTimeReal();

    std::lock_guard<std::mutex> lock(this->scores_mtx);
    BanchoFileWriter db("neosu_scores.db");
    db.write_bytes((u8 *)"NEOSC", 5);
    db.write<u32>(NEOSU_SCORE_DB_VERSION);

    u32 nb_beatmaps = 0;
    u32 nb_scores = 0;
    for(auto &it : this->scores) {
        u32 beatmap_scores = it.second.size();
        if(beatmap_scores > 0) {
            nb_beatmaps++;
            nb_scores += beatmap_scores;
        }
    }
    db.write<u32>(nb_beatmaps);
    db.write<u32>(nb_scores);

    for(auto &it : this->scores) {
        if(it.second.empty()) continue;

        db.write_hash(it.first);
        db.write<u32>(it.second.size());

        for(auto &score : it.second) {
            db.write<u64>(score.mods.flags);
            db.write<f32>(score.mods.speed);
            db.write<i32>(score.mods.notelock_type);
            db.write<f32>(score.mods.ar_override);
            db.write<f32>(score.mods.ar_overridenegative);
            db.write<f32>(score.mods.cs_override);
            db.write<f32>(score.mods.cs_overridenegative);
            db.write<f32>(score.mods.hp_override);
            db.write<f32>(score.mods.od_override);
            if(score.mods.flags & Replay::ModFlags::Autopilot) {
                db.write<f32>(score.mods.autopilot_lenience);
            }
            if(score.mods.flags & Replay::ModFlags::Timewarp) {
                db.write<f32>(score.mods.timewarp_multiplier);
            }
            if(score.mods.flags & Replay::ModFlags::Minimize) {
                db.write<f32>(score.mods.minimize_multiplier);
            }
            if(score.mods.flags & Replay::ModFlags::ARTimewarp) {
                db.write<f32>(score.mods.artimewarp_multiplier);
            }
            if(score.mods.flags & Replay::ModFlags::ARWobble) {
                db.write<f32>(score.mods.arwobble_strength);
                db.write<f32>(score.mods.arwobble_interval);
            }
            if(score.mods.flags & (Replay::ModFlags::Wobble1 | Replay::ModFlags::Wobble2)) {
                db.write<f32>(score.mods.wobble_strength);
                db.write<f32>(score.mods.wobble_frequency);
                db.write<f32>(score.mods.wobble_rotation_speed);
            }
            if(score.mods.flags & (Replay::ModFlags::Jigsaw1 | Replay::ModFlags::Jigsaw2)) {
                db.write<f32>(score.mods.jigsaw_followcircle_radius_factor);
            }
            if(score.mods.flags & Replay::ModFlags::Shirone) {
                db.write<f32>(score.mods.shirone_combo);
            }

            db.write<u64>(score.score);
            db.write<u64>(score.spinner_bonus);
            db.write<u64>(score.unixTimestamp);
            db.write<u32>(score.player_id);
            db.write_string(score.playerName);
            db.write<u8>((u8)score.grade);

            db.write_string(score.client);
            db.write_string(score.server);
            db.write<u64>(score.bancho_score_id);
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

    debugLog("Saved %d scores in %f seconds.\n", nb_scores, (Timing::getTimeReal() - startTime));
}

BeatmapSet *Database::loadRawBeatmap(std::string beatmapPath) {
    if(cv_debug.getBool()) debugLog("BeatmapDatabase::loadRawBeatmap() : %s\n", beatmapPath.c_str());

    // try loading all diffs
    std::vector<BeatmapDifficulty *> *diffs2 = new std::vector<BeatmapDifficulty *>();
    std::vector<std::string> beatmapFiles = env->getFilesInFolder(beatmapPath);
    for(int i = 0; i < beatmapFiles.size(); i++) {
        std::string ext = env->getFileExtensionFromFilePath(beatmapFiles[i]);
        if(ext.compare("osu") != 0) continue;

        std::string fullFilePath = beatmapPath;
        fullFilePath.append(beatmapFiles[i]);

        BeatmapDifficulty *diff2 =
            new BeatmapDifficulty(fullFilePath, beatmapPath, DatabaseBeatmap::BeatmapType::NEOSU_DIFFICULTY);
        if(diff2->loadMetadata()) {
            diffs2->push_back(diff2);
        } else {
            if(cv_debug.getBool()) {
                debugLog("BeatmapDatabase::loadRawBeatmap() : Couldn't loadMetadata(), deleting object.\n");
            }
            SAFE_DELETE(diff2);
        }
    }

    BeatmapSet *set = NULL;
    if(diffs2->empty()) {
        delete diffs2;
    } else {
        set = new BeatmapSet(diffs2, DatabaseBeatmap::BeatmapType::NEOSU_BEATMAPSET);
    }

    return set;
}
