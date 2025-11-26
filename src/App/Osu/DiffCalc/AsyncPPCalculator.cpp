// Copyright (c) 2024, kiwec, All rights reserved.
#include "AsyncPPCalculator.h"

#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "Osu.h"
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

    [[nodiscard]] bool matches(f32 spd, f32 ar, f32 cs) const { return speed == spd && AR == ar && CS == cs; }
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
    pp_res info{};

    [[nodiscard]] bool matches(f32 spd, f32 ar, f32 cs, f32 od, bool relax, bool touch) const {
        return speed == spd && AR == ar && CS == cs && OD == od && rx == relax && td == touch;
    }
};

static const BeatmapDifficulty* current_map = nullptr;

static Sync::condition_variable_any cond;
static Sync::jthread thr;

static Sync::mutex work_mtx;

// bool to keep track of "high priority" state
// might need mod updates to be recalc'd mid-gameplay
static std::vector<std::pair<pp_calc_request, bool>> work;

static Sync::mutex cache_mtx;

static std::vector<std::pair<pp_calc_request, pp_res>> cache;
static std::vector<hitobject_cache> ho_cache;
static std::vector<info_cache> inf_cache;

static void clear_caches() {
    Sync::unique_lock work_lock(work_mtx);
    Sync::unique_lock cache_lock(cache_mtx);

    work.clear();
    cache.clear();
    ho_cache.clear();
    inf_cache.clear();
}

static void run_thread(const Sync::stop_token& stoken) {
    McThread::set_current_thread_name(ULITERAL("async_pp_calc"));
    McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority

    const auto deadCheck = [&stoken](void) -> bool { return stoken.stop_requested(); };

    while(!stoken.stop_requested()) {
        Sync::unique_lock lock(work_mtx);
        cond.wait(lock, stoken, [] { return !work.empty(); });
        if(stoken.stop_requested()) return;

        while(!work.empty()) {
            if(stoken.stop_requested()) return;

            auto [rqt, highprio] = work.front();
            if(!highprio && osu->shouldPauseBGThreads()) {
                lock.unlock();
                Timing::sleepMS(100);
                lock.lock();
                continue;
            }

            work.erase(work.begin());

            // capture current map before unlocking (work items are specific to this map)
            const BeatmapDifficulty* map_for_rqt = current_map;
            lock.unlock();

            if(!map_for_rqt) continue;

            // skip if already computed
            {
                Sync::unique_lock cache_lock(cache_mtx);
                bool found = false;
                for(const auto& [request, info] : cache) {
                    if(request == rqt) {
                        found = true;
                        break;
                    }
                }
                if(found) {
                    lock.lock();
                    continue;
                }
            }

            if(stoken.stop_requested()) return;

            // find or compute hitobjects
            hitobject_cache* computed_ho = nullptr;
            for(auto& ho : ho_cache) {
                if(ho.matches(rqt.speedOverride, rqt.AR, rqt.CS)) {
                    computed_ho = &ho;
                    break;
                }
            }

            if(!computed_ho) {
                if(stoken.stop_requested()) return;

                hitobject_cache new_ho{
                    .speed = rqt.speedOverride,
                    .AR = rqt.AR,
                    .CS = rqt.CS,
                };

                new_ho.diffres = DatabaseBeatmap::loadDifficultyHitObjects(map_for_rqt->getFilePath(), rqt.AR, rqt.CS,
                                                                           rqt.speedOverride, false, deadCheck);

                if(stoken.stop_requested()) return;
                if(new_ho.diffres.errorCode) {
                    lock.lock();
                    continue;
                }

                ho_cache.push_back(std::move(new_ho));
                computed_ho = &ho_cache.back();
            }

            // find or compute difficulty info
            info_cache* computed_info = nullptr;
            for(auto& info : inf_cache) {
                if(info.matches(rqt.speedOverride, rqt.AR, rqt.CS, rqt.OD, rqt.rx, rqt.td)) {
                    computed_info = &info;
                    break;
                }
            }

            if(!computed_info) {
                if(stoken.stop_requested()) return;

                info_cache new_info{
                    .speed = rqt.speedOverride,
                    .AR = rqt.AR,
                    .CS = rqt.CS,
                    .OD = rqt.OD,
                    .rx = rqt.rx,
                    .td = rqt.td,
                };

                DifficultyCalculator::StarCalcParams params{
                    .cachedDiffObjects = std::move(new_info.cachedDiffObjects),
                    .sortedHitObjects = computed_ho->diffres.diffobjects,
                    .CS = rqt.CS,
                    .OD = rqt.OD,
                    .speedMultiplier = rqt.speedOverride,
                    .relax = rqt.rx,
                    .touchDevice = rqt.td,
                    .aim = &new_info.info.aim_stars,
                    .aimSliderFactor = &new_info.info.aim_slider_factor,
                    .aimDifficultSliders = &new_info.info.difficult_aim_sliders,
                    .difficultAimStrains = &new_info.info.difficult_aim_strains,
                    .speed = &new_info.info.speed_stars,
                    .speedNotes = &new_info.info.speed_notes,
                    .difficultSpeedStrains = &new_info.info.difficult_speed_strains,
                    .upToObjectIndex = -1,
                    .incremental = {},
                    .outAimStrains = &new_info.info.aimStrains,
                    .outSpeedStrains = &new_info.info.speedStrains,
                    .cancelCheck = deadCheck,
                };

                new_info.info.total_stars = DifficultyCalculator::calculateStarDiffForHitObjects(params);
                new_info.cachedDiffObjects = std::move(params.cachedDiffObjects);

                if(stoken.stop_requested()) return;

                inf_cache.push_back(std::move(new_info));
                computed_info = &inf_cache.back();
            }

            if(stoken.stop_requested()) return;

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
                .numHitObjects = map_for_rqt->iNumObjects,
                .numCircles = map_for_rqt->iNumCircles,
                .numSliders = map_for_rqt->iNumSliders,
                .numSpinners = map_for_rqt->iNumSpinners,
                .maxPossibleCombo = computed_ho->diffres.maxPossibleCombo,
                .combo = rqt.comboMax,
                .misses = rqt.numMisses,
                .c300 = rqt.num300s,
                .c100 = rqt.num100s,
                .c50 = rqt.num50s,
            };

            computed_info->info.pp = DifficultyCalculator::calculatePPv2(ppv2calcparams);

            {
                Sync::unique_lock cache_lock(cache_mtx);
                cache.emplace_back(rqt, computed_info->info);
            }

            lock.lock();
        }
    }
}
}  // namespace

void set_map(const DatabaseBeatmap* new_map) {
    if(current_map == new_map) return;

    const bool had_map = (current_map != nullptr);
    current_map = new_map;

    if(had_map) {
        clear_caches();
    }

    if(!had_map && new_map != nullptr) {
        thr = Sync::jthread(run_thread);
    } else if(had_map && new_map == nullptr) {
        if(thr.joinable()) {
            thr.request_stop();
            cond.notify_one();
            thr.join();
        }
    }
}

pp_res query_result(const pp_calc_request& rqt, bool ignoreBGThreadPause) {
    {
        Sync::unique_lock cache_lock(cache_mtx);
        for(const auto& [request, info] : cache) {
            if(request == rqt) {
                return info;
            }
        }
    }

    {
        Sync::unique_lock work_lock(work_mtx);
        bool work_exists = false;
        for(const auto& [w, prio] : work) {
            if(w == rqt) {
                work_exists = true;
                break;
            }
        }
        if(!work_exists) {
            work.emplace_back(rqt, ignoreBGThreadPause);
            cond.notify_one();
        }
    }

    static pp_res dummy{
        .total_stars = -1.0,
        .aim_stars = -1.0,
        .aim_slider_factor = -1.0,
        .speed_stars = -1.0,
        .speed_notes = -1.0,
        .pp = -1.0,
    };

    return dummy;
}
}  // namespace AsyncPPC
