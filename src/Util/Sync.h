// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "config.h"

#if defined(USE_NSYNC)
#include "nsync.h"
#include <vector>
#include <system_error>
#include <tuple>
#include <chrono>
#include <functional>
#endif

#include <thread>
#include <condition_variable>
#include <memory>
#include <stop_token>
#include <mutex>

namespace Sync {
#ifdef USE_NSYNC
using namespace nsync;

// exceptions are disabled
namespace detail {
constexpr void abort_message(const char* reason) {
    std::fprintf(stderr, "%s\n", reason);
    std::abort();
}
}  // namespace detail

// ===================================================================
// basic nsync mutex wrapper
// ===================================================================
class nsync_mutex_t {
   private:
    nsync_mu m_mutex{};

   public:
    constexpr nsync_mutex_t() noexcept = default;
    ~nsync_mutex_t() = default;

    nsync_mutex_t(const nsync_mutex_t&) = delete;
    nsync_mutex_t& operator=(const nsync_mutex_t&) = delete;
    nsync_mutex_t(nsync_mutex_t&&) = delete;
    nsync_mutex_t& operator=(nsync_mutex_t&&) = delete;

    void lock() { nsync_mu_lock(&m_mutex); }
    bool try_lock() noexcept { return nsync_mu_trylock(&m_mutex) != 0; }
    void unlock() { nsync_mu_unlock(&m_mutex); }

    // native handle for condition variable use
    nsync_mu* native_handle() noexcept { return &m_mutex; }
};

// ===================================================================
// nsync_recursive_mutex_t: recursive mutex using nsync primitives
// ===================================================================
class nsync_recursive_mutex_t {
   private:
    nsync_mu m_mutex{};
    std::thread::id m_owner;
    int m_count = 0;

    struct condition_arg {
        const nsync_recursive_mutex_t* self{nullptr};
        std::thread::id requester;
    };

    static int is_available_or_owned_by_requester(const void* arg) {
        const auto* carg = static_cast<const condition_arg*>(arg);
        return (carg->self->m_owner == std::thread::id{} || carg->self->m_owner == carg->requester) ? 1 : 0;
    }

   public:
    constexpr nsync_recursive_mutex_t() noexcept = default;
    ~nsync_recursive_mutex_t() = default;

    nsync_recursive_mutex_t(const nsync_recursive_mutex_t&) = delete;
    nsync_recursive_mutex_t& operator=(const nsync_recursive_mutex_t&) = delete;
    nsync_recursive_mutex_t(nsync_recursive_mutex_t&&) = delete;
    nsync_recursive_mutex_t& operator=(nsync_recursive_mutex_t&&) = delete;

    void lock() {
        nsync_mu_lock(&m_mutex);

        auto current_id = std::this_thread::get_id();
        condition_arg arg{this, current_id};

        // wait until lock is either free or already owned by this thread
        nsync_mu_wait(&m_mutex, &is_available_or_owned_by_requester, &arg, nullptr);

        if(m_owner == std::thread::id{}) {
            // lock was free, acquire it
            m_owner = current_id;
            m_count = 1;
        } else {
            // we already own it, increment recursion count
            ++m_count;
        }

        nsync_mu_unlock(&m_mutex);
    }

    bool try_lock() noexcept {
        if(!nsync_mu_trylock(&m_mutex)) {
            return false;
        }

        auto current_id = std::this_thread::get_id();

        if(m_owner == std::thread::id{}) {
            // lock is free, acquire it
            m_owner = current_id;
            m_count = 1;
            nsync_mu_unlock(&m_mutex);
            return true;
        } else if(m_owner == current_id) {
            // we already own it, increment recursion count
            ++m_count;
            nsync_mu_unlock(&m_mutex);
            return true;
        } else {
            // someone else owns it
            nsync_mu_unlock(&m_mutex);
            return false;
        }
    }

    void unlock() {
        nsync_mu_lock(&m_mutex);

        auto current_id = std::this_thread::get_id();
        if(m_owner != current_id) {
            nsync_mu_unlock(&m_mutex);
            detail::abort_message("recursive_mutex::unlock: not owner");
            return;
        }

        --m_count;
        if(m_count == 0) {
            m_owner = std::thread::id{};
        }

        nsync_mu_unlock(&m_mutex);
    }

    // WARNING: this is not expected/tested to work correctly with condition variables
    // leaving it here for possible future expansion (maybe)
    nsync_mu* native_handle() noexcept { return &m_mutex; }
};

// type alias for convenience
using recursive_mutex = nsync_recursive_mutex_t;

// forward declarations for RAII guards
template <typename Mutex>
class lock_guard;
template <typename Mutex>
class unique_lock;
template <typename... Mutexes>
class scoped_lock;

// type alias for convenience
using mutex = nsync_mutex_t;

// ===================================================================
// defer lock tags (matching std library)
// ===================================================================
struct defer_lock_t {
    explicit defer_lock_t() = default;
};
struct try_to_lock_t {
    explicit try_to_lock_t() = default;
};
struct adopt_lock_t {
    explicit adopt_lock_t() = default;
};

inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t adopt_lock{};

// ===================================================================
// lock_guard: simple RAII mutex wrapper
// ===================================================================
template <typename Mutex>
class lock_guard {
   public:
    using mutex_type = Mutex;

    explicit lock_guard(mutex_type& m) : m_p(m) { m_p.lock(); }

    lock_guard(mutex_type& m, adopt_lock_t /* */) noexcept : m_p(m) {}

    ~lock_guard() { m_p.unlock(); }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
    lock_guard(lock_guard&&) = delete;
    lock_guard& operator=(lock_guard&&) = delete;

   private:
    mutex_type& m_p;
};

// ===================================================================
// unique_lock: flexible RAII mutex wrapper
// ===================================================================
template <typename Mutex>
class unique_lock {
   public:
    using mutex_type = Mutex;

    unique_lock() noexcept : m_p(nullptr), m_owns(false) {}

    explicit unique_lock(mutex_type& m) : m_p(std::addressof(m)), m_owns(false) {
        lock();
        m_owns = true;
    }

    unique_lock(mutex_type& m, defer_lock_t /* */) noexcept : m_p(std::addressof(m)), m_owns(false) {}
    unique_lock(mutex_type& m, try_to_lock_t /* */) : m_p(std::addressof(m)), m_owns(m_p->try_lock()) {}
    unique_lock(mutex_type& m, adopt_lock_t /* */) noexcept : m_p(std::addressof(m)), m_owns(true) {}

    ~unique_lock() {
        if(m_owns) unlock();
    }

    unique_lock(unique_lock&& other) noexcept : m_p(other.m_p), m_owns(other.m_owns) {
        other.m_p = nullptr;
        other.m_owns = false;
    }

    unique_lock& operator=(unique_lock&& other) noexcept {
        if(this != std::addressof(other)) {
            if(m_owns) unlock();
            m_p = other.m_p;
            m_owns = other.m_owns;
            other.m_p = nullptr;
            other.m_owns = false;
        }
        return *this;
    }

    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    void lock() {
        if(!m_p) detail::abort_message(std::make_error_code(std::errc::operation_not_permitted).message().c_str());
        if(m_owns)
            detail::abort_message(std::make_error_code(std::errc::resource_deadlock_would_occur).message().c_str());
        m_p->lock();
        m_owns = true;
    }

    bool try_lock() {
        if(!m_p) detail::abort_message(std::make_error_code(std::errc::operation_not_permitted).message().c_str());
        if(m_owns)
            detail::abort_message(std::make_error_code(std::errc::resource_deadlock_would_occur).message().c_str());
        m_owns = m_p->try_lock();
        return m_owns;
    }

    void unlock() {
        if(!m_owns) detail::abort_message(std::make_error_code(std::errc::operation_not_permitted).message().c_str());
        if(m_p) {
            m_p->unlock();
            m_owns = false;
        }
    }

    void swap(unique_lock& other) noexcept {
        std::swap(m_p, other.m_p);
        std::swap(m_owns, other.m_owns);
    }

    mutex_type* release() noexcept {
        mutex_type* ret = m_p;
        m_p = nullptr;
        m_owns = false;
        return ret;
    }

    [[nodiscard]] bool owns_lock() const noexcept { return m_owns; }
    explicit operator bool() const noexcept { return m_owns; }
    [[nodiscard]] mutex_type* mutex() const noexcept { return m_p; }

   private:
    mutex_type* m_p;
    bool m_owns;
};

template <typename Mutex>
void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) noexcept {
    lhs.swap(rhs);
}

// ===================================================================
// scoped_lock: multiple mutex RAII wrapper
// ===================================================================
template <typename... Mutexes>
class scoped_lock {
   public:
    explicit scoped_lock(Mutexes&... mutexes) : m_mutexes(mutexes...) { std::lock(mutexes...); }

    explicit scoped_lock(adopt_lock_t /* */, Mutexes&... mutexes) noexcept : m_mutexes(mutexes...) {}

    ~scoped_lock() { unlock_all(std::index_sequence_for<Mutexes...>{}); }

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
    scoped_lock(scoped_lock&&) = delete;
    scoped_lock& operator=(scoped_lock&&) = delete;

   private:
    std::tuple<Mutexes&...> m_mutexes;

    template <std::size_t... Indices>
    void unlock_all(std::index_sequence<Indices...> /* */) {
        (std::get<sizeof...(Indices) - 1 - Indices>(m_mutexes).unlock(), ...);
    }
};

// single mutex specialization
template <typename Mutex>
class scoped_lock<Mutex> {
   public:
    using mutex_type = Mutex;

    explicit scoped_lock(mutex_type& m) : m_p(m) { m_p.lock(); }
    explicit scoped_lock(adopt_lock_t /* */, mutex_type& m) noexcept : m_p(m) {}
    ~scoped_lock() { m_p.unlock(); }

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
    scoped_lock(scoped_lock&&) = delete;
    scoped_lock& operator=(scoped_lock&&) = delete;

   private:
    mutex_type& m_p;
};

// empty specialization
template <>
class scoped_lock<> {
   public:
    explicit scoped_lock() = default;
    explicit scoped_lock(adopt_lock_t /* */) noexcept {}
    ~scoped_lock() = default;

    scoped_lock(const scoped_lock&) = delete;
    scoped_lock& operator=(const scoped_lock&) = delete;
    scoped_lock(scoped_lock&&) = delete;
    scoped_lock& operator=(scoped_lock&&) = delete;
};

// ===================================================================
// stop_token: nsync-based cancellation token
// ===================================================================

namespace detail {
struct stop_state {
    nsync_note m_note;
    nsync_mutex_t m_mutex;
    std::vector<std::function<void()>> m_callbacks;
    std::atomic<bool> m_stop_requested_flag{false};

    stop_state() : m_note(nsync_note_new(nullptr, nsync_time_no_deadline)) {
        if(!m_note) {
            detail::abort_message("Failed to create nsync_note for stop_token");
        }
    }

    ~stop_state() {
        if(m_note) {
            nsync_note_free(m_note);
        }
    }

    stop_state(const stop_state&) = delete;
    stop_state(stop_state&&) = delete;
    stop_state& operator=(const stop_state&) = delete;
    stop_state& operator=(stop_state&&) = delete;
};
}  // namespace detail

template <typename Callback>
class stop_callback;

class stop_token {
   private:
    std::shared_ptr<detail::stop_state> m_state;

    friend class stop_source;
    template <typename Callback>
    friend class stop_callback;
    explicit stop_token(std::shared_ptr<detail::stop_state> state) noexcept : m_state(std::move(state)) {}

   public:
    stop_token() noexcept = default;

    stop_token(const stop_token&) noexcept = default;
    stop_token(stop_token&&) noexcept = default;

    ~stop_token() = default;

    stop_token& operator=(const stop_token&) noexcept = default;

    stop_token& operator=(stop_token&&) noexcept = default;

    [[nodiscard]] bool stop_requested() const noexcept {
        return m_state && m_state->m_stop_requested_flag.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool stop_possible() const noexcept { return m_state != nullptr; }

    // direct access to underlying nsync_note for low overhead condition variable usage
    [[nodiscard]] nsync_note native_handle() const noexcept { return m_state ? m_state->m_note : nullptr; }

    friend bool operator==(const stop_token& lhs, const stop_token& rhs) noexcept { return lhs.m_state == rhs.m_state; }

    friend bool operator!=(const stop_token& lhs, const stop_token& rhs) noexcept { return !(lhs == rhs); }

    void swap(stop_token& other) noexcept { m_state.swap(other.m_state); }
};

class stop_source {
   private:
    std::shared_ptr<detail::stop_state> m_state;

   public:
    stop_source() : m_state(std::make_shared<detail::stop_state>()) {}

    explicit stop_source(std::nostopstate_t /* */) noexcept {}
    ~stop_source() = default;

    stop_source(const stop_source&) = default;
    stop_source(stop_source&&) = default;
    stop_source& operator=(const stop_source&) = default;
    stop_source& operator=(stop_source&&) = default;

    [[nodiscard]] stop_token get_token() noexcept { return m_state ? stop_token{m_state} : stop_token{}; }

    [[nodiscard]] bool stop_possible() const noexcept { return m_state != nullptr; }

    [[nodiscard]] bool stop_requested() const noexcept {
        return m_state && m_state->m_stop_requested_flag.load(std::memory_order_acquire);
    }

    bool request_stop() noexcept {
        if(!m_state) return false;

        // check if already stopped
        if(m_state->m_stop_requested_flag.exchange(true, std::memory_order_acq_rel)) {
            return false;
        }

        // notify the nsync_note
        nsync_note_notify(m_state->m_note);

        // execute callbacks under lock
        lock_guard<nsync_mutex_t> lock(m_state->m_mutex);
        for(const auto& callback : m_state->m_callbacks) {
            callback();
        }

        return true;
    }

    void swap(stop_source& other) noexcept { m_state.swap(other.m_state); }

    friend bool operator==(const stop_source& lhs, const stop_source& rhs) noexcept {
        return lhs.m_state == rhs.m_state;
    }

    friend bool operator!=(const stop_source& lhs, const stop_source& rhs) noexcept { return !(lhs == rhs); }
};

template <typename Callback>
class stop_callback {
   private:
    std::shared_ptr<detail::stop_state> m_state;
    typename std::vector<std::function<void()>>::iterator m_callback_iter;
    bool m_callback_registered = false;

   public:
    using callback_type = Callback;

    template <typename C>
    explicit stop_callback(const stop_token& token, C&& callback) : m_state(token.m_state) {
        if(m_state) {
            lock_guard<nsync_mutex_t> lock(m_state->m_mutex);

            // if already stopped, execute now
            if(m_state->m_stop_requested_flag.load(std::memory_order_acquire)) {
                std::forward<C>(callback)();
                return;
            }

            // register callback
            m_state->m_callbacks.emplace_back(std::forward<C>(callback));
            m_callback_iter = std::prev(m_state->m_callbacks.end());
            m_callback_registered = true;
        }
    }

    template <typename C>
    explicit stop_callback(stop_token&& token, C&& callback)
        : stop_callback(std::move(token), std::forward<C>(callback)) {}

    ~stop_callback() {
        if(m_callback_registered && m_state) {
            lock_guard<nsync_mutex_t> lock(m_state->m_mutex);
            if(m_callback_iter != m_state->m_callbacks.end()) {
                m_state->m_callbacks.erase(m_callback_iter);
            }
        }
    }

    stop_callback(const stop_callback&) = delete;
    stop_callback(stop_callback&&) = delete;
    stop_callback& operator=(const stop_callback&) = delete;
    stop_callback& operator=(stop_callback&&) = delete;
};

template <typename Callback>
stop_callback(stop_token, Callback) -> stop_callback<Callback>;

template <typename Callback>
stop_callback(stop_token&&, Callback) -> stop_callback<Callback>;

// nostopstate constant
struct nostopstate_t {
    explicit nostopstate_t() = default;
};
inline constexpr nostopstate_t nostopstate{};

// ===================================================================
// jthread: thread with nsync-backed stop_token support
// ===================================================================
class jthread {
   private:
    std::thread m_thread;
    stop_source m_stop_source;

   public:
    using id = std::thread::id;
    using native_handle_type = std::thread::native_handle_type;

    jthread() noexcept = default;

    template <typename F, typename... Args>
    explicit jthread(F&& f, Args&&... args) {
        if constexpr(std::is_invocable_v<std::decay_t<F>, stop_token, std::decay_t<Args>...>) {
            // function takes stop_token as first parameter
            m_thread = std::thread(std::forward<F>(f), m_stop_source.get_token(), std::forward<Args>(args)...);
        } else {
            // function doesn't take stop_token
            m_thread = std::thread(std::forward<F>(f), std::forward<Args>(args)...);
        }
    }

    ~jthread() {
        if(joinable()) {
            request_stop();
            join();
        }
    }

    jthread(const jthread&) = delete;
    jthread(jthread&&) noexcept = default;
    jthread& operator=(const jthread&) = delete;
    jthread& operator=(jthread&& other) noexcept {
        if(this != &other) {
            if(joinable()) {
                request_stop();
                join();
            }
            m_thread = std::move(other.m_thread);
            m_stop_source = std::move(other.m_stop_source);
        }
        return *this;
    }

    void swap(jthread& other) noexcept {
        m_thread.swap(other.m_thread);
        m_stop_source.swap(other.m_stop_source);
    }

    [[nodiscard]] bool joinable() const noexcept { return m_thread.joinable(); }

    void join() { m_thread.join(); }

    void detach() { m_thread.detach(); }

    [[nodiscard]] id get_id() const noexcept { return m_thread.get_id(); }

    [[nodiscard]] native_handle_type native_handle() { return m_thread.native_handle(); }

    [[nodiscard]] static unsigned int hardware_concurrency() noexcept { return std::thread::hardware_concurrency(); }

    // stop token functionality
    [[nodiscard]] stop_source& get_stop_source() noexcept { return m_stop_source; }

    [[nodiscard]] stop_token get_stop_token() noexcept { return m_stop_source.get_token(); }

    bool request_stop() noexcept { return m_stop_source.request_stop(); }
};

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
using mutex = std::mutex;
using recursive_mutex = std::recursive_mutex;
using condition_variable = std::condition_variable;
using condition_variable_any = std::condition_variable_any;

template <typename Mutex>
using lock_guard = std::lock_guard<Mutex>;
template <typename Mutex>
using unique_lock = std::unique_lock<Mutex>;
template <typename... Mutexes>
using scoped_lock = std::scoped_lock<Mutexes...>;

using defer_lock_t = std::defer_lock_t;
using try_to_lock_t = std::try_to_lock_t;
using adopt_lock_t = std::adopt_lock_t;

inline constexpr defer_lock_t defer_lock = std::defer_lock;
inline constexpr try_to_lock_t try_to_lock = std::try_to_lock;
inline constexpr adopt_lock_t adopt_lock = std::adopt_lock;

using stop_token = std::stop_token;
using stop_source = std::stop_source;
template <typename Callback>
using stop_callback = std::stop_callback<Callback>;
using nostopstate_t = std::nostopstate_t;
inline constexpr nostopstate_t nostopstate = std::nostopstate;

using jthread = std::jthread;
#endif

}  // namespace Sync
