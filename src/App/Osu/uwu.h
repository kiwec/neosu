#pragma once
#include <vector>

#include "UString.h"
#include "Thread.h"
#include "SyncMutex.h"
#include "SyncCV.h"
#include "SyncJthread.h"

// Unified Wait Utilities (◕‿◕✿)
namespace uwu {

template <typename Func, typename... Args>
concept WorkItem = std::is_invocable_v<Func, Args...>;

// Promise for queuing work, but only the latest function will be run.
template <WorkItem Func>
struct lazy_promise {
    using Ret = std::invoke_result_t<Func>;
    lazy_promise()
        requires(std::is_default_constructible_v<Ret>)
    = default;
    lazy_promise()
        requires(!std::is_default_constructible_v<Ret>)
    = delete;

    lazy_promise(Ret default_ret) : ret(default_ret) {}

    void enqueue(Func func) {
        {
            Sync::scoped_lock lock(this->funcs_mtx);
            // start thread now
            if(!this->thread.joinable()) {
                this->thread = Sync::jthread([this](const Sync::stop_token &stoken) { return this->run(stoken); });
            }

            this->funcs.push_back(func);
        }
        this->cv.notify_one();
    }

    std::pair<Sync::unique_lock<Sync::mutex>, const Ret &> get() {
        Sync::unique_lock<Sync::mutex> lock(this->ret_mtx);
        return {std::move(lock), this->ret};
    }

    void set(Ret &&ret) {
        Sync::scoped_lock lock(this->ret_mtx);
        this->ret = std::move(ret);
    }

   private:
    void run(const Sync::stop_token &stoken) {
        McThread::set_current_thread_name(ULITERAL("lazy_func_thr"));
        McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority
        while(!stoken.stop_requested()) {
            Sync::unique_lock<Sync::mutex> lock(this->funcs_mtx);
            this->cv.wait(lock, stoken, [this]() { return !this->funcs.empty(); });
            if(stoken.stop_requested()) break;
            if(this->funcs.empty()) {
                continue;  // spurious wakeup
            }

            Func func = std::move(this->funcs.back());
            this->funcs.clear();
            lock.unlock();

            this->set(func());
        }
    }

    Sync::mutex funcs_mtx;
    Sync::condition_variable_any cv;
    std::vector<Func> funcs;

    Ret ret;
    Sync::mutex ret_mtx;

    // lazily created on first enqueue
    Sync::jthread thread;
};

};  // namespace uwu
