#pragma once
// Copyright (c) 2016, PG, All rights reserved.

#include "ConVar.h"

#include "Resource.h"
#include "LegacyReplay.h"
#include "Overrides.h"
#include "UString.h"
#include "score.h"
#include "SyncMutex.h"

#include <atomic>

namespace Timing {
class Timer;
}
class ConVar;

class DatabaseBeatmap;
typedef DatabaseBeatmap BeatmapDifficulty;
typedef DatabaseBeatmap BeatmapSet;

#define NEOSU_MAPS_DB_VERSION 20250801
#define NEOSU_SCORE_DB_VERSION 20240725

class Database;
extern std::unique_ptr<Database> db;

class Database {
// Field ordering matters here
#pragma pack(push, 1)
    struct alignas(1) TIMINGPOINT {
        double msPerBeat;
        double offset;
        bool timingChange;
    };
#pragma pack(pop)
   public:
    struct PlayerStats {
        UString name;
        float pp;
        float accuracy;
        int numScoresWithPP;
        int level;
        float percentToNextLevel;
        unsigned long long totalScore;
    };

    struct PlayerPPScores {
        std::vector<FinishedScore *> ppScores;
        unsigned long long totalScore;
    };

    struct SCORE_SORTING_METHOD {
        std::string name;
        std::function<bool(FinishedScore const &, FinishedScore const &)> comparator;
    };

   public:
    Database();
    ~Database();

    void update();

    void load();
    void cancel();
    void save();

    BeatmapSet *addBeatmapSet(const std::string &beatmapFolderPath, i32 set_id_override = -1);

    int addScore(const FinishedScore &score);
    void deleteScore(MD5Hash beatmapMD5Hash, u64 scoreUnixTimestamp);
    void sortScoresInPlace(std::vector<FinishedScore> &scores);
    void sortScores(MD5Hash beatmapMD5Hash);

    std::vector<UString> getPlayerNamesWithPPScores();
    std::vector<UString> getPlayerNamesWithScoresForUserSwitcher();
    PlayerPPScores getPlayerPPScores(const std::string &playerName);
    PlayerStats calculatePlayerStats(const std::string &playerName);
    static float getWeightForIndex(int i);
    static float getBonusPPForNumScores(size_t numScores);
    static unsigned long long getRequiredScoreForLevel(int level);
    static int getLevelForScore(unsigned long long score, int maxLevel = 120);

    inline float getProgress() const { return this->fLoadingProgress.load(std::memory_order_acquire); }
    inline bool isLoading() const {
        float progress = this->getProgress();
        return progress > 0.f && progress < 1.f;
    }
    inline bool isFinished() const { return (this->getProgress() >= 1.0f); }
    inline bool foundChanges() const { return this->bFoundChanges; }

    DatabaseBeatmap *getBeatmapDifficulty(const MD5Hash &md5hash);
    DatabaseBeatmap *getBeatmapDifficulty(i32 map_id);
    DatabaseBeatmap *getBeatmapSet(i32 set_id);
    inline const std::vector<DatabaseBeatmap *> &getBeatmapSets() const { return this->beatmapsets; }

    inline std::unordered_map<MD5Hash, std::vector<FinishedScore>> *getScores() { return &this->scores; }
    inline const std::array<SCORE_SORTING_METHOD, 6> &getScoreSortingMethods() const {
        return this->scoreSortingMethods;
    }

    static std::string getOsuSongsFolder();

    BeatmapSet *loadRawBeatmap(const std::string &beatmapPath);  // only used for raw loading without db

    UString parseLegacyCfgBeatmapDirectoryParameter();
    void scheduleLoadRaw();
    Sync::mutex peppy_overrides_mtx;
    std::unordered_map<MD5Hash, MapOverrides> peppy_overrides;
    std::vector<BeatmapDifficulty *> maps_to_recalc;
    std::vector<BeatmapDifficulty *> loudness_to_calc;

    Sync::mutex scores_mtx;
    std::atomic<bool> bDidScoresChangeForStats = true;
    std::unordered_map<MD5Hash, std::vector<FinishedScore>> scores;
    std::unordered_map<MD5Hash, std::vector<FinishedScore>> online_scores;

    // should only be accessed from database loader thread!
    std::unordered_map<std::string, UString> database_files;
    u64 bytes_processed{0};
    u64 total_bytes{0};
    std::atomic<float> fLoadingProgress;

    // fine to be modified as long as the db is not currently being loaded
    std::vector<std::string> dbPathsToImport;

   private:
    class AsyncDBLoader : public Resource {
        NOCOPY_NOMOVE(AsyncDBLoader)
       public:
        ~AsyncDBLoader() override = default;
        AsyncDBLoader() : Resource() {}

        [[nodiscard]] Type getResType() const override { return APPDEFINED; }

       protected:
        void init() override;
        void initAsync() override;
        void destroy() override { ; }

       private:
        friend class Database;
    };

    friend class AsyncDBLoader;
    void startLoader();
    void destroyLoader();

    void saveMaps();

    void findDatabases();
    bool importDatabase(const std::string &db_path);
    void loadMaps();
    void loadScores(const UString &dbPath);
    void loadOldMcNeosuScores(const UString &dbPath);
    void loadPeppyScores(const UString &dbPath);
    void saveScores();
    bool addScoreRaw(const FinishedScore &score);
    // returns position of existing score in the scores[hash] array if found, -1 otherwise
    int isScoreAlreadyInDB(u64 unix_timestamp, const MD5Hash &map_hash);

    AsyncDBLoader *loader{nullptr};
    Timing::Timer *importTimer;
    bool bIsFirstLoad;   // only load differences after first raw load
    bool bFoundChanges;  // for total refresh detection of raw loading

    // global
    int iNumBeatmapsToLoad;
    std::atomic<bool> bInterruptLoad;
    std::vector<BeatmapSet *> beatmapsets;

    Sync::mutex beatmap_difficulties_mtx;
    std::unordered_map<MD5Hash, BeatmapDifficulty *> beatmap_difficulties;

    bool neosu_maps_loaded = false;

    // osu!.db
    int iVersion;
    int iFolderCount;

    // scores.db (legacy and custom)
    bool bScoresLoaded = false;

    bool bNeedRawLoad{false};

    PlayerStats prevPlayerStats;
    std::array<SCORE_SORTING_METHOD, 6> scoreSortingMethods;

    // raw load
    bool bRawBeatmapLoadScheduled{false};
    int iCurRawBeatmapLoadIndex{0};
    std::string sRawBeatmapLoadOsuSongFolder;
    std::vector<std::string> rawBeatmapFolders;
    std::vector<std::string> rawLoadBeatmapFolders;
};
