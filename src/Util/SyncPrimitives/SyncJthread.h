// Copyright (c) 2025, WH, All rights reserved.
#pragma once
// jthreads (+ stop token support)

#include "config.h"

#ifdef MCENGINE_PLATFORM_WASM

#include <thread>

namespace Sync {
using jthread = std::jthread;
}

#else
#include "SyncStoptoken.h"

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_cpuinfo.h>

#include <functional>
#include <cassert>

namespace Sync {

// ===================================================================
// sdl_jthread: thread with stop_token support + 8MB stack size
// ===================================================================
class sdl_jthread {
   private:
    SDL_Thread* m_thread{nullptr};
    stop_source m_stop_source;

   public:
    using id = SDL_ThreadID;
    using native_handle_type = SDL_Thread*;

    sdl_jthread() noexcept = default;

    template <typename F, typename... Args>
    explicit sdl_jthread(F&& f, Args&&... args) {
        // wrapper context
        struct thread_context {
            std::decay_t<F> func;
            std::tuple<std::decay_t<Args>...> args;
            stop_token token;

            static int SDLCALL invoke(void* data) {
                auto* ctx = static_cast<thread_context*>(data);

                // invoke with or without stop_token depending on signature
                if constexpr(std::is_invocable_v<std::decay_t<F>, stop_token, std::decay_t<Args>...>) {
                    std::apply(
                        [&](auto&&... a) { std::invoke(ctx->func, ctx->token, std::forward<decltype(a)>(a)...); },
                        std::move(ctx->args));
                } else {
                    std::apply([&](auto&&... a) { std::invoke(ctx->func, std::forward<decltype(a)>(a)...); },
                               std::move(ctx->args));
                }

                delete ctx;
                return 0;
            }
        };

        // allocate the context/arg wrapper
        auto* ctx = new thread_context{std::forward<F>(f), std::make_tuple(std::forward<Args>(args)...),
                                       m_stop_source.get_token()};

        auto props = SDL_CreateProperties();
        assert(props);

        SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void*)&thread_context::invoke);
        SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, ctx);
        SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, 8LL * 1024 * 1024); /* 8MB */

        m_thread = SDL_CreateThreadWithProperties(props);

        if(!m_thread) {
            delete ctx;  // cleanup on failure
        }

        SDL_DestroyProperties(props);
    }

    ~sdl_jthread() {
        if(joinable()) {
            request_stop();
            join();
        }
    }

    sdl_jthread(const sdl_jthread&) = delete;
    sdl_jthread(sdl_jthread&& other) noexcept
        : m_thread(other.m_thread), m_stop_source(std::move(other.m_stop_source)) {
        other.m_thread = nullptr;
    }
    sdl_jthread& operator=(const sdl_jthread&) = delete;
    sdl_jthread& operator=(sdl_jthread&& other) noexcept {
        if(this != &other) {
            this->~sdl_jthread();

            m_thread = other.m_thread;
            other.m_thread = nullptr;
            m_stop_source = std::move(other.m_stop_source);
        }
        return *this;
    }

    void swap(sdl_jthread& other) noexcept {
        auto last = m_thread;
        m_thread = other.m_thread;
        other.m_thread = last;
        m_stop_source.swap(other.m_stop_source);
    }

    [[nodiscard]] bool joinable() const noexcept {
        SDL_ThreadState state = SDL_THREAD_UNKNOWN;
        return m_thread && ((state = SDL_GetThreadState(m_thread)) == SDL_THREAD_ALIVE || state == SDL_THREAD_COMPLETE);
    }

    void join() {
        assert(joinable());
        SDL_WaitThread(m_thread, nullptr);
        m_thread = nullptr;
    }

    void detach() { SDL_DetachThread(m_thread); }

    [[nodiscard]] id get_id() const noexcept { return SDL_GetThreadID(m_thread); }

    [[nodiscard]] native_handle_type native_handle() { return m_thread; }

    [[nodiscard]] static unsigned int hardware_concurrency() noexcept { return SDL_GetNumLogicalCPUCores(); }

    // stop token functionality
    [[nodiscard]] stop_source& get_stop_source() noexcept { return m_stop_source; }

    [[nodiscard]] stop_token get_stop_token() noexcept { return m_stop_source.get_token(); }

    bool request_stop() noexcept { return m_stop_source.request_stop(); }
};

using jthread = sdl_jthread;

}  // namespace Sync

#endif
