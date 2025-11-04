#pragma once
// Copyright (c) 2019, PG & Francesco149, All rights reserved.

#include "BaseEnvironment.h"
#include "types.h"
#include "Replay.h"
#include "Vectors.h"

#include <atomic>
#include <vector>

class SliderCurve;
class ConVar;
class AbstractBeatmapInterface;

struct pp_info {
    f64 total_stars = 0.0;
    f64 aim_stars = 0.0;
    f64 aim_slider_factor = 0.0;
    f64 speed_stars = 0.0;
    f64 speed_notes = 0.0;
    f64 difficult_aim_sliders = 0.0;
    f64 difficult_aim_strains = 0.0;
    f64 difficult_speed_strains = 0.0;
    f64 pp = -1.0;

    bool operator==(const pp_info &) const = default;
};

class OsuDifficultyHitObject {
   public:
    enum class TYPE : u8 {
        INVALID = 0,
        CIRCLE,
        SPINNER,
        SLIDER,
    };

    struct SLIDER_SCORING_TIME {
        enum class TYPE : u8 {
            TICK,
            REPEAT,
            END,
        };

        TYPE type;
        f32 time;
    };

    static inline bool sliderScoringTimeComparator(const SLIDER_SCORING_TIME &a, const SLIDER_SCORING_TIME &b) {
        if(a.time != b.time) return a.time < b.time;
        if(a.type != b.type) return static_cast<i32>(a.type) < static_cast<i32>(b.type);
        return false;  // equivalent
    };

   public:
    OsuDifficultyHitObject(TYPE type, vec2 pos, i32 time);               // circle
    OsuDifficultyHitObject(TYPE type, vec2 pos, i32 time, i32 endTime);  // spinner
    OsuDifficultyHitObject(TYPE type, vec2 pos, i32 time, i32 endTime, f32 spanDuration, i8 osuSliderCurveType,
                           const std::vector<vec2> &controlPoints, f32 pixelLength,
                           std::vector<SLIDER_SCORING_TIME> scoringTimes, i32 repeats,
                           bool calculateSliderCurveInConstructor);  // slider
    ~OsuDifficultyHitObject();

    OsuDifficultyHitObject(const OsuDifficultyHitObject &) = delete;
    OsuDifficultyHitObject(OsuDifficultyHitObject &&dobj) noexcept;

    OsuDifficultyHitObject &operator=(OsuDifficultyHitObject &&dobj) noexcept;

    void updateStackPosition(f32 stackOffset);
    void updateCurveStackPosition(f32 stackOffset);

    // for stacking calculations, always returns the unstacked original position at that point in time
    [[nodiscard]] vec2 getOriginalRawPosAt(i32 pos) const;
    [[nodiscard]] f32 getT(i32 pos, bool raw) const;

    [[nodiscard]] inline i32 getDuration() const { return endTime - time; }

   public:
    // circles (base)
    TYPE type;
    vec2 pos;
    i32 time;

    // spinners + sliders
    i32 endTime;

    // sliders
    f32 spanDuration;  // i.e. sliderTimeWithoutRepeats
    i8 osuSliderCurveType;
    f32 pixelLength;
    std::vector<SLIDER_SCORING_TIME> scoringTimes;
    i32 repeats;

    // custom
    SliderCurve *curve;
    bool scheduledCurveAlloc;
    std::vector<vec2> scheduledCurveAllocControlPoints;
    f32 scheduledCurveAllocStackOffset;

    i32 stack;
    vec2 originalPos;
};

class DifficultyCalculator {
   public:
    static constexpr const i32 PP_ALGORITHM_VERSION = 20250306;

   public:
    struct Skills {
        static constexpr const i32 NUM_SKILLS = 3;
        enum Skill : u8 { SPEED, AIM_SLIDERS, AIM_NO_SLIDERS };
    };

    // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
    // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Aim.cs

    // how much strains decay per interval (if the previous interval's peak strains after applying decay are still higher than the current one's, they will be used as the peak strains).
    static constexpr const f64 decay_base[Skills::NUM_SKILLS] = {0.3, 0.15, 0.15};
    // used to keep speed and aim balanced between eachother
    static constexpr const f64 weight_scaling[Skills::NUM_SKILLS] = {1.46, 25.6, 25.6};

    static constexpr const f64 DIFFCALC_EPSILON = 1e-32;

    struct IncrementalState {
        f64 interval_end;
        f64 max_strain;
        f64 max_object_strain;
        f64 relevant_note_sum;  // speed only
        f64 consistent_top_strain;
        f64 difficult_strains;
        f64 max_slider_strain;
        f64 difficult_sliders;
        std::vector<f64> highest_strains;
        std::vector<f64> slider_strains;
    };

    struct DiffObject {
        DiffObject(OsuDifficultyHitObject *base_object, f32 radius_scaling_factor,
                   std::vector<DiffObject> &diff_objects, i32 prevObjectIdx)
            : ho(base_object),
              norm_start(ho->pos * radius_scaling_factor),
              lazyEndPos(ho->pos),
              prevObjectIndex(prevObjectIdx),
              objects(diff_objects) {}

        std::array<f64, Skills::NUM_SKILLS> strains{};

        OsuDifficultyHitObject *ho;

        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
        // needed because raw speed strain and rhythm strain are combined in different ways
        f64 raw_speed_strain{0.};
        f64 rhythm{0.};

        vec2 norm_start;  // start position normalized on radius

        f64 angle{std::numeric_limits<f64>::quiet_NaN()};  // precalc

        f64 jumpDistance{0.};     // precalc
        f64 minJumpDistance{0.};  // precalc
        f64 minJumpTime{0.};      // precalc
        f64 travelDistance{0.};   // precalc

        f64 delta_time{0.};   // strain temp
        f64 strain_time{0.};  // strain temp

        vec2 lazyEndPos;               // precalc temp
        f64 lazyTravelDist{0.};        // precalc temp
        f64 lazyTravelTime{0.};        // precalc temp
        f64 travelTime{0.};            // precalc temp
        bool lazyCalcFinished{false};  // precalc temp

        i32 prevObjectIndex;  // WARNING: this will be -1 for the first object (as the name implies), see note above

        // NOTE: McOsu stores the first object in this array while lazer doesn't. newer lazer algorithms require referencing objects "randomly", so we just keep the entire vector around.
        const std::vector<DiffObject> &objects;

        [[nodiscard]] inline const DiffObject *get_previous(i32 backwardsIdx) const {
            return (objects.size() > 0 && prevObjectIndex - backwardsIdx < (i32)objects.size()
                        ? &objects[std::max(0, prevObjectIndex - backwardsIdx)]
                        : nullptr);
        }
        [[nodiscard]] inline f64 get_strain(Skills::Skill type) const {
            return strains[type] * (type == Skills::SPEED ? rhythm : 1.0);
        }
        [[nodiscard]] inline f64 get_slider_aim_strain() const {
            return ho->type == OsuDifficultyHitObject::TYPE::SLIDER ? strains[Skills::AIM_SLIDERS] : -1.0;
        }
        inline static f64 applyDiminishingExp(f64 val) { return std::pow(val, 0.99); }
        inline static f64 strainDecay(Skills::Skill type, f64 ms) { return std::pow(decay_base[type], ms / 1000.0); }

        void calculate_strains(const DiffObject &prev, const DiffObject *next, f64 hitWindow300);
        void calculate_strain(const DiffObject &prev, const DiffObject *next, f64 hitWindow300,
                              const Skills::Skill dtype);
        static f64 calculate_difficulty(const Skills::Skill type, const DiffObject *dobjects, size_t dobjectCount,
                                        IncrementalState *incremental, std::vector<f64> *outStrains = nullptr,
                                        f64 *outDifficultStrains = nullptr, f64 *outSkillSpecificAttrib = nullptr);
        f64 spacing_weight2(const Skills::Skill diff_type, const DiffObject &prev, const DiffObject *next,
                            f64 hitWindow300);
        f64 get_doubletapness(const DiffObject *next, f64 hitWindow300) const;
    };

   public:
    struct StarCalcParams {
        std::vector<DiffObject> cachedDiffObjects;
        std::vector<OsuDifficultyHitObject> &sortedHitObjects;

        f32 CS{};
        f32 OD{};
        f32 speedMultiplier{1};
        bool relax{false};
        bool touchDevice{false};
        f64 *aim{};
        f64 *aimSliderFactor{};

        f64 *aimDifficultSliders{};
        f64 *difficultAimStrains{};
        f64 *speed{};
        f64 *speedNotes{};
        f64 *difficultSpeedStrains{};

        i32 upToObjectIndex{-1};
        IncrementalState *incremental{nullptr};

        std::vector<f64> *outAimStrains{nullptr};
        std::vector<f64> *outSpeedStrains{nullptr};

        const std::atomic<bool> &dead;
    };

    // stars, fully static
    static f64 calculateStarDiffForHitObjects(StarCalcParams &params);

    // pp, fully static
    static f64 calculatePPv2(const Replay::Mods &mods, f64 ar, f64 od, f64 aim, f64 aimSliderFactor,
                             f64 aimDifficultSliders, f64 difficultAimStrains, f64 speed, f64 speedNotes,
                             f64 difficultSpeedStrains, i32 numHitObjects, i32 numCircles, i32 numSliders,
                             i32 numSpinners, i32 maxPossibleCombo, i32 combo, i32 misses, i32 c300, i32 c100, i32 c50);

    // helper functions
    static f64 calculateTotalStarsFromSkills(f64 aim, f64 speed);

   private:
    struct Attributes {
        f64 AimStrain;
        f64 SliderFactor;
        f64 AimDifficultSliderCount;
        f64 AimDifficultStrainCount;
        f64 SpeedStrain;
        f64 SpeedNoteCount;
        f64 SpeedDifficultStrainCount;
        f64 ApproachRate;
        f64 OverallDifficulty;
        i32 SliderCount;
    };

    struct ScoreData {
        Replay::Mods mods;
        f64 accuracy;
        i32 countGreat;
        i32 countGood;
        i32 countMeh;
        i32 countMiss;
        i32 totalHits;
        i32 totalSuccessfulHits;
        i32 beatmapMaxCombo;
        i32 scoreMaxCombo;
        i32 amountHitObjectsWithAccuracy;
    };

    struct RhythmIsland {
        // NOTE: lazer stores "deltaDifferenceEpsilon" (hitWindow300 * 0.3) in this struct, but OD is constant here
        i32 delta;
        i32 deltaCount;

        inline bool equals(RhythmIsland &other, f64 deltaDifferenceEpsilon) const {
            return std::abs(delta - other.delta) < deltaDifferenceEpsilon && deltaCount == other.deltaCount;
        }
    };

   private:
    static f64 computeAimValue(const ScoreData &score, const Attributes &attributes, f64 effectiveMissCount);
    static f64 computeSpeedValue(const ScoreData &score, const Attributes &attributes, f64 effectiveMissCount,
                                 f64 speedDeviation);
    static f64 computeAccuracyValue(const ScoreData &score, const Attributes &attributes);

    static f64 calculateSpeedDeviation(const ScoreData &score, const Attributes &attributes);
    static f64 calculateDeviation(const Attributes &attributes, f64 timescale, f64 relevantCountGreat,
                                  f64 relevantCountOk, f64 relevantCountMeh, f64 relevantCountMiss);
    static f64 calculateSpeedHighDeviationNerf(const Attributes &attributes, f64 speedDeviation);

    static f64 erf(f64 x);
    static f64 erfInv(f64 z);
    static f64 erfImp(f64 x, bool invert);
    static f64 erfInvImp(f64 p, f64 q, f64 s);

    template <size_t N>
    static forceinline f64 evaluatePolynomial(f64 z, const f64 (&coefficients)[N]) {
        f64 sum = coefficients[N - 1];
        for(i32 i = N - 2; i >= 0; --i) {
            sum *= z;
            sum += coefficients[i];
        }
        return sum;
    }
};
