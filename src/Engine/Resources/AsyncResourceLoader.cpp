// Copyright (c) 2025, WH, All rights reserved.
#include "AsyncResourceLoader.h"

#include "ConVar.h"
#include "Engine.h"
#include "Thread.h"
#include "Timing.h"
#include "Logging.h"
#include "SyncJthread.h"

#include <algorithm>
#include <utility>

using namespace std::chrono_literals;
namespace chrono = std::chrono;

//==================================
// LOADER THREAD
//==================================
class AsyncResourceLoader::LoaderThread final {
   public:
    size_t thread_index;
    std::atomic<chrono::steady_clock::time_point> last_active;

    LoaderThread(AsyncResourceLoader *const loader, size_t index) noexcept
        : thread_index(index),
          last_active(chrono::steady_clock::now()),
          loader_ptr(loader),
          thread([this](const Sync::stop_token &stoken) { this->worker_loop(stoken); }) {}

    [[nodiscard]] bool isReady() const noexcept { return this->thread.joinable(); }

    [[nodiscard]] bool isIdleTooLong() const noexcept {
        auto lastActive = this->last_active.load(std::memory_order_acquire);
        auto now = chrono::steady_clock::now();
        return chrono::duration_cast<chrono::milliseconds>(now - lastActive) > IDLE_TIMEOUT;
    }

   private:
    AsyncResourceLoader *const loader_ptr;
    Sync::jthread thread;

    void worker_loop(const Sync::stop_token &stoken) noexcept {
        this->loader_ptr->iActiveThreadCount.fetch_add(1, std::memory_order_relaxed);

        logIfCV(debug_rm, "Thread #{} started", this->thread_index);

        const UString loaderThreadName{
            fmt::format("res_ldr_thr{}", (this->thread_index % this->loader_ptr->iMaxThreads) + 1)};
        McThread::set_current_thread_name(loaderThreadName);
        McThread::set_current_thread_prio(
            McThread::Priority::NORMAL);  // reset priority (don't inherit from main thread)

        while(!stoken.stop_requested() && !this->loader_ptr->bShuttingDown.load(std::memory_order_acquire)) {
            const bool debug = cv::debug_rm.getBool();

            auto work = this->loader_ptr->getNextPendingWork();
            if(!work) {
                // yield in case we're sharing a logical CPU, like on a single-core system
                Timing::sleepMS(1);

                Sync::unique_lock lock(this->loader_ptr->workAvailableMutex);

                // wait indefinitely until work is available or stop is requested
                this->loader_ptr->workAvailable.wait(lock, stoken, [this]() {
                    return this->loader_ptr->bShuttingDown.load(std::memory_order_acquire) ||
                           this->loader_ptr->iActiveWorkCount.load(std::memory_order_acquire) > 0;
                });

                continue;
            }

            // notify that this thread completed work
            this->last_active.store(chrono::steady_clock::now(), std::memory_order_release);

            Resource *resource = work->resource;
            const bool interrupted = resource->isInterrupted();

            std::string debugName;
            if(debug) {
                debugName = resource->getDebugIdentifier();
                if(interrupted) {
                    debugLog("Thread #{} skipping (interrupted) workID {} {:s}", this->thread_index, work->workId,
                             debugName);
                } else {
                    debugLog("Thread #{} loading workID {} {:s}", this->thread_index, work->workId, debugName);
                }
            }

            if(interrupted) {
                work->state.store(WorkState::ASYNC_INTERRUPTED, std::memory_order_release);
            } else {
                work->state.store(WorkState::ASYNC_IN_PROGRESS, std::memory_order_release);

                resource->loadAsync();

                logIf(debug, "Thread #{} finished async loading {:s}", this->thread_index, debugName);

                work->state.store(WorkState::ASYNC_COMPLETE, std::memory_order_release);
            }

            this->loader_ptr->markWorkAsyncComplete(std::move(work));

            // yield again before loop
            Timing::sleepMS(0);
        }

        this->loader_ptr->iActiveThreadCount.fetch_sub(1, std::memory_order_acq_rel);

        logIfCV(debug_rm, "Thread #{} exiting", this->thread_index);
    }
};

//==================================
// ASYNC RESOURCE LOADER
//==================================

AsyncResourceLoader::AsyncResourceLoader()
    : iMaxThreads(std::clamp<size_t>(McThread::get_logical_cpu_count() - 1, 1, HARD_THREADCOUNT_LIMIT)),
      iLoadsPerUpdate(static_cast<size_t>(std::ceil(static_cast<double>(this->iMaxThreads) * (1. / 4.)))),
      lastCleanupTime(chrono::steady_clock::now()) {
    // pre-create at least a single thread for better startup responsiveness
    Sync::scoped_lock lock(this->threadsMutex);

    const size_t idx = this->iTotalThreadsCreated.fetch_add(1, std::memory_order_relaxed);
    auto loaderThread = std::make_unique<LoaderThread>(this, idx);

    if(!loaderThread->isReady()) {
        engine->showMessageError("AsyncResourceLoader Error", "Couldn't create core thread!");
    } else {
        logIfCV(debug_rm, "Created initial thread");
        this->threadpool[idx] = std::move(loaderThread);
    }
}

AsyncResourceLoader::~AsyncResourceLoader() { shutdown(); }

void AsyncResourceLoader::shutdown() {
    if(this->bShuttingDown) return;

    this->bShuttingDown = true;

    // wake up all waiting threads before requesting stop
    this->workAvailable.notify_all();

    // request all threads to stop and wake them up
    {
        Sync::scoped_lock lock(this->threadsMutex);
        // clear threadpool vector, jthread destructors will handle join + stop request automatically
        this->threadpool.clear();
    }

    // cleanup remaining work items
    {
        Sync::scoped_lock lock(this->workQueueMutex);
        while(!this->pendingWork.empty()) {
            this->pendingWork.pop();
        }
        while(!this->asyncCompleteWork.empty()) {
            this->asyncCompleteWork.pop();
        }
    }

    // cleanup loading resources tracking
    {
        Sync::scoped_lock lock(this->loadingResourcesMutex);
        this->loadingResourcesSet.clear();
    }

    // cleanup async destroy queue
    for(auto &[rs, del] : this->asyncDestroyQueue) {
        if(del) {
            SAFE_DELETE(rs);
        }
    }
    this->asyncDestroyQueue.clear();
}

void AsyncResourceLoader::requestAsyncLoad(Resource *resource) {
    auto work = std::make_unique<LoadingWork>(resource, this->iWorkIdCounter.fetch_add(1, std::memory_order_relaxed));

    // add to tracking set
    {
        Sync::scoped_lock lock(this->loadingResourcesMutex);
        this->loadingResourcesSet.insert(resource);
    }

    // add to work queue
    {
        Sync::scoped_lock lock(this->workQueueMutex);
        this->pendingWork.push(std::move(work));
    }

    this->iActiveWorkCount.fetch_add(1, std::memory_order_relaxed);
    ensureThreadAvailable();
    this->workAvailable.notify_one();
}

void AsyncResourceLoader::update(bool lowLatency) {
    if(!lowLatency) cleanupIdleThreads();
    const bool debug = cv::debug_rm.getBool();

    const size_t amountToProcess = lowLatency ? 1 : this->iLoadsPerUpdate;

    // process completed async work
    size_t numProcessed = 0;

    while(numProcessed < amountToProcess) {
        auto work = getNextAsyncCompleteWork();
        if(!work) {
            // decay back to default
            this->iLoadsPerUpdate =
                static_cast<size_t>(std::max(std::floor(static_cast<double>(this->iLoadsPerUpdate) * (15. / 16.)),
                                             std::ceil(static_cast<double>(this->iMaxThreads) * (1. / 4.))));
            break;
        }

        Resource *rs = work->resource;
        const bool interrupted =
            work->state.load(std::memory_order_acquire) == WorkState::ASYNC_INTERRUPTED || rs->isInterrupted();
        if(!interrupted) {
            logIf(debug, "Sync init for {:s}", rs->getDebugIdentifier());
            rs->load();
        } else {
            logIf(debug, "Skipping sync init for {:s}", rs->getDebugIdentifier());
        }

        work->state.store(WorkState::SYNC_COMPLETE, std::memory_order_release);

        // remove from tracking set
        {
            Sync::scoped_lock lock(this->loadingResourcesMutex);
            this->loadingResourcesSet.erase(rs);
        }

        this->iActiveWorkCount.fetch_sub(1, std::memory_order_acq_rel);
        if(!interrupted) numProcessed++;

        // work will be automatically destroyed when unique_ptr goes out of scope
    }

    // process async destroy queue
    std::vector<ToDestroy> resourcesReadyForDestroy;

    {
        Sync::scoped_lock lock(this->asyncDestroyMutex);
        for(size_t i = 0; i < this->asyncDestroyQueue.size(); i++) {
            bool canBeDestroyed = true;
            auto &current = this->asyncDestroyQueue[i];

            {
                Sync::scoped_lock loadingLock(this->loadingResourcesMutex);
                if(this->loadingResourcesSet.contains(current.rs)) {
                    canBeDestroyed = false;
                }
            }

            if(canBeDestroyed) {
                if(current.shouldDelete) {
                    resourcesReadyForDestroy.push_back(current);
                }  // don't delete it otherwise, just remove it from the destroy queue (our job of blocking on it to finish is done)
                this->asyncDestroyQueue.erase(this->asyncDestroyQueue.begin() + i);
                i--;
            }
        }
    }

    for(auto &[rs, deletable] : resourcesReadyForDestroy) {
        logIf(debug, "Async destroy of resource {:s}", rs->getDebugIdentifier());
        assert(deletable);
        SAFE_DELETE(rs);
    }
}

void AsyncResourceLoader::scheduleAsyncDestroy(Resource *resource, bool shouldDelete) {
    logIfCV(debug_rm, "Scheduled async destroy of {:s}", resource->getDebugIdentifier());

    Sync::scoped_lock lock(this->asyncDestroyMutex);
    this->asyncDestroyQueue.emplace_back(ToDestroy{.rs = resource, .shouldDelete = shouldDelete});
}

void AsyncResourceLoader::reloadResources(const std::vector<Resource *> &resources) {
    const bool debug = cv::debug_rm.getBool();
    if(resources.empty()) {
        logIf(debug, "W: reloadResources with empty resources vector!");
        return;
    }

    logIf(debug, "Async reloading {} resources", resources.size());

    std::vector<Resource *> resourcesToReload;
    for(Resource *rs : resources) {
        if(rs == nullptr) continue;

        logIf(debug, "Async reloading {:s}", rs->getDebugIdentifier());

        bool isBeingLoaded = isLoadingResource(rs);

        if(!isBeingLoaded) {
            rs->release();
            resourcesToReload.push_back(rs);
        } else if(debug) {
            debugLog("Resource {:s} is currently being loaded, skipping reload", rs->getDebugIdentifier());
        }
    }

    for(Resource *rs : resourcesToReload) {
        requestAsyncLoad(rs);
    }
}

bool AsyncResourceLoader::isLoadingResource(const Resource *resource) const {
    Sync::scoped_lock lock(this->loadingResourcesMutex);
    return this->loadingResourcesSet.contains(resource);
}

void AsyncResourceLoader::ensureThreadAvailable() {
    size_t activeThreads = this->iActiveThreadCount.load(std::memory_order_acquire);
    size_t activeWorkCount = this->iActiveWorkCount.load(std::memory_order_acquire);

    if(activeWorkCount > activeThreads && activeThreads < this->iMaxThreads) {
        Sync::scoped_lock lock(this->threadsMutex);

        if(this->threadpool.size() < this->iMaxThreads) {
            const bool debug = cv::debug_rm.getBool();

            const size_t idx = this->iTotalThreadsCreated.fetch_add(1, std::memory_order_relaxed);
            auto loaderThread = std::make_unique<LoaderThread>(this, idx);

            if(!loaderThread->isReady()) {
                logIf(debug, "W: Couldn't create dynamic thread!");
            } else {
                logIf(debug, "Created dynamic thread #{} (total: {})", idx, this->threadpool.size() + 1);

                this->threadpool[idx] = std::move(loaderThread);
            }
        }
    }
}

void AsyncResourceLoader::cleanupIdleThreads() {
    if(this->threadpool.size() <= MIN_NUM_THREADS) return;

    // only run cleanup periodically to avoid overhead
    auto now = chrono::steady_clock::now();
    if(chrono::duration_cast<chrono::milliseconds>(now - this->lastCleanupTime) < IDLE_GRACE_PERIOD) {
        return;
    }
    this->lastCleanupTime = now;

    // don't cleanup if we still have work
    if(this->iActiveWorkCount.load(std::memory_order_acquire) > 0) return;

    Sync::scoped_lock lock(this->threadsMutex);

    if(this->threadpool.size() <= MIN_NUM_THREADS) return;  // check under lock again

    const bool debug = cv::debug_rm.getBool();

    // find threads that have been idle too long
    for(auto &[idx, thread] : this->threadpool) {
        if(thread->isIdleTooLong()) {
            logIf(debug, "Removing idle thread #{} (idle timeout exceeded, pool size: {} -> {})", idx,
                  this->threadpool.size(), this->threadpool.size() - 1);
            this->threadpool.erase(idx);
            break;  // remove only one thread at a time
        }
    }
}

std::unique_ptr<AsyncResourceLoader::LoadingWork> AsyncResourceLoader::getNextPendingWork() {
    Sync::scoped_lock lock(this->workQueueMutex);

    if(this->pendingWork.empty()) return nullptr;

    auto work = std::move(this->pendingWork.front());
    this->pendingWork.pop();
    return work;
}

void AsyncResourceLoader::markWorkAsyncComplete(std::unique_ptr<LoadingWork> work) {
    Sync::scoped_lock lock(this->workQueueMutex);
    this->asyncCompleteWork.push(std::move(work));
}

std::unique_ptr<AsyncResourceLoader::LoadingWork> AsyncResourceLoader::getNextAsyncCompleteWork() {
    Sync::scoped_lock lock(this->workQueueMutex);

    if(this->asyncCompleteWork.empty()) return nullptr;

    auto work = std::move(this->asyncCompleteWork.front());
    this->asyncCompleteWork.pop();
    return work;
}
