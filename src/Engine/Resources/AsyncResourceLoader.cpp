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
#include <thread>

using namespace std::chrono_literals;

//==================================
// LOADER THREAD
//==================================
class AsyncResourceLoader::LoaderThread final {
   public:
    size_t thread_index;
    std::atomic<std::chrono::steady_clock::time_point> last_active;

    LoaderThread(AsyncResourceLoader *const loader, size_t index) noexcept
        : thread_index(index),
          last_active(std::chrono::steady_clock::now()),
          loader_ptr(loader),
          thread(Sync::jthread([this](const Sync::stop_token &stoken) { this->run(stoken); })) {}

    [[nodiscard]] bool isReady() const noexcept { return this->thread.joinable(); }

    [[nodiscard]] bool isIdleTooLong() const noexcept {
        auto lastActive = this->last_active.load(std::memory_order_acquire);
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - lastActive) > IDLE_TIMEOUT;
    }

   private:
    AsyncResourceLoader *const loader_ptr;
    Sync::jthread thread;

    void run(const Sync::stop_token &stoken) noexcept {
        this->loader_ptr->iActiveThreadCount.fetch_add(1);

        logIfCV(debug_rm, "AsyncResourceLoader: Thread #{} started", this->thread_index);

        const std::string loaderThreadName =
            fmt::format("res_ldr_thr{}", (this->thread_index % this->loader_ptr->iMaxThreads) + 1);
        McThread::set_current_thread_name(loaderThreadName.c_str());
        McThread::set_current_thread_prio(false);  // reset priority (don't inherit from main thread)

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
            this->last_active.store(std::chrono::steady_clock::now(), std::memory_order_release);

            Resource *resource = work->resource;
            work->state.store(WorkState::ASYNC_IN_PROGRESS, std::memory_order_release);

            std::string debugName;
            if(debug) {
                debugName = std::string{resource->getName()};
                debugLog("AsyncResourceLoader: Thread #{} loading {:8p} : {:s}", this->thread_index,
                         static_cast<const void *>(resource), debugName);
            }

            // prevent child threads from inheriting the name
            McThread::set_current_thread_name(fmt::format("res_{}", work->workId).c_str());

            resource->loadAsync();

            // restore loader thread name
            McThread::set_current_thread_name(loaderThreadName.c_str());
            logIf(debug, "AsyncResourceLoader: Thread #{} finished loadAsync() (ready for load(): {}) {:8p} : {:s}",
                  this->thread_index, resource->isReadyForSyncInit(), static_cast<const void *>(resource), debugName);

            if(resource->isReadyForSyncInit()) {
                work->state.store(WorkState::ASYNC_COMPLETE, std::memory_order_release);
            }  // otherwise it's still not ready for sync load()

            // put it in sync pending
            {
                Sync::scoped_lock lock(this->loader_ptr->workQueueMutex);
                this->loader_ptr->syncPendingWork.push_back(std::move(work));
            }

            // yield again before loop
            Timing::sleepMS(0);
        }

        this->loader_ptr->iActiveThreadCount.fetch_sub(1);

        logIfCV(debug_rm, "AsyncResourceLoader: Thread #{} exiting", this->thread_index);
    }
};

//==================================
// ASYNC RESOURCE LOADER
//==================================

AsyncResourceLoader::AsyncResourceLoader()
    : iMaxThreads(std::clamp<size_t>(std::thread::hardware_concurrency() - 1, 1, HARD_THREADCOUNT_LIMIT)),
      iLoadsPerUpdate(this->iMaxThreads),
      lastCleanupTime(std::chrono::steady_clock::now()) {
    // pre-create at least a single thread for better startup responsiveness
    Sync::scoped_lock lock(this->threadsMutex);

    const size_t idx = this->iTotalThreadsCreated.fetch_add(1);
    auto loaderThread = std::make_unique<LoaderThread>(this, idx);

    if(!loaderThread->isReady()) {
        engine->showMessageError("AsyncResourceLoader Error", "Couldn't create core thread!");
    } else {
        logIfCV(debug_rm, "AsyncResourceLoader: Created initial thread");
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
        while(!this->asyncPendingWork.empty()) {
            this->asyncPendingWork.pop();
        }
        while(!this->syncPendingWork.empty()) {
            this->syncPendingWork.pop_front();
        }
    }

    // cleanup loading resources tracking
    {
        Sync::scoped_lock lock(this->loadingResourcesMutex);
        this->loadingResourcesSet.clear();
    }

    // cleanup async destroy queue
    for(auto &rs : this->asyncDestroyQueue) {
        SAFE_DELETE(rs);
    }
    this->asyncDestroyQueue.clear();
}

void AsyncResourceLoader::requestAsyncLoad(Resource *resource) {
    auto work = std::make_unique<LoadingWork>(resource, this->iWorkIdCounter.fetch_add(1));

    // add to tracking set
    {
        Sync::scoped_lock lock(this->loadingResourcesMutex);
        this->loadingResourcesSet.insert(resource);
    }

    // add to work queue
    {
        Sync::scoped_lock lock(this->workQueueMutex);
        this->asyncPendingWork.push(std::move(work));
    }

    this->iActiveWorkCount.fetch_add(1);
    ensureThreadAvailable();
    this->workAvailable.notify_one();
}

void AsyncResourceLoader::update(bool lowLatency) {
    if(!lowLatency) cleanupIdleThreads();
    const bool debug = cv::debug_rm.getBool();

    const size_t amountToProcess = lowLatency ? 1 : this->iLoadsPerUpdate;

    // things which are still not ready for sync init to put back into the queue after the loop
    std::vector<std::unique_ptr<LoadingWork>> stillLoadingWork;

    // process completed async work
    size_t numProcessed = 0;
    while(numProcessed < amountToProcess) {
        auto work = getNextAsyncCompleteWork();
        if(!work) {
            // decay back to default
            this->iLoadsPerUpdate =
                std::max(static_cast<size_t>((this->iLoadsPerUpdate) * (3.0 / 4.0)), this->iMaxThreads);
            break;
        }

        bool processed = false;
        Resource *rs = work->resource;
        if(work->state == WorkState::ASYNC_COMPLETE ||
           (work->state == WorkState::ASYNC_IN_PROGRESS && rs->isReadyForSyncInit())) {
            const bool interrupted = rs->isInterrupted();
            if(!interrupted) {
                logIf(debug, "AsyncResourceLoader: Sync init for {:s} ({:8p})", rs->getName(),
                      static_cast<const void *>(rs));
                rs->load();
                processed = true;
            } else {
                logIf(debug, "AsyncResourceLoader: Skipping sync init for {:s} ({:8p}) due to interruption",
                      rs->getName(), static_cast<const void *>(rs));
            }

            work->state.store(WorkState::SYNC_COMPLETE, std::memory_order_release);

            // remove from tracking set
            {
                Sync::scoped_lock lock(this->loadingResourcesMutex);
                this->loadingResourcesSet.erase(rs);
            }

            this->iActiveWorkCount.fetch_sub(1);
        } else {
            processed = true;  // check back next time
            logIf(debug, "AsyncResourceLoader: Skipping sync init for {:s} ({:8p}) due to ASYNC_IN_PROGRESS",
                  rs->getName(), static_cast<const void *>(rs));
            stillLoadingWork.push_back(std::move(work));
        }

        if(processed) numProcessed++;
    }

    {
        Sync::scoped_lock lock(this->workQueueMutex);
        // put these at the front so we check them early next time
        for(auto &&work : stillLoadingWork) {
            this->syncPendingWork.push_front(std::move(work));
        }
    }

    // process async destroy queue
    std::vector<Resource *> resourcesReadyForDestroy;

    {
        Sync::scoped_lock lock(this->asyncDestroyMutex);
        for(size_t i = 0; i < this->asyncDestroyQueue.size(); i++) {
            bool canBeDestroyed = true;

            {
                Sync::scoped_lock loadingLock(this->loadingResourcesMutex);
                if(this->loadingResourcesSet.find(this->asyncDestroyQueue[i]) != this->loadingResourcesSet.end()) {
                    canBeDestroyed = false;
                }
            }

            if(canBeDestroyed) {
                resourcesReadyForDestroy.push_back(this->asyncDestroyQueue[i]);
                this->asyncDestroyQueue.erase(this->asyncDestroyQueue.begin() + i);
                i--;
            }
        }
    }

    for(Resource *rs : resourcesReadyForDestroy) {
        logIf(debug, "AsyncResourceLoader: Async destroy of resource {:8p} : {:s}", static_cast<const void *>(rs),
              rs->getName());

        SAFE_DELETE(rs);
    }
}

void AsyncResourceLoader::scheduleAsyncDestroy(Resource *resource) {
    logIfCV(debug_rm, "AsyncResourceLoader: Scheduled async destroy of {:s}", resource->getName());

    Sync::scoped_lock lock(this->asyncDestroyMutex);
    this->asyncDestroyQueue.push_back(resource);
}

void AsyncResourceLoader::reloadResources(const std::vector<Resource *> &resources) {
    const bool debug = cv::debug_rm.getBool();
    if(resources.empty()) {
        logIf(debug, "AsyncResourceLoader Warning: reloadResources with empty resources vector!");
        return;
    }

    logIf(debug, "AsyncResourceLoader: Async reloading {} resources", resources.size());

    std::vector<Resource *> resourcesToReload;
    for(Resource *rs : resources) {
        if(rs == nullptr) continue;

        logIf(debug, "AsyncResourceLoader: Async reloading {:8p} : {:s}", static_cast<const void *>(rs), rs->getName());

        bool isBeingLoaded = isLoadingResource(rs);

        if(!isBeingLoaded) {
            rs->release();
            resourcesToReload.push_back(rs);
        } else if(debug) {
            debugLog("AsyncResourceLoader: Resource {:8p} : {:s} is currently being loaded, skipping reload",
                     static_cast<const void *>(rs), rs->getName());
        }
    }

    for(Resource *rs : resourcesToReload) {
        requestAsyncLoad(rs);
    }
}

bool AsyncResourceLoader::isLoadingResource(Resource *resource) const {
    Sync::scoped_lock lock(this->loadingResourcesMutex);
    return this->loadingResourcesSet.find(resource) != this->loadingResourcesSet.end();
}

void AsyncResourceLoader::ensureThreadAvailable() {
    size_t activeThreads = this->iActiveThreadCount.load(std::memory_order_acquire);
    size_t activeWorkCount = this->iActiveWorkCount.load(std::memory_order_acquire);

    if(activeWorkCount > activeThreads && activeThreads < this->iMaxThreads) {
        Sync::scoped_lock lock(this->threadsMutex);

        if(this->threadpool.size() < this->iMaxThreads) {
            const bool debug = cv::debug_rm.getBool();

            const size_t idx = this->iTotalThreadsCreated.fetch_add(1);
            auto loaderThread = std::make_unique<LoaderThread>(this, idx);

            if(!loaderThread->isReady()) {
                logIf(debug, "AsyncResourceLoader Warning: Couldn't create dynamic thread!");
            } else {
                logIf(debug, "AsyncResourceLoader: Created dynamic thread #{} (total: {})", idx,
                      this->threadpool.size() + 1);

                this->threadpool[idx] = std::move(loaderThread);
            }
        }
    }
}

void AsyncResourceLoader::cleanupIdleThreads() {
    if(this->threadpool.size() <= MIN_NUM_THREADS) return;

    // only run cleanup periodically to avoid overhead
    auto now = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastCleanupTime) < IDLE_GRACE_PERIOD) {
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
            logIf(debug, "AsyncResourceLoader: Removing idle thread #{} (idle timeout exceeded, pool size: {} -> {})",
                  idx, this->threadpool.size(), this->threadpool.size() - 1);
            this->threadpool.erase(idx);
            break;  // remove only one thread at a time
        }
    }
}

std::unique_ptr<AsyncResourceLoader::LoadingWork> AsyncResourceLoader::getNextPendingWork() {
    Sync::scoped_lock lock(this->workQueueMutex);

    if(this->asyncPendingWork.empty()) return nullptr;

    auto work = std::move(this->asyncPendingWork.front());
    this->asyncPendingWork.pop();
    return work;
}

std::unique_ptr<AsyncResourceLoader::LoadingWork> AsyncResourceLoader::getNextAsyncCompleteWork() {
    Sync::scoped_lock lock(this->workQueueMutex);

    if(this->syncPendingWork.empty()) return nullptr;

    auto work = std::move(this->syncPendingWork.front());
    this->syncPendingWork.pop_front();
    return work;
}
