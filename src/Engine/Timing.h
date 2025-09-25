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

#include <SDL3/SDL_timer.h>

#include <thread>
#include <concepts>
#include <cstdint>

namespace Timing {
// conversion constants
constexpr uint64_t NS_PER_SECOND = 1'000'000'000;
constexpr uint64_t NS_PER_MS = 1'000'000;
constexpr uint64_t NS_PER_US = 1'000;
constexpr uint64_t US_PER_MS = 1'000;
constexpr uint64_t MS_PER_SECOND = 1'000;

namespace detail {
inline INLINE_BODY void yield_internal() noexcept {
    if constexpr(Env::cfg(OS::WASM))
        SDL_DelayNS(0);
    else
        std::this_thread::yield();
}

template <uint64_t Ratio>
static constexpr forceinline INLINE_BODY uint64_t convert_time(uint64_t ns) noexcept {
    return ns / Ratio;
}

template <typename T = double>
    requires(std::floating_point<T>)
static constexpr forceinline INLINE_BODY T time_ns_to_secs(uint64_t ns) noexcept {
    return static_cast<T>(ns) / static_cast<T>(NS_PER_SECOND);
}

void sleep_ns_internal(uint64_t ns) noexcept;
void sleep_ns_precise_internal(uint64_t ns) noexcept;

}  // namespace detail

inline INLINE_BODY uint64_t getTicksNS() noexcept { return SDL_GetTicksNS(); }

static constexpr forceinline INLINE_BODY uint64_t ticksNSToMS(uint64_t ns) noexcept {
    return detail::convert_time<NS_PER_MS>(ns);
}

inline INLINE_BODY uint64_t getTicksMS() noexcept { return ticksNSToMS(getTicksNS()); }

inline INLINE_BODY void sleep(uint64_t us) noexcept {
    us > 0 ? detail::sleep_ns_internal(us * NS_PER_US) : detail::yield_internal();
}

inline INLINE_BODY void sleepNS(uint64_t ns) noexcept {
    ns > 0 ? detail::sleep_ns_internal(ns) : detail::yield_internal();
}

inline INLINE_BODY void sleepMS(uint64_t ms) noexcept {
    ms > 0 ? detail::sleep_ns_internal(ms * NS_PER_MS) : detail::yield_internal();
}

inline INLINE_BODY void sleepPrecise(uint64_t us) noexcept {
    us > 0 ? detail::sleep_ns_precise_internal(us * NS_PER_US) : detail::yield_internal();
}

inline INLINE_BODY void sleepNSPrecise(uint64_t ns) noexcept {
    ns > 0 ? detail::sleep_ns_precise_internal(ns) : detail::yield_internal();
}

inline INLINE_BODY void sleepMSPrecise(uint64_t ms) noexcept {
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
        const uint64_t now = getTicksNS();
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

    [[nodiscard]] inline uint64_t getElapsedTimeMS() const noexcept {
        return ticksNSToMS(this->lastUpdateNS - this->startTimeNS);
    }

    [[nodiscard]] inline uint64_t getElapsedTimeNS() const noexcept { return this->lastUpdateNS - this->startTimeNS; }

    // get elapsed time without needing update()
    [[nodiscard]] inline double getLiveElapsedTime() const noexcept {
        return detail::time_ns_to_secs<double>(getTicksNS() - this->startTimeNS);
    }

    [[nodiscard]] inline uint64_t getLiveElapsedTimeNS() const noexcept { return getTicksNS() - this->startTimeNS; }

   private:
    uint64_t startTimeNS{};
    uint64_t lastUpdateNS{};
    double deltaSeconds{};
};

};  // namespace Timing

#ifdef MCENGINE_PLATFORM_WINDOWS
#include <ctime>

// thread-safe versions of gmtime, localtime
struct tm *gmtime_x(const int64_t *timer, struct tm *timebuf);
struct tm *localtime_x(const int64_t *timer, struct tm *timebuf);

#else

#define gmtime_x gmtime_r
#define localtime_x localtime_r

#endif


using Timer = Timing::Timer;

#endif
