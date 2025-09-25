// Copyright (c) 2025, WH, All rights reserved.
#pragma once
// jthreads (+ stop token support)

#include "config.h"

#include "SyncStoptoken.h"

#include <thread>

namespace Sync {
#ifdef USE_NSYNC
using namespace nsync;

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
#else
using jthread = std::jthread;
#endif

}  // namespace Sync
