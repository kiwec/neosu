// Copyright (c) 2024, kiwec, All rights reserved.
#include "MapCalcThread.h"

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

    this->maps_to_process = &maps_to_calc;
    this->computed_count = 0;
    this->total_count = static_cast<u32>(this->maps_to_process->size()) + 1;
    this->results.clear();

    this->worker_thread = Sync::jthread([this](const Sync::stop_token& stoken) -> void { return this->run(stoken); });
}

void MapCalcThread::abort_instance() {
    if(!this->worker_thread.joinable()) {
        return;
    }

    this->worker_thread = {};

    this->total_count = 0;
    this->computed_count = 0;
    this->maps_to_process = nullptr;
}

void MapCalcThread::run(const Sync::stop_token& stoken) {
    McThread::set_current_thread_name(ULITERAL("map_calc"));
    McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority

    for(const auto& map : *this->maps_to_process) {
        // pause handling
        while(osu->shouldPauseBGThreads() && !stoken.stop_requested()) {
            Timing::sleepMS(100);
        }
        Timing::sleep(1);

        if(stoken.stop_requested()) {
            return;
        }

        mct_result result{.map = map};

        if(stoken.stop_requested()) {
            return;
        }

        auto c = DatabaseBeatmap::loadPrimitiveObjects(map->sFilePath, stoken);

        if(stoken.stop_requested()) {
            return;
        }

        if(c.error.errc) {
            this->results.push_back(result);
            this->computed_count++;
            continue;
        }

        result.nb_circles = c.numCircles;
        result.nb_sliders = c.numSliders;
        result.nb_spinners = c.numSpinners;

        AsyncPPC::pp_res info;
        auto diffres = DatabaseBeatmap::loadDifficultyHitObjects(c, map->getAR(), map->getCS(), 1.f, false, stoken);

        if(stoken.stop_requested()) {
            return;
        }

        if(c.error.errc) {
            this->results.push_back(result);
            this->computed_count++;
            continue;
        }

        DifficultyCalculator::BeatmapDiffcalcData diffcalc_data{.sortedHitObjects = diffres.diffobjects,
                                                                .CS = map->getCS(),
                                                                .HP = map->getHP(),
                                                                .AR = map->getAR(),
                                                                .OD = map->getOD(),
                                                                .hidden = false,
                                                                .relax = false,
                                                                .autopilot = false,
                                                                .touchDevice = false,
                                                                .speedMultiplier = 1.f,
                                                                .breakDuration = c.totalBreakDuration,
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

        result.star_rating = static_cast<f32>(DifficultyCalculator::calculateStarDiffForHitObjects(params));

        if(stoken.stop_requested()) {
            return;
        }

        info.aim_stars = attributes_out.AimDifficulty;
        info.aim_slider_factor = attributes_out.SliderFactor;
        info.difficult_aim_sliders = attributes_out.AimDifficultSliderCount;
        info.difficult_aim_strains = attributes_out.AimDifficultStrainCount;
        info.speed_stars = attributes_out.SpeedDifficulty;
        info.speed_notes = attributes_out.SpeedNoteCount;
        info.difficult_speed_strains = attributes_out.SpeedDifficultStrainCount;

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
