// Copyright (c) 2025, WH, All rights reserved.
#pragma once
// condition_variable + condition_variable_any

#include "config.h"

#include "SyncMutex.h"
#include "SyncStoptoken.h"

#ifdef USE_NSYNC
#include "nsync_cv.h"
#endif  // USE_NSYNC

#include <condition_variable>

namespace Sync {
#ifdef USE_NSYNC
using namespace nsync;

// ===================================================================
// condition_variable: for nsync mutex only
// ===================================================================
class nsync_condition_variable_t {
   private:
    nsync_cv m_cv{};

   public:
    constexpr nsync_condition_variable_t() noexcept = default;
    ~nsync_condition_variable_t() = default;

    nsync_condition_variable_t(const nsync_condition_variable_t&) = delete;
    nsync_condition_variable_t& operator=(const nsync_condition_variable_t&) = delete;
    nsync_condition_variable_t(nsync_condition_variable_t&&) = delete;
    nsync_condition_variable_t& operator=(nsync_condition_variable_t&&) = delete;

    void notify_one() noexcept { nsync_cv_signal(&m_cv); }
    void notify_all() noexcept { nsync_cv_broadcast(&m_cv); }

    void wait(unique_lock<mutex>& lock) { nsync_cv_wait(&m_cv, lock.mutex()->native_handle()); }

    template <typename Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        while(!pred()) {
            wait(lock);
        }
    }

    template <typename Clock, typename Duration>
    std::cv_status wait_until(unique_lock<mutex>& lock, const std::chrono::time_point<Clock, Duration>& timeout_time) {
        auto now = Clock::now();
        if(timeout_time <= now) {
            return std::cv_status::timeout;
        }

        auto timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout_time - now);
        nsync_time deadline = nsync_time_add(nsync_time_now(), nsync_time_s_ns(0, timeout_ns.count()));

        int result = nsync_cv_wait_with_deadline(&m_cv, lock.mutex()->native_handle(), deadline, nullptr);
        return result == 0 ? std::cv_status::no_timeout : std::cv_status::timeout;
    }

    template <typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock<mutex>& lock, const std::chrono::time_point<Clock, Duration>& timeout_time,
                    Predicate pred) {
        while(!pred()) {
            if(wait_until(lock, timeout_time) == std::cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    template <typename Rep, typename Period>
    std::cv_status wait_for(unique_lock<mutex>& lock, const std::chrono::duration<Rep, Period>& timeout_duration) {
        return wait_until(lock, std::chrono::steady_clock::now() + timeout_duration);
    }

    template <typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock<mutex>& lock, const std::chrono::duration<Rep, Period>& timeout_duration,
                  Predicate pred) {
        return wait_until(lock, std::chrono::steady_clock::now() + timeout_duration, pred);
    }
};

// ===================================================================
// condition_variable_any: condition variable for any lockable type
// ===================================================================
class nsync_condition_variable_any_t {
   private:
    nsync_cv m_cv{};
    std::shared_ptr<nsync_mutex_t> m_mutex;

    template <typename Lock>
    struct unlock_guard {
        explicit unlock_guard(Lock& lock) : m_lock(lock) { m_lock.unlock(); }
        ~unlock_guard() noexcept(false) { m_lock.lock(); }

        unlock_guard(const unlock_guard&) = delete;
        unlock_guard& operator=(const unlock_guard&) = delete;
        unlock_guard(unlock_guard&&) = delete;
        unlock_guard& operator=(unlock_guard&&) = delete;

        Lock& m_lock;
    };

   public:
    constexpr nsync_condition_variable_any_t() : m_mutex(std::make_shared<nsync_mutex_t>()) {}
    ~nsync_condition_variable_any_t() = default;

    nsync_condition_variable_any_t(const nsync_condition_variable_any_t&) = delete;
    nsync_condition_variable_any_t& operator=(const nsync_condition_variable_any_t&) = delete;
    nsync_condition_variable_any_t(nsync_condition_variable_any_t&&) = delete;
    nsync_condition_variable_any_t& operator=(nsync_condition_variable_any_t&&) = delete;

    void notify_one() noexcept {
        lock_guard<nsync_mutex_t> lock(*m_mutex);
        nsync_cv_signal(&m_cv);
    }

    void notify_all() noexcept {
        lock_guard<nsync_mutex_t> lock(*m_mutex);
        nsync_cv_broadcast(&m_cv);
    }

    // basic wait operations
    template <typename Lock>
    void wait(Lock& lock) {
        std::shared_ptr<nsync_mutex_t> mutex = m_mutex;
        unique_lock<nsync_mutex_t> internal_lock(*mutex);
        unlock_guard<Lock> unlock_user(lock);
        // move ownership to shorter lifetime to ensure proper unlock order
        unique_lock<nsync_mutex_t> internal_lock2(std::move(internal_lock));
        nsync_cv_wait(&m_cv, internal_lock2.mutex()->native_handle());
        // internal_lock2 destructor runs first (releases internal mutex)
        // then unlock_guard destructor runs (reacquires user mutex)
    }

    template <typename Lock, typename Predicate>
    void wait(Lock& lock, Predicate pred) {
        while(!pred()) {
            wait(lock);
        }
    }

    // wait_until operations
    template <typename Lock, typename Clock, typename Duration>
    std::cv_status wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& timeout_time) {
        auto now = Clock::now();
        if(timeout_time <= now) {
            return std::cv_status::timeout;
        }

        auto timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout_time - now);
        nsync_time deadline = nsync_time_add(nsync_time_now(), nsync_time_s_ns(0, timeout_ns.count()));

        std::shared_ptr<nsync_mutex_t> mutex = m_mutex;
        unique_lock<nsync_mutex_t> internal_lock(*mutex);
        unlock_guard<Lock> unlock_user(lock);
        // move ownership to shorter lifetime to ensure proper unlock order
        unique_lock<nsync_mutex_t> internal_lock2(std::move(internal_lock));
        int result = nsync_cv_wait_with_deadline(&m_cv, internal_lock2.mutex()->native_handle(), deadline, nullptr);
        return result == 0 ? std::cv_status::no_timeout : std::cv_status::timeout;
    }

    template <typename Lock, typename Clock, typename Duration, typename Predicate>
    bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& timeout_time, Predicate pred) {
        while(!pred()) {
            if(wait_until(lock, timeout_time) == std::cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    // wait_for operations
    template <typename Lock, typename Rep, typename Period>
    std::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& timeout_duration) {
        return wait_until(lock, std::chrono::steady_clock::now() + timeout_duration);
    }

    template <typename Lock, typename Rep, typename Period, typename Predicate>
    bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& timeout_duration, Predicate pred) {
        return wait_until(lock, std::chrono::steady_clock::now() + timeout_duration, pred);
    }

    template <typename Lock, typename Predicate>
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    bool wait(Lock& lock, stop_token stop_token, Predicate pred) {
        if(stop_token.stop_requested()) {
            return pred();
        }

        // get direct access to the underlying nsync_note
        nsync_note stop_note = stop_token.native_handle();
        if(!stop_note) {
            // no actual stop token, fall back to polling
            // is this an error condition?
            while(!pred()) {
                wait(lock);
            }
            return pred();
        }

        std::shared_ptr<nsync_mutex_t> mutex = m_mutex;

        while(!pred()) {
            if(stop_token.stop_requested()) {
                return false;
            }

            // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
            int result;
            {
                unique_lock<nsync_mutex_t> internal_lock(*mutex);
                unlock_guard<Lock> unlock_user(lock);
                unique_lock<nsync_mutex_t> internal_lock2(std::move(internal_lock));

                result = nsync_cv_wait_with_deadline(&m_cv, internal_lock2.mutex()->native_handle(),
                                                     nsync_time_no_deadline, stop_note);
            }

            if(result == ECANCELED) {
                return pred();
            }
        }

        return true;
    }

    template <typename Lock, typename Clock, typename Duration, typename Predicate>
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    bool wait_until(Lock& lock, stop_token stop_token, const std::chrono::time_point<Clock, Duration>& timeout_time,
                    Predicate pred) {
        if(stop_token.stop_requested()) {
            return pred();
        }

        nsync_note stop_note = stop_token.native_handle();
        std::shared_ptr<nsync_mutex_t> mutex = m_mutex;

        while(!pred()) {
            if(stop_token.stop_requested()) {
                return false;
            }

            auto now = Clock::now();
            if(timeout_time <= now) {
                return pred();
            }

            auto timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout_time - now);
            nsync_time deadline = nsync_time_add(nsync_time_now(), nsync_time_s_ns(0, timeout_ns.count()));

            // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
            int result;
            {
                unique_lock<nsync_mutex_t> internal_lock(*mutex);
                unlock_guard<Lock> unlock_user(lock);
                unique_lock<nsync_mutex_t> internal_lock2(std::move(internal_lock));

                result =
                    nsync_cv_wait_with_deadline(&m_cv, internal_lock2.mutex()->native_handle(), deadline, stop_note);
            }

            if(result == ETIMEDOUT || result == ECANCELED) {
                return pred();
            }
        }

        return true;
    }

    template <typename Lock, typename Rep, typename Period, typename Predicate>
    bool wait_for(Lock& lock, stop_token stop_token, const std::chrono::duration<Rep, Period>& timeout_duration,
                  Predicate pred) {
        return wait_until(lock, stop_token, std::chrono::steady_clock::now() + timeout_duration, pred);
    }
};

// type aliases matching standard library
using condition_variable = nsync_condition_variable_t;
using condition_variable_any = nsync_condition_variable_any_t;

#else
// standard library fallback
using condition_variable = std::condition_variable;
using condition_variable_any = std::condition_variable_any;
#endif

}  // namespace Sync
