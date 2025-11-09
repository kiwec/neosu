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
#include "noinclude.h"
#include "SyncStoptoken.h"

#include <functional>
#include <cassert>

extern "C" {
// SDL_stdinc.h
#ifndef SDL_stdinc_h_
typedef void (*SDL_FunctionPointer)(void);
#endif

// SDL_properties.h
#ifndef SDL_properties_h_
typedef uint32_t SDL_PropertiesID;
extern SDL_DECLSPEC SDL_PropertiesID SDLCALL SDL_CreateProperties(void);
extern SDL_DECLSPEC bool SDLCALL SDL_SetPointerProperty(SDL_PropertiesID props, const char* name, void* value);
extern SDL_DECLSPEC bool SDLCALL SDL_SetNumberProperty(SDL_PropertiesID props, const char* name, int64_t value);
extern SDL_DECLSPEC void SDLCALL SDL_DestroyProperties(SDL_PropertiesID props);
#endif // SDL_properties.h

// SDL_thread.h
#ifndef SDL_thread_h_
#include <cstdint>
#if defined(MCENGINE_PLATFORM_WINDOWS)
#include <process.h> /* _beginthreadex() and _endthreadex() */
#endif

#ifndef SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER
#define SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER "SDL.thread.create.entry_function"
#endif
#ifndef SDL_PROP_THREAD_CREATE_NAME_STRING
#define SDL_PROP_THREAD_CREATE_NAME_STRING "SDL.thread.create.name"
#endif
#ifndef SDL_PROP_THREAD_CREATE_USERDATA_POINTER
#define SDL_PROP_THREAD_CREATE_USERDATA_POINTER "SDL.thread.create.userdata"
#endif
#ifndef SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER
#define SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER "SDL.thread.create.stacksize"
#endif

#if defined(MCENGINE_PLATFORM_WINDOWS)
#ifndef SDL_BeginThreadFunction
#define SDL_BeginThreadFunction _beginthreadex
#endif
#ifndef SDL_EndThreadFunction
#define SDL_EndThreadFunction _endthreadex
#endif
#else
#ifndef SDL_BeginThreadFunction
#define SDL_BeginThreadFunction NULL
#endif
#ifndef SDL_EndThreadFunction
#define SDL_EndThreadFunction NULL
#endif
#endif

#ifndef SDL_CreateThreadWithProperties
#define SDL_CreateThreadWithProperties(props)                                                      \
    SDL_CreateThreadWithPropertiesRuntime((props), (SDL_FunctionPointer)(SDL_BeginThreadFunction), \
                                          (SDL_FunctionPointer)(SDL_EndThreadFunction))
#endif
typedef struct SDL_Thread SDL_Thread;
typedef uint64_t SDL_ThreadID;
typedef enum SDL_ThreadState {
    SDL_THREAD_UNKNOWN,
    SDL_THREAD_ALIVE,
    SDL_THREAD_DETACHED,
    SDL_THREAD_COMPLETE
} SDL_ThreadState;

extern SDL_DECLSPEC SDL_Thread* SDLCALL SDL_CreateThreadWithPropertiesRuntime(SDL_PropertiesID props,
                                                                              SDL_FunctionPointer pfnBeginThread,
                                                                              SDL_FunctionPointer pfnEndThread);
extern SDL_DECLSPEC SDL_ThreadState SDLCALL SDL_GetThreadState(SDL_Thread* thread);
extern SDL_DECLSPEC void SDLCALL SDL_WaitThread(SDL_Thread* thread, int* status);
extern SDL_DECLSPEC void SDLCALL SDL_DetachThread(SDL_Thread* thread);
extern SDL_DECLSPEC SDL_ThreadID SDLCALL SDL_GetCurrentThreadID(void);
#endif // SDL_thread.h

// SDL_cpuinfo.h
#ifndef SDL_cpuinfo_h_
extern SDL_DECLSPEC int SDLCALL SDL_GetNumLogicalCPUCores(void);
#endif // SDL_cpuinfo.h
}

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

    [[nodiscard]] id get_id() const noexcept { return SDL_GetCurrentThreadID(); }

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
