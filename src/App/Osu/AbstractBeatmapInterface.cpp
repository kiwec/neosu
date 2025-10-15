#include "AbstractBeatmapInterface.h"

#include "GameRules.h"
#include "LegacyReplay.h"

#define X(rettype, methodname, refresh_time, impl) \
    rettype AbstractBeatmapInterface::methodname() const { CACHED_METHOD_IMPL(rettype, refresh_time, impl) }
CACHED_BASE_METHODS
#undef X

#undef CACHED_BASE_METHODS
#undef CACHED_METHOD_IMPL

LiveScore::HIT AbstractBeatmapInterface::getHitResult(i32 delta) {
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

    // mod_halfwindow only allows early hits
    // mod_halfwindow_allow_300s also allows "late" perfect hits
    if(this->getMods().has(ModFlags::HalfWindow) && delta > 0) {
        if(fDelta > window300 || !this->getMods().has(ModFlags::HalfWindowAllow300s)) {
            return LiveScore::HIT::HIT_MISS;
        }
    }

    if(fDelta < window300) return LiveScore::HIT::HIT_300;
    if(fDelta < window100 && !(this->getMods().has(ModFlags::No100s) || this->getMods().has(ModFlags::Ming3012)))
        return LiveScore::HIT::HIT_100;
    if(fDelta < window50 && !(this->getMods().has(ModFlags::No100s) || this->getMods().has(ModFlags::No50s)))
        return LiveScore::HIT::HIT_50;
    return LiveScore::HIT::HIT_MISS;
}

bool AbstractBeatmapInterface::isClickHeld() {
    return this->getKeys() & (LegacyReplay::K1 | LegacyReplay::K2 | LegacyReplay::M1 | LegacyReplay::M2);
}
