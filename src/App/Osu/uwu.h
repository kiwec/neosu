#pragma once
#include <atomic>
#include <thread>
#include <vector>

#include "Thread.h"
#include "SyncMutex.h"
#include "SyncCV.h"

// Unified Wait Utilities (◕‿◕✿)
namespace uwu {

// Promise for queuing work, but only the latest function will be run.
template <typename Func, typename Ret>
struct lazy_promise {
    lazy_promise(Ret default_ret) : keep_running(true), thread_started(false), ret(default_ret) {
        // don't start thread during construction to avoid races
        // thread will be started lazily on first enqueue
    }

    ~lazy_promise() {
        // signal thread to stop
        {
            Sync::scoped_lock lock(this->funcs_mtx);
            this->keep_running = false;
        }
        this->cv.notify_one();

        // wait for thread to finish, but only if it was started
        if(this->thread_started && this->thread.joinable()) {
            this->thread.join();
        }
    }

    void enqueue(Func func) {
        {
            Sync::scoped_lock lock(this->funcs_mtx);

            // start thread lazily on first enqueue to avoid construction races
            if(!this->thread_started) {
                this->thread = std::thread(&lazy_promise::run, this);
                this->thread_started = true;
            }

            this->funcs.push_back(func);
        }
        this->cv.notify_one();
    }

    Ret get() {
        Sync::scoped_lock lock(this->ret_mtx);
        return this->ret;
    }

    void set(Ret ret) {
        Sync::scoped_lock lock(this->ret_mtx);
        this->ret = ret;
    }

   private:
    void run() {
        McThread::set_current_thread_name("lazy_func_thr");
        McThread::set_current_thread_prio(false);  // reset priority
        for(;;) {
            Sync::unique_lock<Sync::mutex> lock(this->funcs_mtx);
            this->cv.wait(lock, [this]() { return !this->funcs.empty() || !this->keep_running; });
            if(!this->keep_running) break;
            if(this->funcs.empty()) {
                continue;  // spurious wakeup
            }

            Func func = this->funcs[this->funcs.size() - 1];
            this->funcs.clear();
            lock.unlock();

            this->set(func());
        }
    }

    std::atomic<bool> keep_running;
    std::atomic<bool> thread_started;
    Sync::mutex funcs_mtx;
    Sync::condition_variable cv;
    std::vector<Func> funcs;

    Ret ret;
    Sync::mutex ret_mtx;

    // only initialized when needed
    std::thread thread;
};

};  // namespace uwu
