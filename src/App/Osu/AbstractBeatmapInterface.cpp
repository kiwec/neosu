#include "AbstractBeatmapInterface.h"

#include "Osu.h"
#include "GameRules.h"
#include "LegacyReplay.h"

f32 AbstractBeatmapInterface::getHitWindow300() const {
    return GameRules::mapDifficultyRange(this->getOD(), GameRules::getMinHitWindow300(),
                                         GameRules::getMidHitWindow300(), GameRules::getMaxHitWindow300());
}

f32 AbstractBeatmapInterface::getRawHitWindow300() const {
    return GameRules::mapDifficultyRange(this->getRawOD(), GameRules::getMinHitWindow300(),
                                         GameRules::getMidHitWindow300(), GameRules::getMaxHitWindow300());
}

f32 AbstractBeatmapInterface::getHitWindow100() const {
    return GameRules::mapDifficultyRange(this->getOD(), GameRules::getMinHitWindow100(),
                                         GameRules::getMidHitWindow100(), GameRules::getMaxHitWindow100());
}

f32 AbstractBeatmapInterface::getHitWindow50() const {
    return GameRules::mapDifficultyRange(this->getOD(), GameRules::getMinHitWindow50(), GameRules::getMidHitWindow50(),
                                         GameRules::getMaxHitWindow50());
}

f32 AbstractBeatmapInterface::getApproachRateForSpeedMultiplier() const {
    return GameRules::mapDifficultyRangeInv((f32)this->getApproachTime() * (1.0f / this->getSpeedMultiplier()),
                                            GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),
                                            GameRules::getMaxApproachTime());
}

f32 AbstractBeatmapInterface::getRawARForSpeedMultiplier() const {
    return GameRules::mapDifficultyRangeInv((f32)this->getRawApproachTime() * (1.0f / this->getSpeedMultiplier()),
                                            GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),
                                            GameRules::getMaxApproachTime());
}

f32 AbstractBeatmapInterface::getConstantApproachRateForSpeedMultiplier() const {
    return GameRules::mapDifficultyRangeInv((f32)this->getRawApproachTime() * this->getSpeedMultiplier(),
                                            GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),
                                            GameRules::getMaxApproachTime());
}

f32 AbstractBeatmapInterface::getOverallDifficultyForSpeedMultiplier() const {
    return GameRules::mapDifficultyRangeInv((f32)this->getHitWindow300() * (1.0f / this->getSpeedMultiplier()),
                                            GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(),
                                            GameRules::getMaxHitWindow300());
}

f32 AbstractBeatmapInterface::getRawODForSpeedMultiplier() const {
    return GameRules::mapDifficultyRangeInv((f32)this->getRawHitWindow300() * (1.0f / this->getSpeedMultiplier()),
                                            GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(),
                                            GameRules::getMaxHitWindow300());
}

f32 AbstractBeatmapInterface::getConstantOverallDifficultyForSpeedMultiplier() const {
    return GameRules::mapDifficultyRangeInv((f32)this->getRawHitWindow300() * this->getSpeedMultiplier(),
                                            GameRules::getMinHitWindow300(), GameRules::getMidHitWindow300(),
                                            GameRules::getMaxHitWindow300());
}

const Replay::Mods &AbstractBeatmapInterface::getMods() const { return osu->getScore()->mods; }
LegacyFlags AbstractBeatmapInterface::getModsLegacy() const { return osu->getScore()->getModsLegacy(); }

LiveScore::HIT AbstractBeatmapInterface::getHitResult(i32 delta) const {
    // "stable-like" hit windows, see https://github.com/ppy/osu/pull/33882
    f32 window300 = std::floor(this->getHitWindow300()) - 0.5f;
    f32 window100 = std::floor(this->getHitWindow100()) - 0.5f;
    f32 window50 = std::floor(this->getHitWindow50()) - 0.5f;
    f32 windowMiss = std::floor(GameRules::getHitWindowMiss()) - 0.5f;
    f32 fDelta = std::abs((f32)delta);

    // We are 400ms away from the hitobject, don't count this as a miss
    if(fDelta > windowMiss) {
        return LiveScore::HIT::HIT_NULL;
    }

    const auto modFlags = this->getMods().flags;

    // mod_halfwindow only allows early hits
    // mod_halfwindow_allow_300s also allows "late" perfect hits
    if(flags::has<ModFlags::HalfWindow>(modFlags) && delta > 0) {
        if(fDelta > window300 || !flags::has<ModFlags::HalfWindowAllow300s>(modFlags)) {
            return LiveScore::HIT::HIT_MISS;
        }
    }

    if(fDelta < window300) return LiveScore::HIT::HIT_300;
    if(fDelta < window100 && !(flags::has<ModFlags::No100s>(modFlags) || flags::has<ModFlags::Ming3012>(modFlags)))
        return LiveScore::HIT::HIT_100;
    if(fDelta < window50 && !(flags::has<ModFlags::No100s>(modFlags) || flags::has<ModFlags::No50s>(modFlags)))
        return LiveScore::HIT::HIT_50;
    return LiveScore::HIT::HIT_MISS;
}

bool AbstractBeatmapInterface::isClickHeld() const { return this->getKeys() & ~LegacyReplay::Smoke; }
