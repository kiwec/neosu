// Copyright (c) 2025, WH, All rights reserved.
#pragma once
// call_once + once_flag

#include "config.h"

#ifdef USE_NSYNC
#include "nsync_once.h"

#include <tuple>
#include <utility>
#else
#include <mutex>
#endif  // USE_NSYNC

namespace Sync {
#ifdef USE_NSYNC
using namespace nsync;

// ===================================================================
// once_flag: wrapper for nsync_once
// ===================================================================
class once_flag {
   private:
    nsync_once m_flag = NSYNC_ONCE_INIT;

   public:
    constexpr once_flag() noexcept = default;
    ~once_flag() = default;

    once_flag(const once_flag&) = delete;
    once_flag& operator=(const once_flag&) = delete;
    once_flag(once_flag&&) = delete;
    once_flag& operator=(once_flag&&) = delete;

    nsync_once* native_handle() noexcept { return &m_flag; }
};

// ===================================================================
// call_once: execute callable exactly once
// ===================================================================
template <typename Callable, typename... Args>
void call_once(once_flag& flag, Callable&& f, Args&&... args) {
    struct wrapper_t {
        Callable&& func;
        std::tuple<Args&&...> args_tuple;

        void invoke() {
            if constexpr(sizeof...(Args) == 0) {
                std::forward<Callable>(func)();
            } else {
                std::apply(std::forward<Callable>(func), std::move(args_tuple));
            }
        }

        static void static_invoke(void* arg) { static_cast<wrapper_t*>(arg)->invoke(); }
    };

    wrapper_t wrapper{std::forward<Callable>(f), std::forward_as_tuple(std::forward<Args>(args)...)};

    nsync_run_once_arg(flag.native_handle(), &wrapper_t::static_invoke, &wrapper);
}

#else
using std::call_once;
using std::once_flag;
#endif  // USE_NSYNC
}  // namespace Sync
