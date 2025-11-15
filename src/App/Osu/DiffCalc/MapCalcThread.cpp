// Copyright (c) 2024, kiwec, All rights reserved.
#include "MapCalcThread.h"

#include <thread>

#include "DatabaseBeatmap.h"
#include "AsyncPPCalculator.h"
#include "DifficultyCalculator.h"
#include "Osu.h"
#include "Timing.h"
#include "Thread.h"

void MapCalcThread::start_calc_instance(const std::vector<DatabaseBeatmap*>& maps_to_calc) {
    abort_instance();

    if(maps_to_calc.empty()) {
        return;
    }

    this->should_stop = false;
    this->maps_to_process = &maps_to_calc;
    this->computed_count = 0;
    this->total_count = static_cast<u32>(this->maps_to_process->size()) + 1;
    this->results.clear();

    this->worker_thread = std::thread(&MapCalcThread::run, this);
}

void MapCalcThread::abort_instance() {
    if(this->should_stop.load(std::memory_order_acquire)) {
        return;
    }

    this->should_stop = true;

    if(this->worker_thread.joinable()) {
        this->worker_thread.join();
    }

    this->total_count = 0;
    this->computed_count = 0;
    this->maps_to_process = nullptr;
}

void MapCalcThread::run() {
    McThread::set_current_thread_name("map_calc");
    McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority

    const auto deadCheck = [die = &this->should_stop](void) -> bool { return die->load(std::memory_order_acquire); };

    for(const auto& map : *this->maps_to_process) {
        // pause handling
        while(osu->shouldPauseBGThreads() && !this->should_stop.load(std::memory_order_acquire)) {
            Timing::sleepMS(100);
        }
        Timing::sleep(1);

        if(this->should_stop.load(std::memory_order_acquire)) {
            return;
        }

        mct_result result{.map = map};

        if(this->should_stop.load(std::memory_order_acquire)) {
            return;
        }

        auto c = DatabaseBeatmap::loadPrimitiveObjects(map->sFilePath, deadCheck);

        if(this->should_stop.load(std::memory_order_acquire)) {
            return;
        }

        if(c.errorCode) {
            this->results.push_back(result);
            this->computed_count++;
            continue;
        }

        result.nb_circles = c.numCircles;
        result.nb_sliders = c.numSliders;
        result.nb_spinners = c.numSpinners;

        AsyncPPC::pp_res info;
        auto diffres =
            DatabaseBeatmap::loadDifficultyHitObjects(c, map->getAR(), map->getCS(), 1.f, false, deadCheck);

        if(this->should_stop.load(std::memory_order_acquire)) {
            return;
        }

        if(diffres.errorCode) {
            this->results.push_back(result);
            this->computed_count++;
            continue;
        }

        DifficultyCalculator::StarCalcParams params{.cachedDiffObjects = {},
                                                    .sortedHitObjects = diffres.diffobjects,
                                                    .CS = map->getCS(),
                                                    .OD = map->getOD(),
                                                    .speedMultiplier = 1.f,
                                                    .relax = false,
                                                    .touchDevice = false,
                                                    .aim = &info.aim_stars,
                                                    .aimSliderFactor = &info.aim_slider_factor,
                                                    .aimDifficultSliders = &info.difficult_aim_sliders,
                                                    .difficultAimStrains = &info.difficult_aim_strains,
                                                    .speed = &info.speed_stars,
                                                    .speedNotes = &info.speed_notes,
                                                    .difficultSpeedStrains = &info.difficult_speed_strains,
                                                    .upToObjectIndex = -1,
                                                    .incremental = {},
                                                    .outAimStrains = &info.aimStrains,
                                                    .outSpeedStrains = &info.speedStrains,
                                                    .cancelCheck = deadCheck};

        result.star_rating = static_cast<f32>(DifficultyCalculator::calculateStarDiffForHitObjects(params));

        if(this->should_stop.load(std::memory_order_acquire)) {
            return;
        }

        BPMInfo bpm{};
        if(c.timingpoints.size() > 0) {
            this->bpm_calc_buf.resize(c.timingpoints.size());
            bpm = getBPM(c.timingpoints, this->bpm_calc_buf);
        }
        result.min_bpm = bpm.min;
        result.max_bpm = bpm.max;
        result.avg_bpm = bpm.most_common;

        this->results.push_back(result);
        this->computed_count++;
    }

    this->computed_count++;
}

MapCalcThread& MapCalcThread::get_instance() {
    static MapCalcThread instance;
    return instance;
}
