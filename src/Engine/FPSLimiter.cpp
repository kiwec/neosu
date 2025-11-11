// Copyright (c) 2025, WH, All rights reserved.
#include "FPSLimiter.h"
#include "Timing.h"
#include "ConVar.h"
#include "types.h"

#include <cassert>

namespace FPSLimiter {
namespace  // static
{
u64 next_frame_time{0};
}

void limit_frames(int target_fps) {
    if(target_fps > 0) {
        const u64 frame_time_ns = Timing::NS_PER_SECOND / static_cast<u64>(target_fps);
        const u64 now = Timing::getTicksNS();

        const bool use_1ms_mode = cv::fps_limiter_nobusywait.getBool();
        const bool fps_max_yield = cv::fps_max_yield.getBool();

        // if we're ahead of schedule, sleep until next frame
        if(next_frame_time > now) {
            if(use_1ms_mode) {  // sleep for 1ms every so often to reach the frametime target
                const u64 sleep_time_ns = next_frame_time - now;
                const u64 sleep_time_ms = sleep_time_ns / Timing::NS_PER_MS;

                if(sleep_time_ms > 0) {
                    Timing::sleepMS(sleep_time_ms);
                } else if(fps_max_yield) {
                    Timing::sleep(0);
                }
            } else {  // precise sleeps per-frame
                // never sleep more than the current target fps frame time
                const u64 sleep_time = std::min(next_frame_time - now, frame_time_ns);
                Timing::sleepNSPrecise(sleep_time);
            }
        } else {
            // behind schedule or exactly on time
            if(!use_1ms_mode) {
                // in precise mode, reset to now to avoid accumulating error
                if(fps_max_yield) {
                    Timing::sleep(0);
                    next_frame_time = Timing::getTicksNS();
                } else {
                    next_frame_time = now;
                }
            } else if(fps_max_yield) {  // in 1ms mode, don't reset, accumulate error is intentional
                Timing::sleep(0);
            }
        }
        // set time for next frame
        next_frame_time += frame_time_ns;
    } else if(cv::fps_unlimited_yield.getBool()) {
        Timing::sleep(0);
    }
}

void reset() { next_frame_time = 0; }

}  // namespace FPSLimiter
