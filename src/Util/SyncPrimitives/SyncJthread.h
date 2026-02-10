// Copyright (c) 2025-2026, WH, All rights reserved.
#pragma once
// jthreads (+ stop token support)

#include "config.h"
#include "SyncStoptoken.h"

#ifdef MCENGINE_PLATFORM_WASM

#include <thread>

namespace Sync {
using jthread = std::jthread;
}

#else

#include <functional>

// fwd decls to avoid including SDL things here
#ifndef CDECLCALL
#if defined(MCENGINE_PLATFORM_WINDOWS) && !defined(__GNUC__)
#define CDECLCALL __cdecl
#else
#define CDECLCALL
#endif
#endif

// TODO: should probably turn Sync::jthread into McThread since we don't really
// do anything nsync-specific here
namespace McThread {
extern void on_thread_init() noexcept;
}

typedef struct SDL_Thread SDL_Thread;
typedef uint64_t SDL_ThreadID;

namespace Sync {

// ===================================================================
// sdl_jthread: thread with stop_token support + 8MB stack size
// ===================================================================
class sdl_jthread {
   private:
    SDL_Thread* m_thread{nullptr};
    stop_source m_stop_source;

    static SDL_Thread* create_thread_internal(void* ctx, void* entry) noexcept;

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

            static int CDECLCALL invoke(void* data) {
                // as of now, just sets up some thread-local state we want for every thread created
                // (like DAZ + CTZ for SSE / FTZ for ARM)
                McThread::on_thread_init();

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

        m_thread = create_thread_internal(ctx, (void*)&thread_context::invoke);

        if(!m_thread) {
            delete ctx;  // cleanup on failure
        }
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
            if(joinable()) {
                request_stop();
                join();
            }

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

    [[nodiscard]] bool joinable() const noexcept;

    void join();

    void detach();

    [[nodiscard]] id get_id() const noexcept;

    [[nodiscard]] native_handle_type native_handle() { return m_thread; }

    [[nodiscard]] static unsigned int hardware_concurrency() noexcept;

    // stop token functionality
    [[nodiscard]] stop_source& get_stop_source() noexcept { return m_stop_source; }

    [[nodiscard]] stop_token get_stop_token() noexcept { return m_stop_source.get_token(); }

    bool request_stop() noexcept { return m_stop_source.request_stop(); }
};

using jthread = sdl_jthread;

}  // namespace Sync

#endif
