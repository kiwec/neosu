// Copyright (c) 2024, kiwec, All rights reserved.
#include "AsyncPPCalculator.h"

#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "Timing.h"
#include "Thread.h"
#include "SyncMutex.h"
#include "SyncCV.h"

#include "SyncJthread.h"

namespace AsyncPPC {

namespace {  // static namespace
struct hitobject_cache {
    // Selectors
    f32 speed{};
    f32 AR{};
    f32 CS{};

    // Results
    DatabaseBeatmap::LOAD_DIFFOBJ_RESULT diffres{};
};

struct info_cache {
    // Selectors
    f32 speed{};
    f32 AR{};
    f32 CS{};
    f32 OD{};
    bool rx{};
    bool td{};

    // Results
    std::vector<DifficultyCalculator::DiffObject> cachedDiffObjects{};
    std::vector<DifficultyCalculator::DiffObject> diffObjects{};
    pp_res info{};
};

static const BeatmapDifficulty* map = nullptr;

static Sync::condition_variable_any cond;
static Sync::jthread thr;

static Sync::mutex work_mtx;
// bool to keep track of "high priority" state
// might need mod updates to be recalc'd mid-gameplay
static std::vector<std::pair<pp_calc_request, bool>> work;

static Sync::mutex cache_mtx;
static std::vector<std::pair<pp_calc_request, pp_res>> cache;

// We have to use pointers because of C++ MOVE SEMANTICS BULLSHIT
static std::vector<hitobject_cache*> ho_cache;
static std::vector<info_cache*> inf_cache;

static void run_thread(const Sync::stop_token& stoken) {
    McThread::set_current_thread_name("async_pp_calc");
    McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority

    const auto deadCheck = [&stoken](void) -> bool { return stoken.stop_requested(); };

    while(!stoken.stop_requested()) {
        Sync::unique_lock<Sync::mutex> lock(work_mtx);
        cond.wait(lock, stoken, [] { return !work.empty(); });
        if(stoken.stop_requested()) return;

        while(!work.empty()) {
            if(stoken.stop_requested()) return;

            auto [rqt, highprio] = work[0];
            if(!highprio && osu->shouldPauseBGThreads()) {
                work_mtx.unlock();
                Timing::sleepMS(100);
                work_mtx.lock();
                continue;
            }

            work.erase(work.begin());
            work_mtx.unlock();

            // Make sure we haven't already computed it
            bool already_computed = false;
            cache_mtx.lock();
            for(const auto& [request, info] : cache) {
                if(request != rqt) continue;
                already_computed = true;
                break;
            }
            cache_mtx.unlock();
            if(already_computed) {
                work_mtx.lock();
                continue;
            }
            if(stoken.stop_requested()) {
                work_mtx.lock();
                return;
            }

            // Load hitobjects
            bool found_hitobjects = false;
            hitobject_cache* computed_ho = nullptr;
            for(auto ho : ho_cache) {
                if(ho->speed != rqt.speedOverride) continue;
                if(ho->AR != rqt.AR) continue;
                if(ho->CS != rqt.CS) continue;

                computed_ho = ho;
                found_hitobjects = true;
                break;
            }
            if(!found_hitobjects) {
                computed_ho = new hitobject_cache();
                computed_ho->speed = rqt.speedOverride;
                computed_ho->AR = rqt.AR;
                computed_ho->CS = rqt.CS;
                if(stoken.stop_requested()) {
                    work_mtx.lock();
                    return;
                }

                computed_ho->diffres = DatabaseBeatmap::loadDifficultyHitObjects(map->getFilePath(), rqt.AR, rqt.CS,
                                                                                 rqt.speedOverride, false, deadCheck);
                if(stoken.stop_requested()) {
                    work_mtx.lock();
                    return;
                }
                if(computed_ho->diffres.errorCode) {
                    work_mtx.lock();
                    continue;
                }

                ho_cache.push_back(computed_ho);
            }

            // Load pp_res
            bool found_info = false;
            info_cache* computed_info = nullptr;
            for(auto info : inf_cache) {
                if(info->speed != rqt.speedOverride) continue;
                if(info->AR != rqt.AR) continue;
                if(info->CS != rqt.CS) continue;
                if(info->OD != rqt.OD) continue;
                if(info->rx != rqt.rx) continue;
                if(info->td != rqt.td) continue;

                computed_info = info;
                found_info = true;
                break;
            }
            if(!found_info) {
                computed_info = new info_cache();
                computed_info->speed = rqt.speedOverride;
                computed_info->AR = rqt.AR;
                computed_info->CS = rqt.CS;
                computed_info->OD = rqt.OD;
                computed_info->rx = rqt.rx;
                computed_info->td = rqt.td;
                if(stoken.stop_requested()) {
                    work_mtx.lock();
                    return;
                }

                DifficultyCalculator::StarCalcParams params{
                    .cachedDiffObjects = std::move(computed_info->cachedDiffObjects),
                    .sortedHitObjects = computed_ho->diffres.diffobjects,
                    .CS = rqt.CS,
                    .OD = rqt.OD,
                    .speedMultiplier = rqt.speedOverride,
                    .relax = rqt.rx,
                    .touchDevice = rqt.td,
                    .aim = &computed_info->info.aim_stars,
                    .aimSliderFactor = &computed_info->info.aim_slider_factor,
                    .aimDifficultSliders = &computed_info->info.difficult_aim_sliders,
                    .difficultAimStrains = &computed_info->info.difficult_aim_strains,
                    .speed = &computed_info->info.speed_stars,
                    .speedNotes = &computed_info->info.speed_notes,
                    .difficultSpeedStrains = &computed_info->info.difficult_speed_strains,
                    .upToObjectIndex = -1,
                    .incremental = {},
                    .outAimStrains = &computed_info->info.aimStrains,
                    .outSpeedStrains = &computed_info->info.speedStrains,
                    .cancelCheck = deadCheck,
                };
                computed_info->info.total_stars = DifficultyCalculator::calculateStarDiffForHitObjects(params);
                computed_info->cachedDiffObjects = std::move(params.cachedDiffObjects);
                if(stoken.stop_requested()) {
                    work_mtx.lock();
                    return;
                }

                inf_cache.push_back(computed_info);
            }

            if(stoken.stop_requested()) {
                work_mtx.lock();
                return;
            }

            DifficultyCalculator::PPv2CalcParams ppv2calcparams{
                .modFlags = rqt.modFlags,
                .speedOverride = rqt.speedOverride,
                .ar = rqt.AR,
                .od = rqt.OD,
                .aim = computed_info->info.aim_stars,
                .aimSliderFactor = computed_info->info.aim_slider_factor,
                .aimDifficultSliders = computed_info->info.difficult_aim_sliders,
                .aimDifficultStrains = computed_info->info.difficult_aim_strains,
                .speed = computed_info->info.speed_stars,
                .speedNotes = computed_info->info.speed_notes,
                .speedDifficultStrains = computed_info->info.difficult_speed_strains,
                .numHitObjects = map->iNumObjects,
                .numCircles = map->iNumCircles,
                .numSliders = map->iNumSliders,
                .numSpinners = map->iNumSpinners,
                .maxPossibleCombo = computed_ho->diffres.maxPossibleCombo,
                .combo = rqt.comboMax,
                .misses = rqt.numMisses,
                .c300 = rqt.num300s,
                .c100 = rqt.num100s,
                .c50 = rqt.num50s};

            computed_info->info.pp = DifficultyCalculator::calculatePPv2(ppv2calcparams);

            cache_mtx.lock();
            cache.emplace_back(rqt, computed_info->info);
            cache_mtx.unlock();

            work_mtx.lock();
        }
    }
}
}  // namespace

void set_map(const DatabaseBeatmap* new_map) {
    if(map == new_map) return;

    if(map != nullptr) {
        cond.notify_one();
        if(thr.joinable()) {
            thr.request_stop();
            thr.join();
        }
        cache.clear();

        for(auto ho : ho_cache) {
            delete ho;
        }
        ho_cache.clear();

        for(auto inf : inf_cache) {
            delete inf;
        }
        inf_cache.clear();
    }

    map = new_map;
    if(new_map != nullptr) {
        thr = Sync::jthread(run_thread);
    }
    return;
}

pp_res query_result(const pp_calc_request& rqt, bool ignoreBGThreadPause) {
    cache_mtx.lock();
    for(const auto& [request, info] : cache) {
        if(request != rqt) continue;

        pp_res out = info;
        cache_mtx.unlock();
        return out;
    }
    cache_mtx.unlock();

    work_mtx.lock();
    bool work_exists = false;
    for(const auto& [w, prio] : work) {
        if(w != rqt) continue;

        work_exists = true;
        break;
    }
    if(!work_exists) {
        work.emplace_back(rqt, ignoreBGThreadPause);
    }
    work_mtx.unlock();
    cond.notify_one();

    pp_res placeholder{
        .total_stars = -1.0,
        .aim_stars = -1.0,
        .aim_slider_factor = -1.0,
        .speed_stars = -1.0,
        .speed_notes = -1.0,
        .pp = -1.0,
    };

    return placeholder;
}
}  // namespace AsyncPPC
