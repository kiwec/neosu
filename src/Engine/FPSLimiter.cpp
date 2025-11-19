// Copyright (c) 2025, WH, All rights reserved.
#include "FPSLimiter.h"
#include "Timing.h"
#include "ConVar.h"
#include "types.h"

#include <cassert>

namespace FPSLimiter {
namespace  // static
{

// callbacks (because convars are currently poorly optimized and we want to remove as many external variables as possible)
bool set_callbacks_once{false};

bool nobusywait{false};
bool max_yield{false};
bool unlimited_yield{false};

// state
u64 next_frame_time{0};

void set_callbacks() {
    set_callbacks_once = true;

    nobusywait = !!cv::fps_limiter_nobusywait.getVal<int>();
    max_yield = !!cv::fps_max_yield.getVal<int>();
    unlimited_yield = !!cv::fps_unlimited_yield.getVal<int>();

    cv::fps_limiter_nobusywait.setCallback([](float newv) { nobusywait = !!(int)newv; });
    cv::fps_max_yield.setCallback([](float newv) { max_yield = !!(int)newv; });
    cv::fps_unlimited_yield.setCallback([](float newv) { unlimited_yield = !!(int)newv; });
}

}  // namespace

void limit_frames(int target_fps) {
    if(!set_callbacks_once) {
        set_callbacks();
    }

    if(target_fps > 0) {
        const u64 frame_time_ns = Timing::NS_PER_SECOND / static_cast<u64>(target_fps);
        const u64 now = Timing::getTicksNS();

        // if we're ahead of schedule, sleep until next frame
        if(next_frame_time > now) {
            if(nobusywait) {  // sleep for 1ms every so often to reach the frametime target
                const u64 sleep_time_ns = next_frame_time - now;
                const u64 sleep_time_ms = sleep_time_ns / Timing::NS_PER_MS;

                if(sleep_time_ms > 0) {
                    Timing::sleepMS(sleep_time_ms);
                } else if(max_yield) {
                    Timing::sleep(0);
                }
            } else {  // precise sleeps per-frame
                // never sleep more than the current target fps frame time
                const u64 sleep_time = std::min(next_frame_time - now, frame_time_ns);
                Timing::sleepNSPrecise(sleep_time);
            }
        } else if(max_yield) {
            Timing::sleep(0);
            next_frame_time = Timing::getTicksNS();  // update "now" to reflect the time spent in yield
        } else {
            // behind schedule or exactly on time, reset to now
            next_frame_time = now;
        }
        // set time for next frame
        next_frame_time += frame_time_ns;
    } else if(unlimited_yield) {
        Timing::sleep(0);
    }
}

void reset() { next_frame_time = 0; }

}  // namespace FPSLimiter
