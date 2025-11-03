// Copyright (c) 2024, kiwec, All rights reserved.
#include "ScoreConverterThread.h"

#include "Database.h"
#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "SimulatedBeatmapInterface.h"
#include "score.h"
#include "Timing.h"
#include "Logging.h"
#include "SyncJthread.h"

// TODO
static constexpr bool USE_PPV3{false};

std::atomic<u32> sct_computed = 0;
std::atomic<u32> sct_total = 0;

static std::unique_ptr<Sync::jthread> thr;
static std::atomic<bool> dead = true;

// XXX: This is barebones, no caching, *hopefully* fast enough (worst part is loading the .osu files)
// XXX: Probably code duplicated a lot, I'm pretty sure there's 4 places where I calc ppv2...
static void update_ppv2(const FinishedScore& score) {
    if(score.ppv2_version >= DifficultyCalculator::PP_ALGORITHM_VERSION) return;

    auto map = db->getBeatmapDifficulty(score.beatmap_hash);
    if(!map) return;

    f32 AR = score.mods.get_naive_ar(map);
    f32 CS = score.mods.get_naive_cs(map);
    f32 OD = score.mods.get_naive_od(map);
    bool RX = score.mods.has(ModFlags::Relax);
    bool TD = score.mods.has(ModFlags::TouchDevice);

    // Load hitobjects
    auto diffres = DatabaseBeatmap::loadDifficultyHitObjects(map->getFilePath(), AR, CS, score.mods.speed, false, dead);
    if(dead.load(std::memory_order_acquire)) return;
    if(diffres.errorCode) return;

    pp_info info;
    DifficultyCalculator::StarCalcParams params{
        .cachedDiffObjects = {},
        .sortedHitObjects = diffres.diffobjects,

        .CS = CS,
        .OD = OD,
        .speedMultiplier = score.mods.speed,
        .relax = RX,
        .touchDevice = TD,
        .aim = &info.aim_stars,
        .aimSliderFactor = &info.aim_slider_factor,

        .aimDifficultSliders = &info.difficult_aim_sliders,
        .difficultAimStrains = &info.difficult_aim_strains,
        .speed = &info.speed_stars,
        .speedNotes = &info.speed_notes,
        .difficultSpeedStrains = &info.difficult_speed_strains,

        .upToObjectIndex = -1,
        .incremental = {},

        .outAimStrains = {},
        .outSpeedStrains = {},

        .dead = dead,
    };

    info.total_stars = DifficultyCalculator::calculateStarDiffForHitObjects(params);
    if(dead.load(std::memory_order_acquire)) return;

    info.pp = DifficultyCalculator::calculatePPv2(
        score.mods, AR, OD, info.aim_stars, info.aim_slider_factor, info.difficult_aim_sliders,
        info.difficult_aim_strains, info.speed_stars, info.speed_notes, info.difficult_speed_strains, map->iNumObjects,
        map->iNumCircles, map->iNumSliders, map->iNumSpinners, diffres.maxPossibleCombo, score.comboMax,
        score.numMisses, score.num300s, score.num100s, score.num50s);

    // Update score
    db->scores_mtx.lock_shared();  // take read lock
    for(auto& other : (*db->getScores())[score.beatmap_hash]) {
        if(other.unixTimestamp == score.unixTimestamp) {
            db->scores_mtx.unlock_shared();
            db->scores_mtx.lock();  // take write lock

            other.ppv2_version = DifficultyCalculator::PP_ALGORITHM_VERSION;
            other.ppv2_score = info.pp;
            other.ppv2_total_stars = info.total_stars;
            other.ppv2_aim_stars = info.aim_stars;
            other.ppv2_speed_stars = info.speed_stars;
            db->scores_mtx.unlock();
            db->bDidScoresChangeForStats = true;
            break;
        }
    }
}

static forceinline bool score_needs_recalc(const FinishedScore& score) {
    if((USE_PPV3 && score.hitdeltas.empty())
       // should this be < or != ... ?
       || (score.ppv2_version < DifficultyCalculator::PP_ALGORITHM_VERSION)
       // is this correct? e.g. if we can never successfully calculate for a score, what to do? we just keep trying and failing
       || (score.ppv2_score <= 0.f)) {
        return true;
    }
    return false;
}

static void run_sct(const std::unordered_map<MD5Hash, std::vector<FinishedScore>>& all_set_scores) {
    McThread::set_current_thread_name("score_cvt");
    McThread::set_current_thread_prio(false);  // reset priority

    debugLog("Started score converter thread");

    // defer the actual needs-recalc check to run on the thread, to avoid unnecessarily blocking (O(n^2) loop)
    std::vector<FinishedScore> scores_to_calc;
    // lazy reserve, assume 3 scores per score vector
    scores_to_calc.reserve(all_set_scores.size() * 3);

    for(const auto& [_, beatmap] : all_set_scores) {
        for(const auto& score : beatmap) {
            if(score_needs_recalc(score)) {
                scores_to_calc.push_back(score);
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
        while(osu->shouldPauseBGThreads() && !dead.load(std::memory_order_acquire)) {
            Timing::sleepMS(100);
        }
        Timing::sleep(1);

        if(dead.load(std::memory_order_acquire)) return;

        // This is "placeholder" until we get accurate replay simulation
        {
            update_ppv2(score);
        }

        // @PPV3: below
        if(!USE_PPV3) {
            sct_computed++;
            idx++;
            continue;
        }

        if(score.replay.empty()) {
            if(!LegacyReplay::load_from_disk(score, false)) {
                debugLog("Failed to load replay for score {:d}", idx);
                update_ppv2(score);
                sct_computed++;
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
            db->scores_mtx.lock_shared();  // take read lock
            for(auto& dbScore : (*db->getScores())[score.beatmap_hash]) {
                if(dbScore.unixTimestamp == score.unixTimestamp) {
                    db->scores_mtx.unlock_shared();
                    db->scores_mtx.lock();  // take write lock
                    // @PPV3: currently hitdeltas is always empty
                    dbScore.hitdeltas = score.hitdeltas;
                    db->scores_mtx.unlock();
                    break;
                }
            }
        }

        // TODO @kiwec: update & save scores/pp

        sct_computed++;
        idx++;
    }

    sct_computed++;
}

void sct_calc(std::unordered_map<MD5Hash, std::vector<FinishedScore>> scores_to_maybe_calc) {
    sct_abort();
    dead.store(false, std::memory_order_release);

    // to be set in run_sct (find scores which actually need recalc)
    sct_total = 0;
    sct_computed = 0;

    if(!scores_to_maybe_calc.empty()) {
        thr = std::make_unique<Sync::jthread>(run_sct, std::move(scores_to_maybe_calc));
    }
}

void sct_abort() {
    if(dead.load(std::memory_order_acquire)) return;

    dead.store(true, std::memory_order_release);
    thr.reset();

    sct_total = 0;
    sct_computed = 0;
}
