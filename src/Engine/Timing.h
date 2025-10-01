//========== Copyright (c) 2015, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		stopwatch/timer
//
// $NoKeywords: $time $chrono
//===============================================================================//

#pragma once
#ifndef TIMER_H
#define TIMER_H

#include "BaseEnvironment.h"
#include "types.h"

#include <SDL3/SDL_timer.h>

#include <thread>
#include <concepts>

namespace Timing {
// conversion constants
constexpr u64 NS_PER_SECOND = 1'000'000'000;
constexpr u64 NS_PER_MS = 1'000'000;
constexpr u64 NS_PER_US = 1'000;
constexpr u64 US_PER_MS = 1'000;
constexpr u64 MS_PER_SECOND = 1'000;

namespace detail {
inline INLINE_BODY void yield_internal() noexcept {
    if constexpr(Env::cfg(OS::WASM))
        SDL_DelayNS(0);
    else
        std::this_thread::yield();
}

template <u64 Ratio>
static constexpr forceinline INLINE_BODY u64 convert_time(u64 ns) noexcept {
    return ns / Ratio;
}

template <typename T = double>
    requires(std::floating_point<T>)
static constexpr forceinline INLINE_BODY T time_ns_to_secs(u64 ns) noexcept {
    return static_cast<T>(ns) / static_cast<T>(NS_PER_SECOND);
}

void sleep_ns_internal(u64 ns) noexcept;
void sleep_ns_precise_internal(u64 ns) noexcept;

}  // namespace detail

inline INLINE_BODY u64 getTicksNS() noexcept { return SDL_GetTicksNS(); }

static constexpr forceinline INLINE_BODY u64 ticksNSToMS(u64 ns) noexcept {
    return detail::convert_time<NS_PER_MS>(ns);
}

inline INLINE_BODY u64 getTicksMS() noexcept { return ticksNSToMS(getTicksNS()); }

inline INLINE_BODY void sleep(u64 us) noexcept {
    us > 0 ? detail::sleep_ns_internal(us * NS_PER_US) : detail::yield_internal();
}

inline INLINE_BODY void sleepNS(u64 ns) noexcept { ns > 0 ? detail::sleep_ns_internal(ns) : detail::yield_internal(); }

inline INLINE_BODY void sleepMS(u64 ms) noexcept {
    ms > 0 ? detail::sleep_ns_internal(ms * NS_PER_MS) : detail::yield_internal();
}

inline INLINE_BODY void sleepPrecise(u64 us) noexcept {
    us > 0 ? detail::sleep_ns_precise_internal(us * NS_PER_US) : detail::yield_internal();
}

inline INLINE_BODY void sleepNSPrecise(u64 ns) noexcept {
    ns > 0 ? detail::sleep_ns_precise_internal(ns) : detail::yield_internal();
}

inline INLINE_BODY void sleepMSPrecise(u64 ms) noexcept {
    ms > 0 ? detail::sleep_ns_precise_internal(ms * NS_PER_MS) : detail::yield_internal();
}

// current time (since init.) in seconds as float
// decoupled from engine updates!
template <typename T = double>
    requires(std::floating_point<T>)
constexpr forceinline INLINE_BODY T getTimeReal() noexcept {
    return detail::time_ns_to_secs<T>(getTicksNS());
}

class Timer {
   public:
    inline void start(bool startFromZero = false) noexcept {
        this->startTimeNS = startFromZero ? 0 : getTicksNS();
        this->lastUpdateNS = this->startTimeNS;
        this->deltaSeconds = 0.0;
    }

    inline void update() noexcept {
        const u64 now = getTicksNS();
        this->deltaSeconds = detail::time_ns_to_secs<double>(now - this->lastUpdateNS);
        this->lastUpdateNS = now;
    }

    inline void reset(bool startFromZero = false) noexcept {
        this->startTimeNS = startFromZero ? 0 : getTicksNS();
        this->lastUpdateNS = this->startTimeNS;
        this->deltaSeconds = 0.0;
    }

    [[nodiscard]] constexpr double getDelta() const noexcept { return this->deltaSeconds; }

    [[nodiscard]] inline double getElapsedTime() const noexcept {
        return detail::time_ns_to_secs<double>(this->lastUpdateNS - this->startTimeNS);
    }

    [[nodiscard]] inline u64 getElapsedTimeMS() const noexcept {
        return ticksNSToMS(this->lastUpdateNS - this->startTimeNS);
    }

    [[nodiscard]] inline u64 getElapsedTimeNS() const noexcept { return this->lastUpdateNS - this->startTimeNS; }

    // get elapsed time without needing update()
    [[nodiscard]] inline double getLiveElapsedTime() const noexcept {
        return detail::time_ns_to_secs<double>(getTicksNS() - this->startTimeNS);
    }

    [[nodiscard]] inline u64 getLiveElapsedTimeNS() const noexcept { return getTicksNS() - this->startTimeNS; }

   private:
    u64 startTimeNS{};
    u64 lastUpdateNS{};
    double deltaSeconds{};
};

};  // namespace Timing

#ifdef MCENGINE_PLATFORM_WINDOWS
#include <ctime>

// thread-safe versions of gmtime, localtime
struct tm *gmtime_x(const time_t *timer, struct tm *timebuf);
struct tm *localtime_x(const time_t *timer, struct tm *timebuf);

#else

#define gmtime_x gmtime_r
#define localtime_x localtime_r

#endif

using Timer = Timing::Timer;

#endif
