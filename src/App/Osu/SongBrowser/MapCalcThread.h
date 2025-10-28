#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "noinclude.h"
#include "types.h"
#include "templates.h"

#include <atomic>
#include <thread>
#include <vector>

class DatabaseBeatmap;
struct BPMTuple;

class MapCalcThread {
    NOCOPY_NOMOVE(MapCalcThread)
   public:
    MapCalcThread() = default;
    ~MapCalcThread() {
        // only clean up this instance's resources
        abort_instance();
    }

    struct mct_result {
        DatabaseBeatmap* map{};
        u32 nb_circles{};
        u32 nb_sliders{};
        u32 nb_spinners{};
        f32 star_rating{};
        u32 min_bpm{};
        u32 max_bpm{};
        u32 avg_bpm{};
    };

    static inline void start_calc(const std::vector<DatabaseBeatmap*>& maps_to_calc) {
        get_instance().start_calc_instance(maps_to_calc);
    }

    static inline void abort() { get_instance().abort_instance(); }

    // shutdown the singleton
    static inline void shutdown() { get_instance().abort_instance(); }

    // progress tracking
    static inline u32 get_computed() { return get_instance().computed_count.load(std::memory_order_acquire); }

    static inline u32 get_total() { return get_instance().total_count.load(std::memory_order_acquire); }

    static inline bool is_finished() {
        const auto& inst = get_instance();
        const u32 computed = inst.computed_count.load(std::memory_order_acquire);
        const u32 total = inst.total_count.load(std::memory_order_acquire);
        return total > 0 && computed >= total;
    }

    // results access
    static inline std::vector<mct_result>& get_results() { return get_instance().results; }

   private:
    void run();

    void start_calc_instance(const std::vector<DatabaseBeatmap*>& maps_to_calc);
    void abort_instance();

    // singleton access
    static MapCalcThread& get_instance();

    std::thread worker_thread;
    std::atomic<bool> should_stop{true};
    std::atomic<u32> computed_count{0};
    std::atomic<u32> total_count{0};
    std::vector<mct_result> results{};
    zarray<BPMTuple> bpm_calc_buf;

    const std::vector<DatabaseBeatmap*>* maps_to_process{nullptr};
};
