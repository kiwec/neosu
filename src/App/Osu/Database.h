#pragma once
// Copyright (c) 2016, PG, All rights reserved.

#include "Resource.h"
#include "LegacyReplay.h"
#include "Overrides.h"
#include "UString.h"
#include "score.h"
#include "SyncMutex.h"

#include <atomic>
#include <set>

namespace Timing {
class Timer;
}
namespace Collections {
extern bool load_all();
extern bool load_peppy(std::string_view peppy_collections_path);
extern bool load_mcneosu(std::string_view neosu_collections_path);
extern bool save_collections();
}  // namespace Collections
namespace LegacyReplay {
extern bool load_from_disk(FinishedScore &score, bool update_db);
}

class ScoreButton;
class ScoreConverter;
class ConVar;

class DatabaseBeatmap;
typedef DatabaseBeatmap BeatmapDifficulty;
typedef DatabaseBeatmap BeatmapSet;

#define NEOSU_MAPS_DB_VERSION 20251009
#define NEOSU_SCORE_DB_VERSION 20240725

class Database;
// global for convenience, created in osu constructor, destroyed in osu constructor
extern std::unique_ptr<Database> db;

// Field ordering matters here
#pragma pack(push, 1)
struct alignas(1) DB_TIMINGPOINT {
    double msPerBeat;
    double offset;
    bool uninherited;
};
#pragma pack(pop)

using HashToScoreMap = std::unordered_map<MD5Hash, std::vector<FinishedScore>>;

class Database {
    NOCOPY_NOMOVE(Database)
   public:
    struct PlayerStats {
        UString name;
        float pp;
        float accuracy;
        int numScoresWithPP;
        int level;
        float percentToNextLevel;
        u64 totalScore;
    };

    struct PlayerPPScores {
        std::vector<FinishedScore *> ppScores;
        u64 totalScore;
    };

    struct SCORE_SORTING_METHOD {
        using SCORE_SORTING_COMPARATOR = bool (*)(const FinishedScore &, const FinishedScore &);

        std::string_view name;
        SCORE_SORTING_COMPARATOR comparator;
    };

    // sorting methods
    static bool sortScoreByScore(const FinishedScore &a, const FinishedScore &b);
    static bool sortScoreByCombo(const FinishedScore &a, const FinishedScore &b);
    static bool sortScoreByDate(const FinishedScore &a, const FinishedScore &b);
    static bool sortScoreByMisses(const FinishedScore &a, const FinishedScore &b);
    static bool sortScoreByAccuracy(const FinishedScore &a, const FinishedScore &b);
    static bool sortScoreByPP(const FinishedScore &a, const FinishedScore &b);

   public:
    static constexpr std::array<SCORE_SORTING_METHOD, 6> SCORE_SORTING_METHODS{{{"By accuracy", sortScoreByAccuracy},
                                                                                {"By combo", sortScoreByCombo},
                                                                                {"By date", sortScoreByDate},
                                                                                {"By misses", sortScoreByMisses},
                                                                                {"By score", sortScoreByScore},
                                                                                {"By pp", sortScoreByPP}}};

   public:
    Database();
    ~Database();

    void update();

    void load();
    void cancel();
    void save();

    BeatmapSet *addBeatmapSet(const std::string &beatmapFolderPath, i32 set_id_override = -1);

    int addScore(const FinishedScore &score);
    void deleteScore(const MD5Hash &beatmapMD5Hash, u64 scoreUnixTimestamp);
    void sortScoresInPlace(std::vector<FinishedScore> &scores, bool lock_scores_mutex = true);
    inline void sortScores(const MD5Hash &beatmapMD5Hash) { return this->sortScoresInt(beatmapMD5Hash, true); }

    std::vector<UString> getPlayerNamesWithPPScores();
    std::vector<UString> getPlayerNamesWithScoresForUserSwitcher();
    PlayerPPScores getPlayerPPScores(const std::string &playerName);
    PlayerStats calculatePlayerStats(const std::string &playerName);
    static float getWeightForIndex(int i);
    static float getBonusPPForNumScores(size_t numScores);
    static u64 getRequiredScoreForLevel(int level);
    static int getLevelForScore(u64 score, int maxLevel = 120);

    inline float getProgress() const { return this->loading_progress.load(std::memory_order_acquire); }
    inline bool isLoading() const {
        float progress = this->getProgress();
        return progress > 0.f && progress < 1.f;
    }
    inline bool isFinished() const { return (this->getProgress() >= 1.0f); }
    inline bool foundChanges() const { return this->raw_found_changes; }

    DatabaseBeatmap *getBeatmapDifficulty(const MD5Hash &md5hash);
    DatabaseBeatmap *getBeatmapDifficulty(i32 map_id);
    BeatmapSet *getBeatmapSet(i32 set_id);
    inline const std::vector<BeatmapSet *> &getBeatmapSets() const { return this->beatmapsets; }

    // WARNING: Before calling getScores(), you need to lock db->scores_mtx!
    inline const HashToScoreMap &getScores() const { return this->scores; }
    inline HashToScoreMap &getOnlineScores() { return this->online_scores; }

    static std::string getOsuSongsFolder();

    BeatmapSet *loadRawBeatmap(const std::string &beatmapPath);  // only used for raw loading without db

    inline void addPathToImport(const std::string &dbPath) { this->extern_db_paths_to_import.push_back(dbPath); }

    // locks peppy_overrides mutex and updates overrides for loaded-from-stable-db maps which will be stored in the local database
    void update_overrides(DatabaseBeatmap *beatmap);

    Sync::shared_mutex peppy_overrides_mtx;
    Sync::shared_mutex scores_mtx;
    std::atomic<bool> scores_changed{true};

    std::unordered_map<MD5Hash, MapOverrides> peppy_overrides;
    std::vector<BeatmapDifficulty *> maps_to_recalc;
    std::vector<BeatmapDifficulty *> loudness_to_calc;

   private:
    friend bool Collections::load_all();
    friend bool Collections::load_peppy(std::string_view peppy_collections_path);
    friend bool Collections::load_mcneosu(std::string_view neosu_collections_path);
    friend bool Collections::save_collections();
    friend class DatabaseBeatmap;

    void scheduleLoadRaw();

    // for updating scores externally
    friend class ScoreConverter;
    friend class ScoreButton;  // HACKHACK: why are we updating database scores from a BUTTON???
    friend bool LegacyReplay::load_from_disk(FinishedScore &score, bool update_db);
    inline HashToScoreMap &getScoresMutable() { return this->scores; }

    HashToScoreMap scores;
    HashToScoreMap online_scores;

    enum class DatabaseType : u8 {
        INVALID_DB = 0,
        NEOSU_SCORES = 1,
        MCNEOSU_SCORES = 2,
        MCNEOSU_COLLECTIONS = 3,  // mcosu/neosu both use same collection format
        NEOSU_MAPS = 4,
        STABLE_SCORES = 5,
        STABLE_COLLECTIONS = 6,
        STABLE_MAPS = 7,
        LAST = STABLE_MAPS
    };

    static std::string getDBPath(DatabaseType db_type);
    static DatabaseType getDBType(std::string_view db_path);

    // should only be accessed from database loader thread!
    std::unordered_map<DatabaseType, std::string> database_files;
    std::set<std::pair<DatabaseType, std::string>> external_databases;

    u64 bytes_processed{0};
    u64 total_bytes{0};
    std::atomic<float> loading_progress{0.f};

    std::vector<std::string> extern_db_paths_to_import;
    // copy so that more can be added without thread races during loading
    std::vector<std::string> extern_db_paths_to_import_async_copy;

    class AsyncDBLoader final : public Resource {
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
    bool importDatabase(const std::pair<DatabaseType, std::string> &db_pair);
    void loadMaps();
    void loadScores(std::string_view dbPath);
    void loadOldMcNeosuScores(std::string_view dbPath);
    void loadPeppyScores(std::string_view dbPath);
    void saveScores();
    bool addScoreRaw(const FinishedScore &score);
    void sortScoresInt(const MD5Hash &beatmapMD5Hash, bool lock_scores_mutex);
    // returns position of existing score in the scores[hash] array if found, -1 otherwise
    int isScoreAlreadyInDB(u64 unix_timestamp, const MD5Hash &map_hash);

    AsyncDBLoader *loader{nullptr};
    std::unique_ptr<Timing::Timer> importTimer;
    bool is_first_load{true};      // only load differences after first raw load
    bool raw_found_changes{true};  // for total refresh detection of raw loading

    // global
    u32 num_beatmaps_to_load{0};
    std::atomic<bool> load_interrupted{false};
    std::vector<BeatmapSet *> beatmapsets;
    std::vector<BeatmapSet *>
        temp_loading_beatmapsets;  // only used during loading, contents moved into beatmapsets after

    Sync::shared_mutex beatmap_difficulties_mtx;
    std::unordered_map<MD5Hash, BeatmapDifficulty *> beatmap_difficulties;

    bool neosu_maps_loaded{false};

    // scores.db (legacy and custom)
    bool scores_loaded{false};

    PlayerStats prevPlayerStats{
        .name = "",
        .pp = 0.0f,
        .accuracy = 0.0f,
        .numScoresWithPP = 0,
        .level = 0,
        .percentToNextLevel = 0.0f,
        .totalScore = 0,
    };

    // raw load
    bool needs_raw_load{false};
    bool raw_load_scheduled{false};
    u32 cur_raw_load_idx{0};
    std::string raw_load_osu_song_folder;
    std::vector<std::string> raw_loaded_beatmap_folders;
    std::vector<std::string> raw_load_beatmap_folders;
};
