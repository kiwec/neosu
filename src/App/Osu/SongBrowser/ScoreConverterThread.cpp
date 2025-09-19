// Copyright (c) 2024, kiwec, All rights reserved.
#include "ScoreConverterThread.h"

#include "Database.h"
#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "Engine.h"
#include "SimulatedBeatmapInterface.h"
#include "score.h"

// TODO
static constexpr bool USE_PPV3{false};

std::atomic<u32> sct_computed = 0;
std::atomic<u32> sct_total = 0;

static std::thread thr;
static std::atomic<bool> dead = true;

static std::vector<f64> aimStrains;
static std::vector<f64> speedStrains;
static std::vector<DifficultyCalculator::DiffObject> diffObjects;

// XXX: This is barebones, no caching, *hopefully* fast enough (worst part is loading the .osu files)
// XXX: Probably code duplicated a lot, I'm pretty sure there's 4 places where I calc ppv2...
static void update_ppv2(const FinishedScore& score) {
    if(score.ppv2_version >= DifficultyCalculator::PP_ALGORITHM_VERSION) return;

    auto map = db->getBeatmapDifficulty(score.beatmap_hash);
    if(!map) return;

    f32 AR = score.mods.get_naive_ar(map);
    f32 CS = score.mods.get_naive_cs(map);
    f32 OD = score.mods.get_naive_od(map);
    bool RX = ModMasks::eq(score.mods.flags, Replay::ModFlags::Relax);
    bool TD = ModMasks::eq(score.mods.flags, Replay::ModFlags::TouchDevice);

    // Load hitobjects
    auto diffres = DatabaseBeatmap::loadDifficultyHitObjects(map->getFilePath(), AR, CS, score.mods.speed, false, dead);
    if(dead.load()) return;
    if(diffres.errorCode) return;

    aimStrains.clear();
    speedStrains.clear();
    diffObjects.clear();

    pp_info info;
    DifficultyCalculator::StarCalcParams params;
    params.sortedHitObjects.swap(diffres.diffobjects);
    params.CS = CS;
    params.OD = OD;
    params.speedMultiplier = score.mods.speed;
    params.relax = RX;
    params.touchDevice = TD;
    params.aim = &info.aim_stars;
    params.aimSliderFactor = &info.aim_slider_factor;
    params.difficultAimStrains = &info.difficult_aim_strains;
    params.speed = &info.speed_stars;
    params.speedNotes = &info.speed_notes;
    params.difficultSpeedStrains = &info.difficult_speed_strains;
    params.upToObjectIndex = -1;
    params.outAimStrains = &aimStrains;
    params.outSpeedStrains = &speedStrains;
    info.total_stars = DifficultyCalculator::calculateStarDiffForHitObjectsInt(diffObjects, params, nullptr, dead);
    if(dead.load()) return;

    // swap back
    diffres.diffobjects.swap(params.sortedHitObjects);

    info.pp = DifficultyCalculator::calculatePPv2(
        score.mods.to_legacy(), score.mods.speed, AR, OD, info.aim_stars, info.aim_slider_factor,
        info.difficult_aim_strains, info.speed_stars, info.speed_notes, info.difficult_speed_strains, map->iNumCircles,
        map->iNumSliders, map->iNumSpinners, diffres.maxPossibleCombo, score.comboMax, score.numMisses, score.num300s,
        score.num100s, score.num50s);

    // Update score
    db->scores_mtx.lock();
    for(auto& other : (*db->getScores())[score.beatmap_hash]) {
        if(other.unixTimestamp == score.unixTimestamp) {
            other.ppv2_version = DifficultyCalculator::PP_ALGORITHM_VERSION;
            other.ppv2_score = info.pp;
            other.ppv2_total_stars = info.total_stars;
            other.ppv2_aim_stars = info.aim_stars;
            other.ppv2_speed_stars = info.speed_stars;
            db->bDidScoresChangeForStats = true;
            break;
        }
    }
    db->scores_mtx.unlock();
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

    debugLog("Started score converter thread\n");

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

    debugLog("Found {} scores which need pp recalculation\n", sct_total.load());

    // nothing to do...
    if(sct_total == 0) return;

    for(i32 idx = 0; auto& score : scores_to_calc) {
        while(osu->shouldPauseBGThreads() && !dead.load()) {
            Timing::sleepMS(100);
        }
        Timing::sleep(1);

        if(dead.load()) return;

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
                debugLog("Failed to load replay for score {:d}\n", idx);
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
            debugLog("Score {:d}: comboMax was {:d}, simulated {:d}\n", idx, score.comboMax,
                     smap.live_score.getComboMax());
        if(score.num300s != smap.live_score.getNum300s())
            debugLog("Score {:d}: n300 was {:d}, simulated {:d}\n", idx, score.num300s, smap.live_score.getNum300s());
        if(score.num100s != smap.live_score.getNum100s())
            debugLog("Score {:d}: n100 was {:d}, simulated {:d}\n", idx, score.num100s, smap.live_score.getNum100s());
        if(score.num50s != smap.live_score.getNum50s())
            debugLog("Score {:d}: n50 was {:d}, simulated {:d}\n", idx, score.num50s, smap.live_score.getNum50s());
        if(score.numMisses != smap.live_score.getNumMisses())
            debugLog("Score {:d}: nMisses was {:d}, simulated {:d}\n", idx, score.numMisses,
                     smap.live_score.getNumMisses());

        db->scores_mtx.lock();
        for(auto& dbScore : (*db->getScores())[score.beatmap_hash]) {
            if(dbScore.unixTimestamp == score.unixTimestamp) {
                // @PPV3: currently hitdeltas is always empty
                dbScore.hitdeltas = score.hitdeltas;
                break;
            }
        }
        db->scores_mtx.unlock();

        // TODO @kiwec: update & save scores/pp

        sct_computed++;
        idx++;
    }

    sct_computed++;
}

void sct_calc(std::unordered_map<MD5Hash, std::vector<FinishedScore>> scores_to_maybe_calc) {
    sct_abort();

    dead = false;

    // to be set in run_sct (find scores which actually need recalc)
    sct_total = 0;
    sct_computed = 0;

    if(!scores_to_maybe_calc.empty()) {
        thr = std::thread(run_sct, std::move(scores_to_maybe_calc));
    }
}

void sct_abort() {
    if(dead.load()) return;

    dead = true;
    thr.join();

    sct_total = 0;
    sct_computed = 0;
}
