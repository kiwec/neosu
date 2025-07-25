#include "LeaderboardPPCalcThread.h"

#include <mutex>
#include <thread>

#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "Timing.h"

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
    pp_info info{};
};

static BeatmapDifficulty* diff = NULL;

static std::condition_variable cond;
static std::thread thr;
static std::atomic<bool> dead = true;

static std::mutex work_mtx;
static std::vector<pp_calc_request> work;

static std::mutex cache_mtx;
static std::vector<std::pair<pp_calc_request, pp_info>> cache;

// We have to use pointers because of C++ MOVE SEMANTICS BULLSHIT
static std::vector<hitobject_cache*> ho_cache;
static std::vector<info_cache*> inf_cache;

static void run_thread() {
    std::vector<f64> aimStrains;
    std::vector<f64> speedStrains;

    for(;;) {
        std::unique_lock<std::mutex> lock(work_mtx);
        cond.wait(lock, [] { return !work.empty() || dead.load(); });
        if(dead.load()) return;

        while(!work.empty()) {
            if(dead.load()) return;
            if(osu->should_pause_background_threads.load()) {
                work_mtx.unlock();
                Timing::sleepMS(100);
                work_mtx.lock();
                continue;
            }

            pp_calc_request rqt = work[0];
            work.erase(work.begin());
            work_mtx.unlock();

            // Make sure we haven't already computed it
            bool already_computed = false;
            cache_mtx.lock();
            for(auto pair : cache) {
                if(pair.first.mods_legacy != rqt.mods_legacy) continue;
                if(pair.first.speed != rqt.speed) continue;
                if(pair.first.AR != rqt.AR) continue;
                if(pair.first.CS != rqt.CS) continue;
                if(pair.first.OD != rqt.OD) continue;
                if(pair.first.rx != rqt.rx) continue;
                if(pair.first.td != rqt.td) continue;
                if(pair.first.comboMax != rqt.comboMax) continue;
                if(pair.first.numMisses != rqt.numMisses) continue;
                if(pair.first.num300s != rqt.num300s) continue;
                if(pair.first.num100s != rqt.num100s) continue;
                if(pair.first.num50s != rqt.num50s) continue;

                already_computed = true;
                break;
            }
            cache_mtx.unlock();
            if(already_computed) {
                work_mtx.lock();
                continue;
            }
            if(dead.load()) {
                work_mtx.lock();
                return;
            }

            // Load hitobjects
            bool found_hitobjects = false;
            hitobject_cache* computed_ho = NULL;
            for(auto ho : ho_cache) {
                if(ho->speed != rqt.speed) continue;
                if(ho->AR != rqt.AR) continue;
                if(ho->CS != rqt.CS) continue;

                computed_ho = ho;
                found_hitobjects = true;
                break;
            }
            if(!found_hitobjects) {
                computed_ho = new hitobject_cache();
                computed_ho->speed = rqt.speed;
                computed_ho->AR = rqt.AR;
                computed_ho->CS = rqt.CS;
                if(dead.load()) {
                    work_mtx.lock();
                    return;
                }
                computed_ho->diffres =
                    DatabaseBeatmap::loadDifficultyHitObjects(diff->getFilePath(), rqt.AR, rqt.CS, rqt.speed, false, dead);
                if(dead.load()) {
                    work_mtx.lock();
                    return;
                }
                if(computed_ho->diffres.errorCode) {
                    work_mtx.lock();
                    continue;
                }

                ho_cache.push_back(computed_ho);
            }

            // Load pp_info
            bool found_info = false;
            info_cache* computed_info = NULL;
            for(auto info : inf_cache) {
                if(info->speed != rqt.speed) continue;
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
                aimStrains.clear();
                speedStrains.clear();
                computed_info = new info_cache();
                computed_info->speed = rqt.speed;
                computed_info->AR = rqt.AR;
                computed_info->CS = rqt.CS;
                computed_info->OD = rqt.OD;
                computed_info->rx = rqt.rx;
                computed_info->td = rqt.td;
                if(dead.load()) {
                    work_mtx.lock();
                    return;
                }

                DifficultyCalculator::StarCalcParams params;
                params.sortedHitObjects.swap(computed_ho->diffres.diffobjects);
                params.CS = rqt.CS;
                params.OD = rqt.OD;
                params.speedMultiplier = rqt.speed;
                params.relax = rqt.rx;
                params.touchDevice = rqt.td;
                params.aim = &computed_info->info.aim_stars;
                params.aimSliderFactor = &computed_info->info.aim_slider_factor;
                params.difficultAimStrains = &computed_info->info.difficult_aim_strains;
                params.speed = &computed_info->info.speed_stars;
                params.speedNotes = &computed_info->info.speed_notes;
                params.difficultSpeedStrains = &computed_info->info.difficult_speed_strains;
                params.upToObjectIndex = -1;
                params.outAimStrains = &aimStrains;
                params.outSpeedStrains = &speedStrains;
                computed_info->info.total_stars = DifficultyCalculator::calculateStarDiffForHitObjectsInt(
                    computed_info->cachedDiffObjects, params, NULL, dead);
                if(dead.load()) {
                    work_mtx.lock();
                    return;
                }

                inf_cache.push_back(computed_info);

                // swap back
                computed_ho->diffres.diffobjects.swap(params.sortedHitObjects);
            }

            if(dead.load()) {
                work_mtx.lock();
                return;
            }
            computed_info->info.pp = DifficultyCalculator::calculatePPv2(
                rqt.mods_legacy, rqt.speed, rqt.AR, rqt.OD, computed_info->info.aim_stars,
                computed_info->info.aim_slider_factor, computed_info->info.difficult_aim_strains,
                computed_info->info.speed_stars, computed_info->info.speed_notes,
                computed_info->info.difficult_speed_strains, diff->iNumCircles, diff->iNumSliders,
                diff->iNumSpinners, computed_ho->diffres.maxPossibleCombo, rqt.comboMax, rqt.numMisses, rqt.num300s,
                rqt.num100s, rqt.num50s);

            cache_mtx.lock();
            cache.emplace_back(rqt, computed_info->info);
            cache_mtx.unlock();

            work_mtx.lock();
        }
    }
}

void lct_set_map(DatabaseBeatmap* new_diff) {
    if(diff == new_diff) return;

    if(diff != NULL) {
        dead = true;
        cond.notify_one();
        thr.join();
        cache.clear();

        for(auto ho : ho_cache) {
            delete ho;
        }
        ho_cache.clear();

        for(auto inf : inf_cache) {
            delete inf;
        }
        inf_cache.clear();

        dead = false;
    }

    diff = new_diff;
    if(new_diff != NULL) {
        thr = std::thread(run_thread);
    }
}

pp_info lct_get_pp(pp_calc_request rqt) {
    cache_mtx.lock();
    for(auto pair : cache) {
        if(pair.first.mods_legacy != rqt.mods_legacy) continue;
        if(pair.first.speed != rqt.speed) continue;
        if(pair.first.AR != rqt.AR) continue;
        if(pair.first.CS != rqt.CS) continue;
        if(pair.first.OD != rqt.OD) continue;
        if(pair.first.rx != rqt.rx) continue;
        if(pair.first.td != rqt.td) continue;
        if(pair.first.comboMax != rqt.comboMax) continue;
        if(pair.first.numMisses != rqt.numMisses) continue;
        if(pair.first.num300s != rqt.num300s) continue;
        if(pair.first.num100s != rqt.num100s) continue;
        if(pair.first.num50s != rqt.num50s) continue;

        pp_info out = pair.second;
        cache_mtx.unlock();
        return out;
    }
    cache_mtx.unlock();

    work_mtx.lock();
    bool work_exists = false;
    for(auto w : work) {
        if(w.mods_legacy != rqt.mods_legacy) continue;
        if(w.speed != rqt.speed) continue;
        if(w.AR != rqt.AR) continue;
        if(w.CS != rqt.CS) continue;
        if(w.OD != rqt.OD) continue;
        if(w.rx != rqt.rx) continue;
        if(w.td != rqt.td) continue;
        if(w.comboMax != rqt.comboMax) continue;
        if(w.numMisses != rqt.numMisses) continue;
        if(w.num300s != rqt.num300s) continue;
        if(w.num100s != rqt.num100s) continue;
        if(w.num50s != rqt.num50s) continue;

        work_exists = true;
        break;
    }
    if(!work_exists) {
        work.push_back(rqt);
    }
    work_mtx.unlock();
    cond.notify_one();

    pp_info placeholder;
    placeholder.total_stars = -1.0;
    placeholder.aim_stars = -1.0;
    placeholder.aim_slider_factor = -1.0;
    placeholder.speed_stars = -1.0;
    placeholder.speed_notes = -1.0;
    placeholder.pp = -1.0;
    return placeholder;
}
