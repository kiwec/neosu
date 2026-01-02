#pragma once
// Copyright (c) 2024, kiwec & 2025, WH, All rights reserved.
#include "types.h"

#include <optional>
#include <vector>

struct MD5Hash;

class DatabaseBeatmap;
using BeatmapDifficulty = DatabaseBeatmap;
using BeatmapSet = DatabaseBeatmap;

struct FinishedScore;

// Recalculates outdated/legacy scores and beatmaps imported from databases asynchronously.
namespace DBRecalculator {

struct MapResult {
    BeatmapDifficulty* map{};
    u32 nb_circles{};
    u32 nb_sliders{};
    u32 nb_spinners{};
    f32 star_rating{};
    u32 min_bpm{};
    u32 max_bpm{};
    u32 avg_bpm{};
};

// Start unified calculation for maps and all scores that need PP recalculation.
// Groups work by beatmap to load each file only once.
void start_calc();

void abort_calc();

[[nodiscard]] u32 get_maps_total();
[[nodiscard]] u32 get_maps_processed();

[[nodiscard]] u32 get_scores_total();
[[nodiscard]] u32 get_scores_processed();

[[nodiscard]] bool running();          // is the thread still running?
[[nodiscard]] bool scores_finished();  // are score recalculations done?
[[nodiscard]] bool is_finished();      // is everything done?

// Get map calculation results (scores are updated directly in the database)
[[nodiscard]] std::vector<MapResult> get_map_results();
[[nodiscard]] std::optional<std::vector<MapResult>> try_get_map_results();

struct internal;
}  // namespace DBRecalculator
