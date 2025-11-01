#pragma once
#include "Replay.h"
#include "score.h"
#include "Engine.h"

class HitObject;

#define CACHED_VIRTUAL_METHODS                   \
    X(u32, getScoreV1DifficultyMultiplier, 0.01) \
    X(f32, getRawAR, 0.01)                       \
    X(f32, getRawOD, 0.01)                       \
    X(f32, getAR, 0.01)                          \
    X(f32, getCS, 0.01)                          \
    X(f32, getHP, 0.01)                          \
    X(f32, getOD, 0.01)                          \
    X(f32, getApproachTime, 0.01)                \
    X(f32, getRawApproachTime, 0.01)

#define CACHED_BASE_METHODS                                                                                          \
    X(f32, getHitWindow300, 0.01,                                                                                    \
      GameRules::mapDifficultyRange(this->getOD(), GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(), \
                                    GameRules::getMaxHitWindow300()))                                                \
    X(f32, getRawHitWindow300, 0.01,                                                                                 \
      GameRules::mapDifficultyRange(this->getRawOD(), GameRules::getMinHitWindow300(),                               \
                                    GameRules::getMidHitWindow300(), GameRules::getMaxHitWindow300()))               \
    X(f32, getHitWindow100, 0.01,                                                                                    \
      GameRules::mapDifficultyRange(this->getOD(), GameRules::getMinHitWindow100(), GameRules::getMidHitWindow100(), \
                                    GameRules::getMaxHitWindow100()))                                                \
    X(f32, getHitWindow50, 0.01,                                                                                     \
      GameRules::mapDifficultyRange(this->getOD(), GameRules::getMinHitWindow50(), GameRules::getMidHitWindow50(),   \
                                    GameRules::getMaxHitWindow50()))                                                 \
    X(f32, getApproachRateForSpeedMultiplier, 0.01,                                                                  \
      GameRules::mapDifficultyRangeInv((f32)this->getApproachTime() * (1.0f / this->getSpeedMultiplier()),           \
                                       GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),             \
                                       GameRules::getMaxApproachTime()))                                             \
    X(f32, getRawARForSpeedMultiplier, 0.01,                                                                         \
      GameRules::mapDifficultyRangeInv((f32)this->getRawApproachTime() * (1.0f / this->getSpeedMultiplier()),        \
                                       GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),             \
                                       GameRules::getMaxApproachTime()))                                             \
    X(f32, getConstantApproachRateForSpeedMultiplier, 0.01,                                                          \
      GameRules::mapDifficultyRangeInv((f32)this->getRawApproachTime() * this->getSpeedMultiplier(),                 \
                                       GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),             \
                                       GameRules::getMaxApproachTime()))                                             \
    X(f32, getOverallDifficultyForSpeedMultiplier, 0.01,                                                             \
      GameRules::mapDifficultyRangeInv((f32)this->getHitWindow300() * (1.0f / this->getSpeedMultiplier()),           \
                                       GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(),             \
                                       GameRules::getMaxHitWindow300()))                                             \
    X(f32, getRawODForSpeedMultiplier, 0.01,                                                                         \
      GameRules::mapDifficultyRangeInv((f32)this->getRawHitWindow300() * (1.0f / this->getSpeedMultiplier()),        \
                                       GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(),             \
                                       GameRules::getMaxHitWindow300()))                                             \
    X(f32, getConstantOverallDifficultyForSpeedMultiplier, 0.01,                                                     \
      GameRules::mapDifficultyRangeInv((f32)this->getRawHitWindow300() * this->getSpeedMultiplier(),                 \
                                       GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(),             \
                                       GameRules::getMaxHitWindow300()))

// either simulated or actual
class AbstractBeatmapInterface {
    NOCOPY_NOMOVE(AbstractBeatmapInterface)
   public:
    AbstractBeatmapInterface() = default;
    virtual ~AbstractBeatmapInterface() = default;

    virtual LiveScore::HIT addHitResult(HitObject *hitObject, LiveScore::HIT hit, i32 delta, bool isEndOfCombo = false,
                                        bool ignoreOnHitErrorBar = false, bool hitErrorBarOnly = false,
                                        bool ignoreCombo = false, bool ignoreScore = false,
                                        bool ignoreHealth = false) = 0;

    [[nodiscard]] virtual u32 getBreakDurationTotal() const = 0;
    [[nodiscard]] virtual u8 getKeys() const = 0;
    [[nodiscard]] virtual u32 getLength() const = 0;
    [[nodiscard]] virtual u32 getLengthPlayable() const = 0;
    [[nodiscard]] virtual bool isContinueScheduled() const = 0;
    [[nodiscard]] virtual bool isPaused() const = 0;
    [[nodiscard]] virtual bool isPlaying() const = 0;
    [[nodiscard]] virtual bool isWaiting() const = 0;
    [[nodiscard]] virtual f32 getSpeedMultiplier() const = 0;

    virtual void addScorePoints(int points, bool isSpinner = false) = 0;
    virtual void addSliderBreak() = 0;

#define X(rettype, methodname, refresh_time) \
    [[nodiscard]] inline rettype methodname() const { return methodname##_cached(); }
    CACHED_VIRTUAL_METHODS
#undef X

    [[nodiscard]] virtual const Replay::Mods &getMods() const; // overridden by SimulatedBeatmapInterface
    [[nodiscard]] virtual LegacyFlags getModsLegacy() const; // overridden by SimulatedBeatmapInterface

    [[nodiscard]] virtual vec2 getCursorPos() const = 0;

    [[nodiscard]] virtual vec2 pixels2OsuCoords(vec2 pixelCoords) const = 0;
    [[nodiscard]] virtual vec2 osuCoords2Pixels(vec2 coords) const = 0;
    [[nodiscard]] virtual vec2 osuCoords2RawPixels(vec2 coords) const = 0;
    [[nodiscard]] virtual vec2 osuCoords2LegacyPixels(vec2 coords) const = 0;

    f64 fHpMultiplierComboEnd = 1.0;
    f64 fHpMultiplierNormal = 1.0;
    u32 iMaxPossibleCombo = 0;
    u32 iScoreV2ComboPortionMaximum = 0;

    // It is assumed these values are set correctly
    u32 nb_hitobjects = 0;
    f32 fHitcircleDiameter = 0.f;
    f32 fRawHitcircleDiameter = 0.f;
    f32 fSliderFollowCircleDiameter = 0.f;
    u8 lastPressedKey = 0;
    bool holding_slider = false;

    // Generic behavior below, do not override
    [[nodiscard]] bool isClickHeld();
    LiveScore::HIT getHitResult(i32 delta);  // can't really be cached

#define X(rettype, methodname, refresh_time, impl) /* clang-format please stop messing up my formatting thanks */ \
    [[nodiscard]] rettype methodname() const;
    CACHED_BASE_METHODS
#undef X

   private:
    // cache for expensive accesors
#define CACHED_METHOD_IMPL(rettype, refresh_time, value_expr)                    \
    static_assert((double)(refresh_time) > 0 && (double)(refresh_time) <= 3600); \
    static rettype cached_value{};                                               \
    static double cache_time{-(refresh_time)};                                   \
    const auto now = engine->getTime();                                          \
    if(now >= cache_time + (refresh_time)) {                                     \
        cache_time = now;                                                        \
        cached_value = (value_expr);                                             \
    }                                                                            \
    return cached_value;

    // the methodname_full() is a pure virtual function and should be overridden in derived classes
    // doing it this way to avoid needing to change calling code
#define X(rettype, methodname, refresh_time)                           \
    [[nodiscard]] virtual rettype methodname##_full() const = 0;       \
    [[nodiscard]] forceinline rettype methodname##_cached() const {    \
        CACHED_METHOD_IMPL(rettype, refresh_time, methodname##_full()) \
    }
    CACHED_VIRTUAL_METHODS
#undef X
};

#undef CACHED_VIRTUAL_METHODS
