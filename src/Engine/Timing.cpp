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

#include <cassert>
#include <ctime>
#endif

namespace Timing::detail {
#ifdef MCENGINE_PLATFORM_WINDOWS
namespace {  // static namespace
class SleepHandler final {
    NOCOPY_NOMOVE(SleepHandler)
   public:
    SleepHandler() noexcept {
        auto *ntdll_handle{reinterpret_cast<dynutils::lib_obj *>(GetModuleHandle(TEXT("ntdll.dll")))};
        if(ntdll_handle) {
            m_pNtDelayExec = dynutils::load_func<NtDelayExecution_t>(ntdll_handle, "NtDelayExecution");
            m_pNtQueryTimerRes = dynutils::load_func<NtQueryTimerResolution_t>(ntdll_handle, "NtQueryTimerResolution");
            m_pNtSetTimerRes = dynutils::load_func<NtSetTimerResolution_t>(ntdll_handle, "NtSetTimerResolution");
        }

        if(!!m_pNtDelayExec && !!m_pNtQueryTimerRes && !!m_pNtSetTimerRes) {
            ULONG maxRes, minRes, currentRes;
            if(m_pNtQueryTimerRes(&maxRes, &minRes, &currentRes) >= 0) {
                ULONG actualRes;
                if(m_pNtSetTimerRes(minRes, TRUE, &actualRes) >= 0 && actualRes <= 10000) {
                    // only enable if we achieved <= 1ms resolution (10000 * 100ns units)
                    m_bUseNTDelayExec = true;
                    m_actualDelayAmount = actualRes * 100ULL;
                    measureActualDelay();  // get accurate NtDelayExecution time
                }
            }
        }

        // register callback instead of using getBool because convars are slow now...
        cv::alt_sleep.setCallback(SA::MakeDelegate<&SleepHandler::forceDisable>(this));
    }
    ~SleepHandler() = default;

    forceinline void imprecise(uint64_t ns) const noexcept {
        if(m_bForceDisable || !m_bUseNTDelayExec) {
            SDL_DelayNS(ns);
            return;
        }

        LARGE_INTEGER sleepTicks{.QuadPart = -static_cast<LONGLONG>(ns / 100LL)};
        m_pNtDelayExec(0, static_cast<PLARGE_INTEGER>(&sleepTicks));
    }

    forceinline void precise(uint64_t ns) const noexcept {
        if(m_bForceDisable || !m_bUseNTDelayExec) {
            SDL_DelayPrecise(ns);
            return;
        }

        // use NtDelayExecution for bulk delay, busy-wait for remainder
        const uint64_t targetTime = getTicksNS() + ns;
        if(ns > m_actualDelayAmount) {
            // get "remainder", time which the sleep resolution might not handle
            // e.g. 0.51ms with 0.5ms minimum: NtDelayExecution for exactly 1 x 0.5ms = 0.5ms, busy wait for 0.01ms remainder
            //      1.25ms with 0.5ms minimum: NtDelayExecution for exactly 2 x 0.5ms = 1ms, busy wait for 0.25ms remainder
            const uint64_t bulkDelay = (ns / m_actualDelayAmount) * m_actualDelayAmount;

            LARGE_INTEGER sleepTicks{.QuadPart = -static_cast<LONGLONG>(bulkDelay / 100LL)};
            m_pNtDelayExec(0, static_cast<PLARGE_INTEGER>(&sleepTicks));
        }

        // busy-wait remainder
        while(getTicksNS() < targetTime) {
            YieldProcessor();
        }
    }

   private:
    void measureActualDelay() noexcept {
        // measure the minimum time NtDelayExecution actually sleeps for
        constexpr int numSamples = 3;  // 3 samples per delay amount
        constexpr const auto testDelays = std::array{50000ULL, 100000ULL, 250000ULL, 500000ULL};  // 0.05ms to 0.25ms

        uint64_t minActualSleep = UINT64_MAX;

        for(const auto testDelayNs : testDelays) {
            uint64_t totalActual = 0;
            int validSamples = 0;

            for(int i = 0; i < numSamples; ++i) {
                uint64_t startTime = getTicksNS();

                LARGE_INTEGER sleepTicks{.QuadPart = -static_cast<LONGLONG>(testDelayNs / 100LL)};
                m_pNtDelayExec(0, static_cast<PLARGE_INTEGER>(&sleepTicks));

                uint64_t endTime = getTicksNS();

                uint64_t actualDelay = endTime - startTime;
                if(actualDelay >= testDelayNs / 2 && actualDelay < testDelayNs * 4) {  // sanity check
                    totalActual += actualDelay;
                    validSamples++;
                }
            }

            if(validSamples > 0) {
                uint64_t avgActual = totalActual / validSamples;
                if(avgActual < minActualSleep) {
                    minActualSleep = avgActual;
                }
            }
        }

        if(minActualSleep != UINT64_MAX) {
            // add 200usec artificial overhead to be safe (prefer busy waiting instead of oversleeping)
            m_actualDelayAmount = minActualSleep + 200000;
            // cap at 2ms
            if(m_actualDelayAmount > 2000000) {
                m_actualDelayAmount = 2000000;
            }
        }
        // else if we failed, m_actualDelayAmount will == the timer resolution
    }

    // callback
    void forceDisable(float newVal) { m_bForceDisable = !static_cast<int>(newVal); }

    using NtDelayExecution_t = LONG NTAPI(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
    using NtQueryTimerResolution_t = LONG NTAPI(PULONG MaximumTime, PULONG MinimumTime, PULONG CurrentTime);
    using NtSetTimerResolution_t = LONG NTAPI(ULONG DesiredTime, BOOLEAN SetResolution, PULONG ActualTime);

    NtDelayExecution_t *m_pNtDelayExec{nullptr};
    NtQueryTimerResolution_t *m_pNtQueryTimerRes{nullptr};
    NtSetTimerResolution_t *m_pNtSetTimerRes{nullptr};

    uint64_t m_actualDelayAmount{0};  // minimum time NtDelayExecution actually sleeps for in nanoseconds
    bool m_bUseNTDelayExec{false};
    bool m_bForceDisable{false};
};

// so we can run ctor/dtors on program init/shutdown
static forceinline const SleepHandler &getSleeper() {
    // lazy init (static initialization order fiasco)
    static SleepHandler sleeper;
    return sleeper;
}

}  // namespace

void sleep_ns_internal(uint64_t ns) noexcept { getSleeper().imprecise(ns); }
void sleep_ns_precise_internal(uint64_t ns) noexcept { getSleeper().precise(ns); }

#else
void sleep_ns_internal(uint64_t ns) noexcept { SDL_DelayNS(ns); }
void sleep_ns_precise_internal(uint64_t ns) noexcept { SDL_DelayPrecise(ns); }
#endif
}  // namespace Timing::detail

#ifdef MCENGINE_PLATFORM_WINDOWS

struct tm *gmtime_x(const int64_t *timer, struct tm *timebuf) {
    _gmtime64_s(timebuf, timer);
    return timebuf;
}

struct tm *localtime_x(const int64_t *timer, struct tm *timebuf) {
    _localtime64_s(timebuf, timer);
    return timebuf;
}

#endif