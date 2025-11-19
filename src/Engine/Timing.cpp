// Copyright (c) 2025, WH, All rights reserved.
#include "Timing.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <libloaderapi.h>
#include <winnt.h>
#include <profileapi.h>
#include "dynutils.h"

#include "MakeDelegateWrapper.h"
#include "ConVar.h"
#include "SyncOnce.h"

#include <cassert>
#endif

namespace Timing::detail {
#ifdef MCENGINE_PLATFORM_WINDOWS
namespace {  // static

Sync::once_flag sleeper_initialized;

using NtDelayExecution_t = LONG NTAPI(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
using NtQueryTimerResolution_t = LONG NTAPI(PULONG MaximumTime, PULONG MinimumTime, PULONG CurrentTime);
using NtSetTimerResolution_t = LONG NTAPI(ULONG DesiredTime, BOOLEAN SetResolution, PULONG ActualTime);

NtDelayExecution_t *pNtDelayExec{nullptr};
NtQueryTimerResolution_t *pNtQueryTimerRes{nullptr};
NtSetTimerResolution_t *pNtSetTimerRes{nullptr};

u64 actualDelayAmount{0};  // minimum time NtDelayExecution actually sleeps for in nanoseconds
bool bUseNTDelayExec{false};
bool bForceDisable{false};

void measureActualDelay() noexcept {
    // measure the minimum time NtDelayExecution actually sleeps for
    constexpr i32 numSamples = 3;  // 3 samples per delay amount
    constexpr const auto testDelays = std::array{50000ULL, 100000ULL, 250000ULL, 500000ULL};  // 0.05ms to 0.25ms

    u64 minActualSleep = UINT64_MAX;

    for(const auto testDelayNs : testDelays) {
        u64 totalActual = 0;
        i32 validSamples = 0;

        for(i32 i = 0; i < numSamples; ++i) {
            u64 startTime = getTicksNS();

            LARGE_INTEGER sleepTicks{.QuadPart = -static_cast<LONGLONG>(testDelayNs / 100LL)};
            pNtDelayExec(0, static_cast<PLARGE_INTEGER>(&sleepTicks));

            u64 endTime = getTicksNS();

            u64 actualDelay = endTime - startTime;
            if(actualDelay >= testDelayNs / 2 && actualDelay < testDelayNs * 4) {  // sanity check
                totalActual += actualDelay;
                validSamples++;
            }
        }

        if(validSamples > 0) {
            u64 avgActual = totalActual / validSamples;
            if(avgActual < minActualSleep) {
                minActualSleep = avgActual;
            }
        }
    }

    if(minActualSleep != UINT64_MAX) {
        // add 200usec artificial overhead to be safe (prefer busy waiting instead of oversleeping)
        actualDelayAmount = minActualSleep + 200000;
        // cap at 2ms
        if(actualDelayAmount > 2000000) {
            actualDelayAmount = 2000000;
        }
    }
    // else if we failed, m_actualDelayAmount will == the timer resolution
}

void init_sleeper() {
    auto *ntdll_handle{reinterpret_cast<dynutils::lib_obj *>(GetModuleHandle(TEXT("ntdll.dll")))};
    if(ntdll_handle) {
        pNtDelayExec = dynutils::load_func<NtDelayExecution_t>(ntdll_handle, "NtDelayExecution");
        pNtQueryTimerRes = dynutils::load_func<NtQueryTimerResolution_t>(ntdll_handle, "NtQueryTimerResolution");
        pNtSetTimerRes = dynutils::load_func<NtSetTimerResolution_t>(ntdll_handle, "NtSetTimerResolution");
    }

    if(!!pNtDelayExec && !!pNtQueryTimerRes && !!pNtSetTimerRes) {
        ULONG maxRes, minRes, currentRes;
        if(pNtQueryTimerRes(&maxRes, &minRes, &currentRes) >= 0) {
            ULONG actualRes;
            if(pNtSetTimerRes(minRes, TRUE, &actualRes) >= 0 && actualRes <= 10000) {
                // only enable if we achieved <= 1ms resolution (10000 * 100ns units)
                bUseNTDelayExec = true;
                actualDelayAmount = actualRes * 100ULL;
                measureActualDelay();  // get accurate NtDelayExecution time
            }
        }
    }

    // register callback instead of using getBool because convars are sub-optimal for precise timing now
    cv::alt_sleep.setCallback([](float newVal) -> void { bForceDisable = !static_cast<i32>(newVal); });
}

}  // namespace

void sleep_ns_internal(u64 ns) noexcept {
    Sync::call_once(sleeper_initialized, []() -> void { init_sleeper(); });

    if(bForceDisable || !bUseNTDelayExec) {
        SDL_DelayNS(ns);
        return;
    }

    LARGE_INTEGER sleepTicks{.QuadPart = -static_cast<LONGLONG>(ns / 100LL)};
    pNtDelayExec(0, static_cast<PLARGE_INTEGER>(&sleepTicks));
}

void sleep_ns_precise_internal(u64 ns) noexcept {
    Sync::call_once(sleeper_initialized, []() -> void { init_sleeper(); });

    if(bForceDisable || !bUseNTDelayExec) {
        SDL_DelayPrecise(ns);
        return;
    }

    // use NtDelayExecution for bulk delay, busy-wait for remainder
    const u64 targetTime = getTicksNS() + ns;
    if(ns > actualDelayAmount) {
        // get "remainder", time which the sleep resolution might not handle
        // e.g. 0.51ms with 0.5ms minimum: NtDelayExecution for exactly 1 x 0.5ms = 0.5ms, busy wait for 0.01ms remainder
        //      1.25ms with 0.5ms minimum: NtDelayExecution for exactly 2 x 0.5ms = 1ms, busy wait for 0.25ms remainder
        const u64 bulkDelay = (ns / actualDelayAmount) * actualDelayAmount;

        LARGE_INTEGER sleepTicks{.QuadPart = -static_cast<LONGLONG>(bulkDelay / 100LL)};
        pNtDelayExec(0, static_cast<PLARGE_INTEGER>(&sleepTicks));
    }

    // busy-wait remainder
    while(getTicksNS() < targetTime) {
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