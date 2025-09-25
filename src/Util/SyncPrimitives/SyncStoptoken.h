// Copyright (c) 2025, WH, All rights reserved.
#pragma once
// stop_token

#include "config.h"
#include "SyncMutex.h"

#ifdef USE_NSYNC
#include "nsync_note.h"
#include <functional>
#include <vector>
#else
#include <stop_token>
#endif

namespace Sync {
#ifdef USE_NSYNC
using namespace nsync;

// ===================================================================
// stop_token: nsync-based cancellation token
// ===================================================================

// nostopstate constant
struct nostopstate_t {
    explicit nostopstate_t() = default;
};
inline constexpr nostopstate_t nostopstate{};

namespace detail {
struct stop_state {
    nsync_note m_note;
    nsync_mutex_t m_mutex;
    std::vector<std::function<void()>> m_callbacks;
    std::atomic<bool> m_stop_requested_flag{false};

    stop_state() : m_note(nsync_note_new(nullptr, nsync_time_no_deadline)) {
        assert(!!m_note && "stop_state::stop_state: nsync_note_new failed");
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

    explicit stop_source(nostopstate_t /* */) noexcept {}
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

#else
using stop_token = std::stop_token;
using stop_source = std::stop_source;
template <typename Callback>
using stop_callback = std::stop_callback<Callback>;
using nostopstate_t = std::nostopstate_t;
inline constexpr nostopstate_t nostopstate = std::nostopstate;
#endif

}  // namespace Sync
