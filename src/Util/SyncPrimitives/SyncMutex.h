// Copyright (c) 2025, WH, All rights reserved.
#pragma once
// mutex + recursive_mutex + RAII locking things

#include "config.h"

#ifdef USE_NSYNC
#include "nsync_mu.h"
#include "nsync_mu_wait.h"

#include <system_error>
#include <thread>
#include <mutex>  // for std::lock (works with generic-compatible mutexes)

#else

#include <mutex>

#endif  // USE_NSYNC

namespace Sync {
#ifdef USE_NSYNC
using namespace nsync;

// ===================================================================
// fundamental mutex wrapping nsync_mu
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

// exceptions are disabled
namespace detail {
constexpr void abort_message(const char* reason) {
    std::fprintf(stderr, "%s\n", reason);
    std::abort();
}
}  // namespace detail

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
// nsync_recursive_mutex_t: recursive mutex using nsync primitives
// WARNING: this is not expected/tested to work correctly with condition variables (yet)
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

// type alias
using recursive_mutex = nsync_recursive_mutex_t;

#else
// stdlib fallback aliases
using mutex = std::mutex;
using recursive_mutex = std::recursive_mutex;

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

#endif  // USE_NSYNC
}  // namespace Sync
