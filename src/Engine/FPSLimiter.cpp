// Copyright (c) 2025, WH, All rights reserved.
#include "FPSLimiter.h"
#include "Timing.h"
#include "ConVar.h"
#include "types.h"

#include <cassert>

namespace FPSLimiter {
namespace  // static
{

// callbacks
bool s_set_callbacks_once{false};

bool s_nobusywait{false};
bool s_max_yield{false};
bool s_unlimited_yield{false};

// state
u64 s_next_frame_time{0};

void set_callbacks() {
    s_nobusywait = !!cv::fps_limiter_nobusywait.getVal<int>();
    s_max_yield = !!cv::fps_max_yield.getVal<int>();
    s_unlimited_yield = !!cv::fps_unlimited_yield.getVal<int>();

    cv::fps_limiter_nobusywait.setCallback([](float newv) { s_nobusywait = !!(int)newv; });
    cv::fps_max_yield.setCallback([](float newv) { s_max_yield = !!(int)newv; });
    cv::fps_unlimited_yield.setCallback([](float newv) { s_unlimited_yield = !!(int)newv; });
}

}  // namespace

void limit_frames(int target_fps) {
    if(!s_set_callbacks_once) {
        s_set_callbacks_once = true;
        set_callbacks();
    }

    if(target_fps > 0) {
        const u64 frame_time_ns = Timing::NS_PER_SECOND / static_cast<u64>(target_fps);
        const u64 now = Timing::getTicksNS();

        // if we're ahead of schedule, sleep until next frame
        if(s_next_frame_time > now) {
            if(s_nobusywait) {  // sleep for 1ms every so often to reach the frametime target
                const u64 sleep_time_ns = s_next_frame_time - now;
                const u64 sleep_time_ms = sleep_time_ns / Timing::NS_PER_MS;

                if(sleep_time_ms > 0) {
                    Timing::sleepMS(sleep_time_ms);
                } else if(s_max_yield) {
                    Timing::sleep(0);
                }
            } else {  // precise sleeps per-frame
                // never sleep more than the current target fps frame time
                const u64 sleep_time = std::min(s_next_frame_time - now, frame_time_ns);
                Timing::sleepNSPrecise(sleep_time);
            }
        } else if(s_max_yield) {
            Timing::sleep(0);
            s_next_frame_time = Timing::getTicksNS();  // update "now" to reflect the time spent in yield
        } else {
            // behind schedule or exactly on time, reset to now
            s_next_frame_time = now;
        }
        // set time for next frame
        s_next_frame_time += frame_time_ns;
    } else if(s_unlimited_yield) {
        Timing::sleep(0);
    }
}

void reset() { s_next_frame_time = 0; }

}  // namespace FPSLimiter
