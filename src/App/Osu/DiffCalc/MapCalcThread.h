#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "noinclude.h"
#include "types.h"

#include "SyncMutex.h"
#include "SyncJthread.h"
#include "SyncStoptoken.h"

#include <atomic>
#include <cassert>
#include <vector>
#include <optional>
#include <vector>

class DatabaseBeatmap;
struct BPMTuple;

class MapCalcThread {
    NOCOPY_NOMOVE(MapCalcThread)
   public:
    MapCalcThread() = default;
    ~MapCalcThread();

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

    // move results out once finished
    static inline std::vector<mct_result> get_results() {
        assert(is_finished());
        std::vector<mct_result> moved_from = std::move(get_instance().results);
        get_instance().results.clear();
        return moved_from;
    }

    // return all results since last successful try_get of the currently in-progress results, without blocking
    static inline std::optional<std::vector<mct_result>> try_get() { return get_instance().try_get_instance(); }

   private:
    void run(const Sync::stop_token& stoken);

    void start_calc_instance(const std::vector<DatabaseBeatmap*>& maps_to_calc);
    void abort_instance();
    std::optional<std::vector<mct_result>> try_get_instance();

    // singleton access
    static MapCalcThread& get_instance();

    Sync::jthread worker_thread;
    std::atomic<u32> computed_count{0};
    std::atomic<u32> total_count{0};
    std::vector<mct_result> results{};
    Sync::mutex results_mutex;
    std::vector<BPMTuple> bpm_calc_buf;

    const std::vector<DatabaseBeatmap*>* maps_to_process{nullptr};
};
