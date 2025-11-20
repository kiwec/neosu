// Copyright (c) 2025, WH, All rights reserved.
#include "Timing.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <libloaderapi.h>
#include <winnt.h>
#include <profileapi.h>
#include "dynutils.h"

#include "ConVar.h"

#include <cassert>
#endif

namespace Timing::detail {
#ifdef MCENGINE_PLATFORM_WINDOWS
namespace {  // static

using NtDelayExecution_t = LONG NTAPI(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
using NtQueryTimerResolution_t = LONG NTAPI(PULONG MaximumTime, PULONG MinimumTime, PULONG CurrentTime);
using NtSetTimerResolution_t = LONG NTAPI(ULONG DesiredTime, BOOLEAN SetResolution, PULONG ActualTime);

NtDelayExecution_t *pNtDelayExecution{nullptr};
NtQueryTimerResolution_t *pNtQueryTimerResolution{nullptr};
NtSetTimerResolution_t *pNtSetTimerResolution{nullptr};

u64 s_actual_delay_amount{0};  // minimum time NtDelayExecution actually sleeps for in nanoseconds
bool s_use_ntdelayexec{false};
bool s_force_use_sdl_sleep{false};

bool s_sleeper_initialized{false};

void measure_actual_ntdelayexecution_time() noexcept {
    constexpr i32 num_samples = 3;  // 3 samples per delay amount
    constexpr const auto test_delays = std::array{50000ULL, 100000ULL, 250000ULL, 500000ULL};  // 0.05ms to 0.25ms

    u64 min_actual_sleep = UINT64_MAX;

    for(const auto test_delay_ns : test_delays) {
        u64 total_actual_sleep = 0;
        i32 valid_samples = 0;

        for(i32 i = 0; i < num_samples; ++i) {
            const u64 start_time = getTicksNS();

            LARGE_INTEGER sleep_ticks{.QuadPart = -static_cast<LONGLONG>(test_delay_ns / 100LL)};
            pNtDelayExecution(0, static_cast<PLARGE_INTEGER>(&sleep_ticks));

            const u64 end_time = getTicksNS();

            const u64 actual_delay = end_time - start_time;
            if(actual_delay >= test_delay_ns / 2 && actual_delay < test_delay_ns * 4) {  // sanity check
                total_actual_sleep += actual_delay;
                valid_samples++;
            }
        }

        if(valid_samples > 0) {
            const u64 avg_actual_sleep = total_actual_sleep / valid_samples;
            if(avg_actual_sleep < min_actual_sleep) {
                min_actual_sleep = avg_actual_sleep;
            }
        }
    }

    if(min_actual_sleep != UINT64_MAX) {
        // add 200usec artificial overhead to be safe (prefer busy waiting instead of oversleeping)
        s_actual_delay_amount = min_actual_sleep + 200000;
        // cap at 2ms
        if(s_actual_delay_amount > 2000000) {
            s_actual_delay_amount = 2000000;
        }
    }
    // else if we failed, m_actualDelayAmount will == the timer resolution
}

void init_sleeper() {
    auto *ntdll_handle{reinterpret_cast<dynutils::lib_obj *>(GetModuleHandle(TEXT("ntdll.dll")))};
    if(ntdll_handle) {
        pNtDelayExecution = dynutils::load_func<NtDelayExecution_t>(ntdll_handle, "NtDelayExecution");
        pNtQueryTimerResolution = dynutils::load_func<NtQueryTimerResolution_t>(ntdll_handle, "NtQueryTimerResolution");
        pNtSetTimerResolution = dynutils::load_func<NtSetTimerResolution_t>(ntdll_handle, "NtSetTimerResolution");
    }

    if(!!pNtDelayExecution && !!pNtQueryTimerResolution && !!pNtSetTimerResolution) {
        ULONG max_res, min_res, current_res;
        if(pNtQueryTimerResolution(&max_res, &min_res, &current_res) >= 0) {
            ULONG actual_res;
            if(pNtSetTimerResolution(min_res, TRUE, &actual_res) >= 0 && actual_res <= 10000) {
                // only enable if we achieved <= 1ms resolution (10000 * 100ns units)
                s_use_ntdelayexec = true;
                s_actual_delay_amount = actual_res * 100ULL;
                measure_actual_ntdelayexecution_time();
            }
        }
    }

    // register callback instead of using getBool because convars are sub-optimal for precise timing (cache coherency)
    cv::alt_sleep.setCallback([](float newVal) -> void { s_force_use_sdl_sleep = !static_cast<i32>(newVal); });
}

}  // namespace

void sleep_ns_internal(u64 ns) noexcept {
    if (!s_sleeper_initialized) {
        s_sleeper_initialized = true;
        init_sleeper();
    }

    if(s_force_use_sdl_sleep || !s_use_ntdelayexec) {
        SDL_DelayNS(ns);
        return;
    }

    LARGE_INTEGER sleep_ticks{.QuadPart = -static_cast<LONGLONG>(ns / 100LL)};
    pNtDelayExecution(0, static_cast<PLARGE_INTEGER>(&sleep_ticks));
}

void sleep_ns_precise_internal(u64 ns) noexcept {
    if (!s_sleeper_initialized) {
        s_sleeper_initialized = true;
        init_sleeper();
    }

    if(s_force_use_sdl_sleep || !s_use_ntdelayexec) {
        SDL_DelayPrecise(ns);
        return;
    }

    // use NtDelayExecution for bulk delay, busy-wait for remainder
    const u64 target_time = getTicksNS() + ns;
    if(ns > s_actual_delay_amount) {
        // get "remainder", time which the sleep resolution might not handle
        // e.g. 0.51ms with 0.5ms minimum: NtDelayExecution for exactly 1 x 0.5ms = 0.5ms, busy wait for 0.01ms remainder
        //      1.25ms with 0.5ms minimum: NtDelayExecution for exactly 2 x 0.5ms = 1ms, busy wait for 0.25ms remainder
        const u64 bulk_delay = (ns / s_actual_delay_amount) * s_actual_delay_amount;

        LARGE_INTEGER sleep_ticks{.QuadPart = -static_cast<LONGLONG>(bulk_delay / 100LL)};
        pNtDelayExecution(0, static_cast<PLARGE_INTEGER>(&sleep_ticks));
    }

    // busy-wait remainder
    while(getTicksNS() < target_time) {
        YieldProcessor();
    }
}

#else
void sleep_ns_internal(u64 ns) noexcept { SDL_DelayNS(ns); }
void sleep_ns_precise_internal(u64 ns) noexcept { SDL_DelayPrecise(ns); }
#endif
}  // namespace Timing::detail

#ifdef MCENGINE_PLATFORM_WINDOWS

struct tm *gmtime_x(const time_t *timer, struct tm *timebuf) {
    _gmtime64_s(timebuf, timer);
    return timebuf;
}

struct tm *localtime_x(const time_t *timer, struct tm *timebuf) {
    _localtime64_s(timebuf, timer);
    return timebuf;
}

#endif