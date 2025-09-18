#pragma once
// Copyright (c) 2019, PG & Francesco149, All rights reserved.

#include "types.h"
#include "Vectors.h"

#include <atomic>
#include <vector>

class SliderCurve;
class ConVar;

struct pp_info {
    f64 total_stars = 0.0;
    f64 aim_stars = 0.0;
    f64 aim_slider_factor = 0.0;
    f64 speed_stars = 0.0;
    f64 speed_notes = 0.0;
    f64 difficult_aim_strains = 0.0;
    f64 difficult_speed_strains = 0.0;
    f64 pp = -1.0;
};

class OsuDifficultyHitObject {
   public:
    enum class TYPE : i8 {
        INVALID = 0,
        CIRCLE,
        SPINNER,
        SLIDER,
    };

    struct SLIDER_SCORING_TIME {
        enum class TYPE : uint8_t {
            TICK,
            REPEAT,
            END,
        };

        TYPE type;
        f32 time;
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
    OsuDifficultyHitObject &operator=(const OsuDifficultyHitObject &) = delete;

    OsuDifficultyHitObject(OsuDifficultyHitObject &&dobj) noexcept;
    OsuDifficultyHitObject &operator=(OsuDifficultyHitObject &&dobj) noexcept;

    void updateStackPosition(f32 stackOffset);
    void updateCurveStackPosition(f32 stackOffset);

    // for stacking calculations, always returns the unstacked original position at that point in time
    vec2 getOriginalRawPosAt(i32 pos);

    f32 getT(i32 pos, bool raw);

    [[nodiscard]] inline i32 getDuration() const { return this->endTime - this->time; }

    // circles (base)
    TYPE type{};
    vec2 pos{0.f};
    i32 time{};

    // spinners + sliders
    i32 endTime{};

    // sliders
    f32 spanDuration{};  // i.e. sliderTimeWithoutRepeats
    i8 osuSliderCurveType{};
    f32 pixelLength{};
    std::vector<SLIDER_SCORING_TIME> scoringTimes{};
    i32 repeats{};

    // custom
    SliderCurve *curve{nullptr};
    bool scheduledCurveAlloc{false};
    std::vector<vec2> scheduledCurveAllocControlPoints;
    f32 scheduledCurveAllocStackOffset{};

    i32 stack{};
    vec2 originalPos{0.f};
};

class DifficultyCalculator {
   public:
    static constexpr const i32 PP_ALGORITHM_VERSION = 20241007;

    class Skills {
       public:
        static constexpr const int NUM_SKILLS = 3;

       public:
        enum class Skill : uint8_t {
            SPEED,
            AIM_SLIDERS,
            AIM_NO_SLIDERS,
        };

        static int skillToIndex(const Skill skill) {
            switch(skill) {
                case Skill::SPEED:
                    return 0;
                case Skill::AIM_SLIDERS:
                    return 1;
                case Skill::AIM_NO_SLIDERS:
                    return 2;
            }

            return 0;
        }
    };

    // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
    // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Aim.cs

    // how much strains decay per interval (if the previous interval's peak strains after applying decay are
    // still higher than the current one's, they will be used as the peak strains).
    static constexpr const f64 decay_base[Skills::NUM_SKILLS] = {0.3, 0.15, 0.15};

    // used to keep speed and aim balanced between eachother
    static constexpr const f64 weight_scaling[Skills::NUM_SKILLS] = {1.430, 25.18, 25.18};

    static constexpr const f64 DIFFCALC_EPSILON = 1e-32;

    struct IncrementalState {
        f64 interval_end;
        f64 max_strain;
        f64 max_object_strain;
        f64 relevant_note_sum;  // speed only
        f64 consistent_top_strain;
        f64 difficult_strains;
        std::vector<f64> highest_strains;
    };

    class DiffObject {
       public:
        OsuDifficultyHitObject *ho;

        double strains[Skills::NUM_SKILLS];

        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
        // needed because raw speed strain and rhythm strain are combined in different ways
        double raw_speed_strain;
        double rhythm;

        vec2 norm_start{0.f};  // start position normalized on radius

        double angle;  // precalc

        double jumpDistance;     // precalc
        double minJumpDistance;  // precalc
        double minJumpTime;      // precalc
        double travelDistance;   // precalc

        double delta_time;   // strain temp
        double strain_time;  // strain temp

        bool lazyCalcFinished;  // precalc temp
        vec2 lazyEndPos{0.f};   // precalc temp
        double lazyTravelDist;  // precalc temp
        double lazyTravelTime;  // precalc temp
        double travelTime;      // precalc temp

        // NOTE(McKay): McOsu stores the first object in this array while lazer doesn't. newer lazer algorithms
        // require referencing objects "randomly", so we just keep the entire vector around.
        const std::vector<DiffObject> &objects;

        // WARNING: this will be -1 for the first object (as the name implies), see note above
        int prevObjectIndex;

        DiffObject(OsuDifficultyHitObject *base_object, float radius_scaling_factor,
                   std::vector<DiffObject> &diff_objects, int prevObjectIdx);

        [[nodiscard]] inline const DiffObject *get_previous(int backwardsIdx) const {
            int foo = this->prevObjectIndex - backwardsIdx;
            if(foo < 0) foo = 0;  // msvc
            return (this->objects.size() > 0 && this->prevObjectIndex - backwardsIdx < (int)this->objects.size()
                        ? &this->objects[foo]
                        : nullptr);
        }
        [[nodiscard]] inline f64 get_strain(Skills::Skill type) const {
            return this->strains[Skills::skillToIndex(type)] * (type == Skills::Skill::SPEED ? this->rhythm : 1.0);
        }
        inline static double applyDiminishingExp(double val) { return std::pow(val, 0.99); }
        inline static double strainDecay(Skills::Skill type, double ms) {
            return std::pow(decay_base[Skills::skillToIndex(type)], ms / 1000.0);
        }

        void calculate_strains(const DiffObject &prev, const DiffObject *next, double hitWindow300);
        void calculate_strain(const DiffObject &prev, const DiffObject *next, double hitWindow300,
                              const Skills::Skill dtype);
        static f64 calculate_difficulty(const Skills::Skill type, const DiffObject *dobjects, size_t dobjectCount,
                                        IncrementalState *incremental, std::vector<f64> *outStrains = nullptr,
                                        f64 *outDifficultStrains = nullptr, f64 *outRelevantNotes = nullptr);
        static double spacing_weight1(const double distance, const Skills::Skill diff_type);
        double spacing_weight2(const Skills::Skill diff_type, const DiffObject &prev, const DiffObject *next,
                               double hitWindow300);
        f64 get_doubletapness(const DiffObject *next, f64 hitWindow300) const;
    };

    struct StarCalcParams {
        std::vector<OsuDifficultyHitObject> sortedHitObjects;
        f32 CS;
        f32 OD;
        f32 speedMultiplier;
        bool relax;
        bool touchDevice;
        f64 *aim;
        f64 *aimSliderFactor;
        f64 *difficultAimStrains;
        f64 *speed;
        f64 *speedNotes;
        f64 *difficultSpeedStrains;
        i32 upToObjectIndex = -1;
        std::vector<f64> *outAimStrains = nullptr;
        std::vector<f64> *outSpeedStrains = nullptr;
    };

    struct RhythmIsland {
        // NOTE: lazer stores "deltaDifferenceEpsilon" (hitWindow300 * 0.3) in this struct, but OD is constant here
        i32 delta;
        i32 deltaCount;

        inline bool equals(RhythmIsland &other, f64 deltaDifferenceEpsilon) const {
            return std::abs(this->delta - other.delta) < deltaDifferenceEpsilon && this->deltaCount == other.deltaCount;
        }
    };

    // stars, fully static
    static f64 calculateStarDiffForHitObjects(StarCalcParams &params);
    static f64 calculateStarDiffForHitObjects(StarCalcParams &params, const std::atomic<bool> &dead);
    static f64 calculateStarDiffForHitObjectsInt(std::vector<DiffObject> &cachedDiffObjects, StarCalcParams &params,
                                                 IncrementalState *incremental, const std::atomic<bool> &dead);

    // pp, fully static
    static f64 calculatePPv2(u32 modsLegacy, f64 timescale, f64 ar, f64 od, f64 aim, f64 aimSliderFactor,
                             f64 aimDifficultStrains, f64 speed, f64 speedNotes, f64 speedDifficultStrains,
                             i32 numCircles, i32 numSliders, i32 numSpinners, i32 maxPossibleCombo, i32 combo,
                             i32 misses, i32 c300, i32 c100, i32 c50);
};
