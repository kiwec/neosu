#pragma once
// Copyright (c) 2020, PG, All rights reserved.

#include "DifficultyCalculator.h"
#include "HitSounds.h"
#include "Osu.h"
#include "Overrides.h"
#include "templates.h"

#include <atomic>
#include <string_view>

class AbstractBeatmapInterface;
class HitObject;

class Database;

class BGImageHandler;

// purpose:
// 1) contain all infos which are ALWAYS kept in memory for beatmaps
// 2) be the data source for Beatmap when starting a difficulty
// 3) allow async calculations/loaders to work on the contained data (e.g. background image loader)
// 4) be a container for difficulties (all top level DatabaseBeatmap objects are containers)

class DatabaseBeatmap;
typedef DatabaseBeatmap BeatmapDifficulty;
typedef DatabaseBeatmap BeatmapSet;

class DatabaseBeatmap final {
    NOCOPY_NOMOVE(DatabaseBeatmap)
   public:
    // raw structs (not editable, we're following db format directly)
    struct TIMINGPOINT {
        f64 offset;
        f64 msPerBeat;

        i32 sampleSet;
        i32 sampleIndex;
        i32 volume;

        bool timingChange;
        bool kiai;
    };

    struct BREAK {
        i64 startTime;
        i64 endTime;
    };

    // custom structs
    struct LOAD_DIFFOBJ_RESULT {
        std::vector<OsuDifficultyHitObject> diffobjects{};

        int maxPossibleCombo{};
        int errorCode{0};
    };

    struct LOAD_GAMEPLAY_RESULT {
        std::vector<HitObject *> hitobjects;
        std::vector<BREAK> breaks;
        std::vector<Color> combocolors;

        i32 defaultSampleSet{1};
        int errorCode{0};
    };

    struct TIMING_INFO {
        i32 offset = 0;

        f32 beatLengthBase = 0.f;
        f32 beatLength = 0.f;

        i32 sampleSet = 0;
        i32 sampleIndex = 0;
        i32 volume = 0;

        bool isNaN = false;
    };

    enum class BeatmapType : uint8_t {
        NEOSU_BEATMAPSET,
        PEPPY_BEATMAPSET,
        NEOSU_DIFFICULTY,
        PEPPY_DIFFICULTY,
    };

    // primitive objects

    struct HITCIRCLE {
        int x, y;
        u32 time;
        int number;
        int colorCounter;
        int colorOffset;
        bool clicked;
        HitSamples samples;
    };

    struct SLIDER {
        int x, y;
        char type;
        int repeat;
        float pixelLength;
        u32 time;
        int number;
        int colorCounter;
        int colorOffset;
        std::vector<vec2> points;
        HitSamples hoverSamples;
        std::vector<HitSamples> edgeSamples;

        float sliderTime;
        float sliderTimeWithoutRepeats;
        std::vector<float> ticks;

        std::vector<OsuDifficultyHitObject::SLIDER_SCORING_TIME> scoringTimesForStarCalc;
    };

    struct SPINNER {
        int x, y;
        u32 time;
        u32 endTime;
        HitSamples samples;
    };

    struct PRIMITIVE_CONTAINER {
        std::vector<HITCIRCLE> hitcircles{};
        std::vector<SLIDER> sliders{};
        std::vector<SPINNER> spinners{};
        std::vector<BREAK> breaks{};

        zarray<DatabaseBeatmap::TIMINGPOINT> timingpoints{};
        std::vector<Color> combocolors{};

        f32 stackLeniency{};
        f32 sliderMultiplier{};
        f32 sliderTickRate{};

        u32 numCircles{};
        u32 numSliders{};
        u32 numSpinners{};
        u32 numHitobjects{};

        // sample set to use if timing point doesn't specify it
        // 1 = normal, 2 = soft, 3 = drum
        i32 defaultSampleSet{1};

        i32 version{};
        i32 errorCode{0};
    };

    DatabaseBeatmap(std::string filePath, std::string folder, BeatmapType type);
    DatabaseBeatmap(std::vector<DatabaseBeatmap *> *difficulties, BeatmapType type);
    ~DatabaseBeatmap();

    static LOAD_DIFFOBJ_RESULT loadDifficultyHitObjects(std::string_view osuFilePath, float AR, float CS,
                                                        float speedMultiplier, bool calculateStarsInaccurately = false);
    static LOAD_DIFFOBJ_RESULT loadDifficultyHitObjects(std::string_view osuFilePath, float AR, float CS,
                                                        float speedMultiplier, bool calculateStarsInaccurately,
                                                        const std::atomic<bool> &dead);
    static LOAD_DIFFOBJ_RESULT loadDifficultyHitObjects(PRIMITIVE_CONTAINER &c, float AR, float CS,
                                                        float speedMultiplier, bool calculateStarsInaccurately,
                                                        const std::atomic<bool> &dead);
    bool loadMetadata(bool compute_md5 = true);

    static LOAD_GAMEPLAY_RESULT loadGameplay(DatabaseBeatmap *databaseBeatmap, AbstractBeatmapInterface *beatmap);
    inline LOAD_GAMEPLAY_RESULT loadGameplay(AbstractBeatmapInterface *beatmap) { return loadGameplay(this, beatmap); }

    MapOverrides get_overrides();
    void update_overrides();

    void setLocalOffset(i16 localOffset) {
        this->iLocalOffset = localOffset;
        this->update_overrides();
    }

    void setOnlineOffset(i16 onlineOffset) {
        this->iOnlineOffset = onlineOffset;
        this->update_overrides();
    }

    [[nodiscard]] inline const std::string &getFolder() const { return this->sFolder; }
    [[nodiscard]] inline const std::string &getFilePath() const { return this->sFilePath; }

    template <typename T = DatabaseBeatmap>
    [[nodiscard]] inline const std::vector<T *> &getDifficulties() const
        requires(std::is_same_v<std::remove_cv_t<T>, DatabaseBeatmap>)
    {
        static std::vector<T *> empty;
        return this->difficulties == nullptr ? empty : reinterpret_cast<const std::vector<T *> &>(*this->difficulties);
    }

    [[nodiscard]] inline const MD5Hash &getMD5Hash() const { return this->sMD5Hash; }

    [[nodiscard]] TIMING_INFO getTimingInfoForTime(u32 positionMS) const;
    static TIMING_INFO getTimingInfoForTimeAndTimingPoints(u32 positionMS,
                                                           const zarray<DatabaseBeatmap::TIMINGPOINT> &timingpoints);

    // raw metadata

    [[nodiscard]] inline int getVersion() const { return this->iVersion; }
    [[nodiscard]] inline int getGameMode() const { return this->iGameMode; }
    [[nodiscard]] inline int getID() const { return this->iID; }
    [[nodiscard]] inline int getSetID() const { return this->iSetID; }

    [[nodiscard]] inline const std::string &getTitle() const {
        if(!this->bEmptyTitleUnicode && osu->useCJKNames()) {
            return this->sTitleUnicode;
        } else {
            return this->sTitle;
        }
    }
    [[nodiscard]] inline const std::string &getTitleLatin() const { return this->sTitle; }
    [[nodiscard]] inline const std::string &getTitleUnicode() const { return this->sTitleUnicode; }

    [[nodiscard]] inline const std::string &getArtist() const {
        if(!this->bEmptyArtistUnicode && osu->useCJKNames()) {
            return this->sArtistUnicode;
        } else {
            return this->sArtist;
        }
    }
    [[nodiscard]] inline const std::string &getArtistLatin() const { return this->sArtist; }
    [[nodiscard]] inline const std::string &getArtistUnicode() const { return this->sArtistUnicode; }

    [[nodiscard]] inline const std::string &getCreator() const { return this->sCreator; }
    [[nodiscard]] inline const std::string &getDifficultyName() const { return this->sDifficultyName; }
    [[nodiscard]] inline const std::string &getSource() const { return this->sSource; }
    [[nodiscard]] inline const std::string &getTags() const { return this->sTags; }
    [[nodiscard]] inline const std::string &getBackgroundImageFileName() const {
        return this->sBackgroundImageFileName;
    }
    [[nodiscard]] inline const std::string &getAudioFileName() const { return this->sAudioFileName; }

    [[nodiscard]] inline u32 getLengthMS() const { return this->iLengthMS; }
    [[nodiscard]] inline int getPreviewTime() const { return this->iPreviewTime; }

    [[nodiscard]] inline float getAR() const { return this->fAR; }
    [[nodiscard]] inline float getCS() const { return this->fCS; }
    [[nodiscard]] inline float getHP() const { return this->fHP; }
    [[nodiscard]] inline float getOD() const { return this->fOD; }

    [[nodiscard]] inline float getStackLeniency() const { return this->fStackLeniency; }
    [[nodiscard]] inline float getSliderTickRate() const { return this->fSliderTickRate; }
    [[nodiscard]] inline float getSliderMultiplier() const { return this->fSliderMultiplier; }

    [[nodiscard]] inline const zarray<DatabaseBeatmap::TIMINGPOINT> &getTimingpoints() const {
        return this->timingpoints;
    }

    using MapFileReadDoneCallback = std::function<void(std::vector<u8>)>;  // == AsyncIOHandler::ReadCallback
    bool getMapFileAsync(MapFileReadDoneCallback data_callback);
    std::string getFullSoundFilePath();

    // redundant data
    [[nodiscard]] inline const std::string &getFullBackgroundImageFilePath() const {
        return this->sFullBackgroundImageFilePath;
    }

    // precomputed data

    [[nodiscard]] inline float getStarsNomod() const { return this->fStarsNomod; }

    [[nodiscard]] inline int getMinBPM() const { return this->iMinBPM; }
    [[nodiscard]] inline int getMaxBPM() const { return this->iMaxBPM; }
    [[nodiscard]] inline int getMostCommonBPM() const { return this->iMostCommonBPM; }

    [[nodiscard]] inline int getNumObjects() const { return this->iNumObjects; }
    [[nodiscard]] inline int getNumCircles() const { return this->iNumCircles; }
    [[nodiscard]] inline int getNumSliders() const { return this->iNumSliders; }
    [[nodiscard]] inline int getNumSpinners() const { return this->iNumSpinners; }

    // custom data

    i64 last_modification_time = 0;

    [[nodiscard]] inline i32 getLocalOffset() const { return this->iLocalOffset; }
    [[nodiscard]] inline i32 getOnlineOffset() const { return this->iOnlineOffset; }

    zarray<DatabaseBeatmap::TIMINGPOINT> timingpoints;  // necessary for main menu anim

    // redundant data (technically contained in metadata, but precomputed anyway)

    std::string sFolder;    // path to folder containing .osu file (e.g. "/path/to/beatmapfolder/")
    std::string sFilePath;  // path to .osu file (e.g. "/path/to/beatmapfolder/beatmap.osu")
    std::string sFullSoundFilePath;
    std::string sFullBackgroundImageFilePath;

    bool bEmptyArtistUnicode{false};
    bool bEmptyTitleUnicode{false};

    // raw metadata

    std::string sTitle;
    std::string sTitleUnicode;
    std::string sArtist;
    std::string sArtistUnicode;
    std::string sCreator;
    std::string sDifficultyName;  // difficulty name ("Version")
    std::string sSource;          // only used by search
    std::string sTags;            // only used by search
    std::string sBackgroundImageFileName;
    std::string sAudioFileName;

    int iID;  // online ID, if uploaded
    u32 iLengthMS;

    u8 iVersion;   // e.g. "osu file format v12" -> 12
    u8 iGameMode;  // 0 = osu!standard, 1 = Taiko, 2 = Catch the Beat, 3 = osu!mania
    int iSetID;    // online set ID, if uploaded

    int iPreviewTime;

    float fAR;
    float fCS;
    float fHP;
    float fOD;

    float fStackLeniency;
    float fSliderTickRate;
    float fSliderMultiplier;

    // precomputed data (can-run-without-but-nice-to-have data)

    float fStarsNomod;

    int iMinBPM = 0;
    int iMaxBPM = 0;
    int iMostCommonBPM = 0;

    int iNumObjects;
    int iNumCircles;
    int iNumSliders;
    int iNumSpinners;

    // custom data (not necessary, not part of the beatmap file, and not precomputed)
    std::atomic<f32> loudness = 0.f;

    i16 iLocalOffset;
    i16 iOnlineOffset;

    struct CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT {
        int errorCode;
    };

    // class internal data (custom)

    friend class Database;
    friend class BGImageHandler;

    static PRIMITIVE_CONTAINER loadPrimitiveObjects(std::string_view osuFilePath);
    static PRIMITIVE_CONTAINER loadPrimitiveObjects(std::string_view osuFilePath, const std::atomic<bool> &dead);
    static CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT calculateSliderTimesClicksTicks(
        int beatmapVersion, std::vector<SLIDER> &sliders, zarray<DatabaseBeatmap::TIMINGPOINT> &timingpoints,
        float sliderMultiplier, float sliderTickRate);
    static CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT calculateSliderTimesClicksTicks(
        int beatmapVersion, std::vector<SLIDER> &sliders, zarray<DatabaseBeatmap::TIMINGPOINT> &timingpoints,
        float sliderMultiplier, float sliderTickRate, const std::atomic<bool> &dead);

    BeatmapType type;

    MD5Hash sMD5Hash;

    bool draw_background = true;
    bool do_not_store = false;

   private:
    std::vector<DatabaseBeatmap *> *difficulties = nullptr;

    static bool parse_timing_point(std::string_view curLine, DatabaseBeatmap::TIMINGPOINT *out);

    enum class BlockId : i8 {
        Sentinel = -2,  // for skipping the first string scan, header must come first
        Header = -1,
        General = 0,
        Metadata = 1,
        Difficulty = 2,
        Events = 3,
        TimingPoints = 4,
        Colours = 5,
        HitObjects = 6,
    };

    struct MetadataBlock {
        std::string_view str;
        BlockId id;
    };

    static constexpr const std::initializer_list<MetadataBlock> metadataBlocks{
        {.str = "[General]", .id = BlockId::General},           {.str = "[Metadata]", .id = BlockId::Metadata},
        {.str = "[Difficulty]", .id = BlockId::Difficulty},     {.str = "[Events]", .id = BlockId::Events},
        {.str = "[TimingPoints]", .id = BlockId::TimingPoints}, {.str = "[Colours]", .id = BlockId::Colours},
        {.str = "[HitObjects]", .id = BlockId::HitObjects}};
};

struct BPMInfo {
    i32 min{0};
    i32 max{0};
    i32 most_common{0};
};

struct BPMTuple {
    i32 bpm;
    double duration;
};

template <typename T>
struct BPMInfo getBPM(const zarray<T> &timing_points, zarray<BPMTuple> &bpm_buffer) {
    if(timing_points.empty()) {
        return {};
    }

    bpm_buffer.clear();  // reuse existing buffer
    bpm_buffer.reserve(timing_points.size());

    double lastTime = timing_points[timing_points.size() - 1].offset;
    for(size_t i = 0; i < timing_points.size(); i++) {
        const T &t = timing_points[i];
        if(t.offset > lastTime) continue;
        if(t.msPerBeat <= 0.0) continue;

        // "osu-stable forced the first control point to start at 0."
        // "This is reproduced here to maintain compatibility around osu!mania scroll speed and song
        // select display."
        double currentTime = (i == 0 ? 0 : t.offset);
        double nextTime = (i == timing_points.size() - 1 ? lastTime : timing_points[i + 1].offset);

        i32 bpm = std::min(60000.0 / t.msPerBeat, 9001.0);
        double duration = std::max(nextTime - currentTime, 0.0);

        bool found = false;
        for(auto &tuple : bpm_buffer) {
            if(tuple.bpm == bpm) {
                tuple.duration += duration;
                found = true;
                break;
            }
        }

        if(!found) {
            bpm_buffer.push_back(BPMTuple{
                .bpm = bpm,
                .duration = duration,
            });
        }
    }

    i32 min = 9001;
    i32 max = 0;
    i32 mostCommonBPM = 0;
    double longestDuration = 0;
    for(const auto &tuple : bpm_buffer) {
        if(tuple.bpm > max) max = tuple.bpm;
        if(tuple.bpm < min) min = tuple.bpm;
        if(tuple.duration > longestDuration || (tuple.duration == longestDuration && tuple.bpm > mostCommonBPM)) {
            longestDuration = tuple.duration;
            mostCommonBPM = tuple.bpm;
        }
    }
    if(min > max) min = max;

    return BPMInfo{
        .min = min,
        .max = max,
        .most_common = mostCommonBPM,
    };
}
