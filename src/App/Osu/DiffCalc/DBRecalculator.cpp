// Copyright (c) 2024, kiwec & 2025, WH, All rights reserved.

// - Groups all work by beatmap MD5 hash so each .osu file is loaded exactly once
// - Within each beatmap, groups scores by mod parameters (AR/CS/OD/speed/etc.) so
//   difficulty attributes are calculated once per unique parameter set

#include "DBRecalculator.h"

#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Osu.h"
#include "OsuConVars.h"
#include "DifficultyCalculator.h"
#include "score.h"
#include "Timing.h"
#include "Logging.h"
#include "Thread.h"
#include "SyncJthread.h"
#include "SyncStoptoken.h"

#include <atomic>
#include <memory>
#include <unordered_map>

namespace DBRecalculator {

struct internal {
    static void update_score_in_db(const FinishedScore& score, f64 pp, f64 total_stars, f64 aim_stars, f64 speed_stars);
    static std::vector<BeatmapDifficulty*> collect_outdated_db_diffs(const Sync::stop_token& stoken);
};

void internal::update_score_in_db(const FinishedScore& score, f64 pp, f64 total_stars, f64 aim_stars, f64 speed_stars) {
    Sync::shared_lock readlock(db->scores_mtx);
    auto it = db->getScoresMutable().find(score.beatmap_hash);
    if(it == db->getScoresMutable().end()) return;

    for(auto& dbScore : it->second) {
        if(dbScore.unixTimestamp == score.unixTimestamp) {
            readlock.unlock();
            readlock.release();
            Sync::unique_lock writelock(db->scores_mtx);

            dbScore.ppv2_version = DiffCalc::PP_ALGORITHM_VERSION;
            dbScore.ppv2_score = pp;
            dbScore.ppv2_total_stars = total_stars;
            dbScore.ppv2_aim_stars = aim_stars;
            dbScore.ppv2_speed_stars = speed_stars;
            db->scores_changed.store(true, std::memory_order_release);
            return;
        }
    }
}

std::vector<BeatmapDifficulty*> internal::collect_outdated_db_diffs(const Sync::stop_token& stoken) {
    std::vector<BeatmapDifficulty*> ret;
    {
        Sync::shared_lock readlock(db->beatmap_difficulties_mtx);
        for(const auto& [_, diff] : db->beatmap_difficulties) {
            if(stoken.stop_requested()) break;
            if(diff->fStarsNomod <= 0.f || diff->ppv2Version < DiffCalc::PP_ALGORITHM_VERSION) {
                ret.push_back(diff);
            }
        }
    }
    return ret;
}

namespace {

#define logFailure(err__, ...) \
    cv::debug_pp.getBool() ? debugLog("{}: {}", (err__).error_string(), fmt::format(__VA_ARGS__)) : (void)(0)

// Mod parameters that affect difficulty calculation. Scores with identical
// ModParams on the same beatmap can share star rating calculations.
struct ModParams {
    f32 ar{5.f}, cs{5.f}, od{5.f}, hp{5.f};
    f32 speed{1.f};
    bool hd{false}, rx{false}, ap{false}, td{false};

    bool operator==(const ModParams&) const = default;
};

struct ModParamsHash {
    size_t operator()(const ModParams& p) const {
        size_t h = std::hash<f32>{}(p.ar);
        h ^= std::hash<f32>{}(p.cs) << 1;
        h ^= std::hash<f32>{}(p.od) << 2;
        h ^= std::hash<f32>{}(p.speed) << 3;
        h ^= (p.hd ? 1 : 0) << 4;
        h ^= (p.rx ? 1 : 0) << 5;
        h ^= (p.ap ? 1 : 0) << 6;
        h ^= (p.td ? 1 : 0) << 7;
        return h;
    }
};

struct ScoreWork {
    FinishedScore score;
    ModParams params;
};

// All work for a single beatmap: optional map recalc + zero or more scores
struct WorkItem {
    MD5Hash hash;
    BeatmapDifficulty* map;
    bool needs_map_calc;
    std::vector<ScoreWork> scores;
};

Sync::jthread worker_thread;

std::atomic<u32> scores_processed{0};
std::atomic<u32> scores_total{0};
std::atomic<u32> maps_processed{0};
std::atomic<u32> maps_total{0};
std::atomic<bool> workqueue_ready{false};

std::vector<MapResult> map_results;
Sync::mutex results_mutex;

// Owned by worker thread during execution
std::vector<WorkItem> work_queue;
std::vector<BPMTuple> bpm_calc_buf;

// The order in which the work is run doesn't really make this cache that useful, but just pass it
// as a star calculation parameter to avoid it needing to reallocate a new cache
std::unique_ptr<std::vector<DifficultyCalculator::DiffObject>> dummy_diffobj_cache;

// Maps to recalc, copied from start_calc argument for thread-safe access
std::vector<BeatmapDifficulty*> pending_diffs_to_recalc;

forceinline bool score_needs_recalc(const FinishedScore& score) {
    return score.ppv2_version < DiffCalc::PP_ALGORITHM_VERSION || score.ppv2_score <= 0.f;
}

// Calculate difficulty and PP for a group of scores sharing mod parameters.
void process_score_group(BeatmapDifficulty* map, const ModParams& params, const std::vector<const ScoreWork*>& scores,
                         DatabaseBeatmap::PRIMITIVE_CONTAINER& primitives, const Sync::stop_token& stoken) {
    if(scores.empty()) return;

    auto diffres =
        DatabaseBeatmap::loadDifficultyHitObjects(primitives, params.ar, params.cs, params.speed, false, stoken);
    if(stoken.stop_requested()) return;
    if(diffres.error.errc) {
        logFailure(diffres.error, "loadDifficultyHitObjects map hash {} map path {}", map->getMD5().string(),
                   map->getFilePath());
        scores_processed.fetch_add(scores.size(), std::memory_order_relaxed);
        return;
    }

    DifficultyCalculator::BeatmapDiffcalcData diffcalc_data{.sortedHitObjects = diffres.diffobjects,
                                                            .CS = params.cs,
                                                            .HP = params.hp,
                                                            .AR = params.ar,
                                                            .OD = params.od,
                                                            .hidden = params.hd,
                                                            .relax = params.rx,
                                                            .autopilot = params.ap,
                                                            .touchDevice = params.td,
                                                            .speedMultiplier = params.speed,
                                                            .breakDuration = diffres.totalBreakDuration,
                                                            .playableLength = diffres.playableLength};

    DifficultyCalculator::DifficultyAttributes attributes{};

    DifficultyCalculator::StarCalcParams star_params{.cachedDiffObjects = std::move(dummy_diffobj_cache),
                                                     .outAttributes = attributes,
                                                     .beatmapData = diffcalc_data,
                                                     .outAimStrains = nullptr,
                                                     .outSpeedStrains = nullptr,
                                                     .incremental = nullptr,
                                                     .upToObjectIndex = -1,
                                                     .cancelCheck = stoken};

    f64 total_stars = DifficultyCalculator::calculateStarDiffForHitObjects(star_params);
    dummy_diffobj_cache = std::move(star_params.cachedDiffObjects);
    dummy_diffobj_cache->clear();

    if(stoken.stop_requested()) return;

    // Calculate PP for each score using shared difficulty attributes
    for(const auto* sw : scores) {
        const auto& score = sw->score;

        DifficultyCalculator::PPv2CalcParams ppv2params{.attributes = attributes,
                                                        .modFlags = score.mods.flags,
                                                        .timescale = score.mods.speed,
                                                        .ar = params.ar,
                                                        .od = params.od,
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
        f64 pp = DifficultyCalculator::calculatePPv2(ppv2params, score.is_mcosu_imported());
        internal::update_score_in_db(score, pp, total_stars, attributes.AimDifficulty, attributes.SpeedDifficulty);

        scores_processed.fetch_add(1, std::memory_order_relaxed);
    }
}

// Build work queue on worker thread to avoid blocking main thread.
// Iterating over 100k+ scores with score_needs_recalc() checks is O(n).
void build_work_queue(const Sync::stop_token& stoken) {
    std::unordered_map<MD5Hash, WorkItem> work_by_hash;

    // add maps needing star rating recalc (ppv2 version outdated)
    pending_diffs_to_recalc = internal::collect_outdated_db_diffs(stoken);
    if(stoken.stop_requested()) return;

    for(auto* diff : pending_diffs_to_recalc) {
        if(stoken.stop_requested()) return;
        const auto& hash = diff->getMD5();
        auto& item = work_by_hash[hash];
        item.hash = hash;
        item.map = diff;
        item.needs_map_calc = true;
    }

    maps_total.store(static_cast<u32>(pending_diffs_to_recalc.size()), std::memory_order_relaxed);
    pending_diffs_to_recalc.clear();
    pending_diffs_to_recalc.shrink_to_fit();

    // find all scores needing PP recalc, grouped by beatmap
    u32 score_count = 0;
    {
        Sync::shared_lock lock(db->scores_mtx);
        for(const auto& [hash, scores] : db->getScores()) {
            if(stoken.stop_requested()) return;

            for(const auto& score : scores) {
                if(!score_needs_recalc(score)) continue;

                auto* map = db->getBeatmapDifficulty(hash);
                if(!map) continue;

                auto& item = work_by_hash[hash];
                if(item.map == nullptr) {
                    item.hash = hash;
                    item.map = map;
                }

                ScoreWork sw;
                sw.score = score;
                sw.params.ar = score.mods.get_naive_ar(map);
                sw.params.cs = score.mods.get_naive_cs(map);
                sw.params.od = score.mods.get_naive_od(map);
                sw.params.hp = score.mods.get_naive_hp(map);
                sw.params.speed = score.mods.speed;
                sw.params.hd = score.mods.has(ModFlags::Hidden);
                sw.params.rx = score.mods.has(ModFlags::Relax);
                sw.params.ap = score.mods.has(ModFlags::Autopilot);
                sw.params.td = score.mods.has(ModFlags::TouchDevice);

                item.scores.push_back(std::move(sw));
                score_count++;
            }
        }
    }

    scores_total.store(score_count, std::memory_order_relaxed);

    // flatten to vector for iteration
    work_queue.clear();
    work_queue.reserve(work_by_hash.size());
    for(auto& [_, item] : work_by_hash) {
        work_queue.push_back(std::move(item));
    }
}

void process_work_item(WorkItem& item, const Sync::stop_token& stoken) {
    if(!item.map) return;

    // load primitive objects once for this beatmap
    auto primitives = DatabaseBeatmap::loadPrimitiveObjects(item.map->sFilePath, stoken);
    if(stoken.stop_requested()) return;

    if(primitives.error.errc) {
        logFailure(primitives.error, "loadPrimitiveObjects map hash: {} map path: {}", item.map->getMD5().string(),
                   item.map->sFilePath);
        if(item.needs_map_calc) {
            Sync::scoped_lock lock(results_mutex);
            map_results.push_back(MapResult{.map = item.map});
            maps_processed.fetch_add(1, std::memory_order_relaxed);
        }
        scores_processed.fetch_add(static_cast<u32>(item.scores.size()), std::memory_order_relaxed);
        return;
    }

    // process map calculation (nomod stars, BPM, object counts)
    if(item.needs_map_calc) {
        // first loadDifficultyHitObjects call calculates slider times and sets sliderTimesCalculated
        auto diffres = DatabaseBeatmap::loadDifficultyHitObjects(primitives, item.map->getAR(), item.map->getCS(), 1.f,
                                                                 false, stoken);

        MapResult result{.map = item.map,
                         .length_ms = diffres.playableLength,
                         .nb_circles = primitives.numCircles,
                         .nb_sliders = primitives.numSliders,
                         .nb_spinners = primitives.numSpinners};

        if(stoken.stop_requested()) return;

        if(!diffres.error.errc) {
            DifficultyCalculator::BeatmapDiffcalcData diffcalc_data{.sortedHitObjects = diffres.diffobjects,
                                                                    .CS = item.map->getCS(),
                                                                    .HP = item.map->getHP(),
                                                                    .AR = item.map->getAR(),
                                                                    .OD = item.map->getOD(),
                                                                    .hidden = false,
                                                                    .relax = false,
                                                                    .autopilot = false,
                                                                    .touchDevice = false,
                                                                    .speedMultiplier = 1.f,
                                                                    .breakDuration = primitives.totalBreakDuration,
                                                                    .playableLength = diffres.playableLength};

            DifficultyCalculator::DifficultyAttributes attributes{};

            DifficultyCalculator::StarCalcParams star_params{.cachedDiffObjects = std::move(dummy_diffobj_cache),
                                                             .outAttributes = attributes,
                                                             .beatmapData = diffcalc_data,
                                                             .outAimStrains = nullptr,
                                                             .outSpeedStrains = nullptr,
                                                             .incremental = nullptr,
                                                             .upToObjectIndex = -1,
                                                             .cancelCheck = stoken};

            result.star_rating = static_cast<f32>(DifficultyCalculator::calculateStarDiffForHitObjects(star_params));

            dummy_diffobj_cache = std::move(star_params.cachedDiffObjects);
            dummy_diffobj_cache->clear();
        } else {
            logFailure(diffres.error, "loadDifficultyHitObjects map hash: {} map path: {}", item.map->getMD5().string(),
                       item.map->sFilePath);
        }

        if(stoken.stop_requested()) return;

        if(!primitives.timingpoints.empty()) {
            bpm_calc_buf.resize(primitives.timingpoints.size());
            BPMInfo bpm = getBPM(primitives.timingpoints, bpm_calc_buf);
            result.min_bpm = bpm.min;
            result.max_bpm = bpm.max;
            result.avg_bpm = bpm.most_common;
        }

        {
            Sync::scoped_lock lock(results_mutex);
            map_results.push_back(result);
        }
        maps_processed.fetch_add(1, std::memory_order_relaxed);
    }

    if(stoken.stop_requested()) return;

    // process score calculations, grouped by mod parameters to share difficulty calc
    // subsequent loadDifficultyHitObjects calls skip slider timing (sliderTimesCalculated == true)
    if(!item.scores.empty()) {
        std::unordered_map<ModParams, std::vector<const ScoreWork*>, ModParamsHash> score_groups;
        for(const auto& sw : item.scores) {
            score_groups[sw.params].push_back(&sw);
        }

        for(const auto& [params, group] : score_groups) {
            if(stoken.stop_requested()) return;
            process_score_group(item.map, params, group, primitives, stoken);
        }
    }

    // free memory from processed scores
    item.scores.clear();
    item.scores.shrink_to_fit();
}

void runloop(const Sync::stop_token& stoken) {
    McThread::set_current_thread_name(US_("db_recalc"));
    McThread::set_current_thread_prio(McThread::Priority::NORMAL);

    build_work_queue(stoken);
    workqueue_ready.store(true, std::memory_order_release);

    if(stoken.stop_requested()) return;

    debugLog("DB recalculator: {} work items ({} maps, {} scores)", work_queue.size(), get_maps_total(),
             get_scores_total());

    for(auto& item : work_queue) {
        while(osu->shouldPauseBGThreads() && !stoken.stop_requested()) {
            Timing::sleepMS(100);
        }

        if(stoken.stop_requested()) return;

        process_work_item(item, stoken);

        Timing::sleep(0);
    }

    // just in case
    maps_processed.store(get_maps_total(), std::memory_order_release);
    scores_processed.store(get_scores_total(), std::memory_order_release);

    // cleanup
    work_queue.clear();
    work_queue.shrink_to_fit();
    bpm_calc_buf.clear();
    bpm_calc_buf.shrink_to_fit();
}

}  // namespace

void start_calc() {
    abort_calc();

    maps_processed = 0;
    scores_processed = 0;
    maps_total = 0;
    scores_total = 0;
    workqueue_ready = false;
    map_results.clear();
    if(!dummy_diffobj_cache) {
        // this will stay alive forever, just make sure its created once
        dummy_diffobj_cache = std::make_unique<std::vector<DifficultyCalculator::DiffObject>>();
    } else {
        dummy_diffobj_cache->clear();
    }

    worker_thread = Sync::jthread(runloop);
}

void abort_calc() {
    if(!worker_thread.joinable()) return;

    worker_thread = {};

    scores_total = 0;
    maps_total = 0;
    maps_processed = 0;
    scores_processed = 0;
    workqueue_ready = false;
    work_queue.clear();
    map_results.clear();
    pending_diffs_to_recalc.clear();
    if(dummy_diffobj_cache) {
        dummy_diffobj_cache->clear();
    }
}

u32 get_maps_total() { return maps_total.load(std::memory_order_acquire); }

u32 get_maps_processed() { return maps_processed.load(std::memory_order_acquire); }

u32 get_scores_total() { return scores_total.load(std::memory_order_acquire); }

u32 get_scores_processed() { return scores_processed.load(std::memory_order_acquire); }

bool running() { return workqueue_ready.load(std::memory_order_acquire) && worker_thread.joinable(); }

bool scores_finished() {
    const u32 score_total = get_scores_total();
    return get_scores_processed() >= score_total;
}

bool is_finished() {
    const u32 processed = get_maps_processed() + get_scores_processed();
    const u32 total = get_maps_total() + get_scores_total();
    return workqueue_ready.load(std::memory_order_acquire) && processed >= total;
}

std::vector<MapResult> get_map_results() {
    Sync::scoped_lock lock(results_mutex);
    std::vector<MapResult> moved = std::move(map_results);
    map_results.clear();
    return moved;
}

std::optional<std::vector<MapResult>> try_get_map_results() {
    Sync::unique_lock lock(results_mutex, Sync::try_to_lock);
    if(!lock.owns_lock()) return std::nullopt;

    std::vector<MapResult> moved = std::move(map_results);
    map_results.clear();
    return moved;
}

}  // namespace DBRecalculator
