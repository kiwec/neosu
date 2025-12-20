// Copyright (c) 2024, kiwec, All rights reserved.
#include "ScoreConverterThread.h"

#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Osu.h"
#include "DifficultyCalculator.h"
#include "SimulatedBeatmapInterface.h"
#include "score.h"
#include "Timing.h"
#include "Logging.h"
#include "Thread.h"
#include "SyncJthread.h"
#include "SyncStoptoken.h"

#include <atomic>
#include <memory>

namespace ScoreConverter {
namespace {
// TODO
constexpr bool USE_PPV3{false};

std::atomic<u32> sct_computed{0};
std::atomic<u32> sct_total{0};

Sync::jthread thr;
}  // namespace

// for Database to do friend struct ScoreConverter::internal
struct internal {
    static void update_ppv2(const FinishedScore& score, const Sync::stop_token& stoken);
    static bool score_needs_recalc(const FinishedScore& score);
    static void runloop(const Sync::stop_token& stoken);
};

// XXX: This is barebones, no caching, *hopefully* fast enough (worst part is loading the .osu files)
// XXX: Probably code duplicated a lot, I'm pretty sure there's 4 places where I calc ppv2...
void internal::update_ppv2(const FinishedScore& score, const Sync::stop_token& stoken) {
    if(score.ppv2_version >= DiffCalc::PP_ALGORITHM_VERSION) return;

    const auto* map = db->getBeatmapDifficulty(score.beatmap_hash);
    if(!map) return;

    const f32 AR = score.mods.get_naive_ar(map);
    const f32 HP = score.mods.get_naive_hp(map);
    const f32 CS = score.mods.get_naive_cs(map);
    const f32 OD = score.mods.get_naive_od(map);
    const bool HD = score.mods.has(ModFlags::Hidden);
    const bool RX = score.mods.has(ModFlags::Relax);
    const bool TD = score.mods.has(ModFlags::TouchDevice);
    const bool AP = score.mods.has(ModFlags::Autopilot);

    // Load hitobjects
    auto diffres =
        DatabaseBeatmap::loadDifficultyHitObjects(map->getFilePath(), AR, CS, score.mods.speed, false, stoken);
    if(stoken.stop_requested()) return;
    if(diffres.error.errc) return;

    AsyncPPC::pp_res info;
    DifficultyCalculator::BeatmapDiffcalcData diffcalc_data{.sortedHitObjects = diffres.diffobjects,
                                                            .CS = CS,
                                                            .HP = HP,
                                                            .AR = AR,
                                                            .OD = OD,
                                                            .hidden = HD,
                                                            .relax = RX,
                                                            .autopilot = AP,
                                                            .touchDevice = TD,
                                                            .speedMultiplier = score.mods.speed,
                                                            .breakDuration = diffres.totalBreakDuration,
                                                            .playableLength = diffres.playableLength};

    DifficultyCalculator::DifficultyAttributes attributes_out{};

    DifficultyCalculator::StarCalcParams params{.cachedDiffObjects = {},
                                                .outAttributes = attributes_out,
                                                .beatmapData = diffcalc_data,
                                                .outAimStrains = &info.aimStrains,
                                                .outSpeedStrains = &info.speedStrains,
                                                .incremental = nullptr,
                                                .upToObjectIndex = -1,
                                                .cancelCheck = stoken};

    info.total_stars = DifficultyCalculator::calculateStarDiffForHitObjects(params);

    info.aim_stars = attributes_out.AimDifficulty;
    info.aim_slider_factor = attributes_out.SliderFactor;
    info.difficult_aim_sliders = attributes_out.AimDifficultSliderCount;
    info.difficult_aim_strains = attributes_out.AimDifficultStrainCount;
    info.speed_stars = attributes_out.SpeedDifficulty;
    info.speed_notes = attributes_out.SpeedNoteCount;
    info.difficult_speed_strains = attributes_out.SpeedDifficultStrainCount;

    if(stoken.stop_requested()) return;

    DifficultyCalculator::PPv2CalcParams ppv2calcparams{.attributes = attributes_out,
                                                        .modFlags = score.mods.flags,
                                                        .timescale = score.mods.speed,
                                                        .ar = AR,
                                                        .od = OD,
                                                        .numHitObjects = map->iNumObjects,
                                                        .numCircles = map->iNumCircles,
                                                        .numSliders = map->iNumSliders,
                                                        .numSpinners = map->iNumSpinners,
                                                        .maxPossibleCombo = (i32)diffres.getTotalMaxCombo(),
                                                        .combo = score.comboMax,
                                                        .misses = score.numMisses,
                                                        .c300 = score.num300s,
                                                        .c100 = score.num100s,
                                                        .c50 = score.num50s,
                                                        .legacyTotalScore = (u32)score.score};

    // mcosu scores use a different scorev1 algorithm
    info.pp = DifficultyCalculator::calculatePPv2(ppv2calcparams, score.is_mcosu_imported());

    // Update score
    Sync::shared_lock readlock(db->scores_mtx);
    if(const auto& it = db->getScoresMutable().find(score.beatmap_hash); it != db->getScoresMutable().end()) {
        for(auto& dbScore : it->second) {
            if(dbScore.unixTimestamp == score.unixTimestamp) {
                readlock.unlock();
                readlock.release();
                Sync::unique_lock writelock(db->scores_mtx);

                dbScore.ppv2_version = DiffCalc::PP_ALGORITHM_VERSION;
                dbScore.ppv2_score = info.pp;
                dbScore.ppv2_total_stars = info.total_stars;
                dbScore.ppv2_aim_stars = info.aim_stars;
                dbScore.ppv2_speed_stars = info.speed_stars;
                db->scores_changed.store(true, std::memory_order_release);
                break;
            }
        }
    }
}

bool internal::score_needs_recalc(const FinishedScore& score) {
    if((USE_PPV3 && score.hitdeltas.empty())
       // should this be < or != ... ?
       || (score.ppv2_version < DiffCalc::PP_ALGORITHM_VERSION)
       // is this correct? e.g. if we can never successfully calculate for a score, what to do? we just keep trying and failing
       || (score.ppv2_score <= 0.f)) {
        return true;
    }
    return false;
}

void internal::runloop(const Sync::stop_token& stoken) {
    McThread::set_current_thread_name(ULITERAL("score_cvt"));
    McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority

    debugLog("Started score converter thread");

    // defer the actual needs-recalc check to run on the thread, to avoid unnecessarily blocking (O(n^2) loop)
    std::vector<FinishedScore> scores_to_calc;
    // lazy reserve, assume 3 scores per score vector
    scores_to_calc.reserve(db->getScores().size() * 3);

    {
        Sync::shared_lock lock(db->scores_mtx);
        for(const auto& [_, beatmap] : db->getScores()) {
            for(const auto& score : beatmap) {
                if(score_needs_recalc(score)) {
                    // make a copy
                    scores_to_calc.push_back(score);
                }
            }
        }
    }

    // deallocate unneeded space from reserve (if any)
    scores_to_calc.shrink_to_fit();
    sct_total = scores_to_calc.size();

    debugLog("Found {} scores which need pp recalculation", sct_total.load(std::memory_order_acquire));

    // nothing to do...
    if(sct_total == 0) return;

    for(i32 idx = 0; auto& score : scores_to_calc) {
        while(osu->shouldPauseBGThreads() && !stoken.stop_requested()) {
            Timing::sleepMS(100);
        }
        Timing::sleep(1);

        if(stoken.stop_requested()) return;

        // This is "placeholder" until we get accurate replay simulation
        {
            update_ppv2(score, stoken);
        }

        // @PPV3: below
        if(!USE_PPV3) {
            sct_computed.fetch_add(1, std::memory_order_relaxed);
            idx++;
            continue;
        }

        if(score.replay.empty()) {
            if(!LegacyReplay::load_from_disk(score, false)) {
                debugLog("Failed to load replay for score {:d}", idx);
                update_ppv2(score, stoken);
                sct_computed.fetch_add(1, std::memory_order_relaxed);
                idx++;
                continue;
            }
        }

        auto map = db->getBeatmapDifficulty(score.beatmap_hash);
        SimulatedBeatmapInterface smap(map, score.mods);
        smap.spectated_replay = score.replay;
        smap.simulate_to(map->getLengthMS());

        if(score.comboMax != smap.live_score.getComboMax())
            debugLog("Score {:d}: comboMax was {:d}, simulated {:d}", idx, score.comboMax,
                     smap.live_score.getComboMax());
        if(score.num300s != smap.live_score.getNum300s())
            debugLog("Score {:d}: n300 was {:d}, simulated {:d}", idx, score.num300s, smap.live_score.getNum300s());
        if(score.num100s != smap.live_score.getNum100s())
            debugLog("Score {:d}: n100 was {:d}, simulated {:d}", idx, score.num100s, smap.live_score.getNum100s());
        if(score.num50s != smap.live_score.getNum50s())
            debugLog("Score {:d}: n50 was {:d}, simulated {:d}", idx, score.num50s, smap.live_score.getNum50s());
        if(score.numMisses != smap.live_score.getNumMisses())
            debugLog("Score {:d}: nMisses was {:d}, simulated {:d}", idx, score.numMisses,
                     smap.live_score.getNumMisses());

        {
            Sync::shared_lock readlock(db->scores_mtx);
            if(const auto& it = db->getScoresMutable().find(score.beatmap_hash); it != db->getScoresMutable().end()) {
                for(auto& dbScore : it->second) {
                    if(dbScore.unixTimestamp == score.unixTimestamp) {
                        readlock.unlock();
                        readlock.release();
                        Sync::unique_lock writelock(db->scores_mtx);
                        // @PPV3: currently hitdeltas is always empty
                        dbScore.hitdeltas = score.hitdeltas;
                        break;
                    }
                }
            }
        }

        // TODO @kiwec: update & save scores/pp

        sct_computed.fetch_add(1, std::memory_order_relaxed);
        idx++;
    }

    sct_computed.fetch_add(1, std::memory_order_relaxed);
}

void start_calc() {
    abort_calc();

    // to be set in runloop (find scores which actually need recalc)
    sct_total = 0;
    sct_computed = 0;

    if(!db->getScores().empty()) {
        thr = Sync::jthread(internal::runloop);
    }
}

void abort_calc() {
    if(!thr.joinable()) return;

    thr = {};

    sct_total = 0;
    sct_computed = 0;
}

bool running() { return thr.joinable(); }

u32 get_total() { return sct_total.load(std::memory_order_acquire); }
u32 get_computed() { return sct_computed.load(std::memory_order_acquire); }
}  // namespace ScoreConverter
