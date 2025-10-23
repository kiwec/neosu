#pragma once

#include "Resource.h"
#include "SyncCV.h"

#include <algorithm>
#include <atomic>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ConVar;

class AsyncResourceLoader final {
    NOCOPY_NOMOVE(AsyncResourceLoader)

    friend class ResourceManager;

   public:
    AsyncResourceLoader();
    ~AsyncResourceLoader();

   private:
    // main interface for ResourceManager
    inline void resetMaxPerUpdate() { this->bMaxLoadsResetPending = true; }
    inline void setMaxPerUpdate(size_t num) { this->iLoadsPerUpdate = std::clamp<size_t>(num, 1, 512); }
    void requestAsyncLoad(Resource *resource);
    void update(bool lowLatency);
    void shutdown();

    // resource lifecycle management
    void scheduleAsyncDestroy(Resource *resource);
    void reloadResources(const std::vector<Resource *> &resources);

    // status queries
    [[nodiscard]] inline bool isLoading() const { return this->iActiveWorkCount.load(std::memory_order_acquire) > 0; }
    [[nodiscard]] bool isLoadingResource(Resource *resource) const;
    [[nodiscard]] size_t getNumLoadingWork() const { return this->iActiveWorkCount.load(std::memory_order_acquire); }
    [[nodiscard]] size_t getNumActiveThreads() const {
        return this->iActiveThreadCount.load(std::memory_order_acquire);
    }
    [[nodiscard]] inline size_t getNumLoadingWorkAsyncDestroy() const { return this->asyncDestroyQueue.size(); }
    [[nodiscard]] inline size_t getMaxPerUpdate() const { return this->iLoadsPerUpdate; }

    enum class WorkState : uint8_t { PENDING = 0, ASYNC_IN_PROGRESS = 1, ASYNC_COMPLETE = 2, SYNC_COMPLETE = 3 };

    struct LoadingWork {
        Resource *resource;
        size_t workId;
        std::atomic<WorkState> state{WorkState::PENDING};

        LoadingWork(Resource *res, size_t id) : resource(res), workId(id) {}
    };

    class LoaderThread;
    friend class LoaderThread;

    // thread management
    void ensureThreadAvailable();
    void cleanupIdleThreads();

    // work queue management
    std::unique_ptr<LoadingWork> getNextPendingWork();
    std::unique_ptr<LoadingWork> getNextAsyncCompleteWork();

    // set during ctor, dependent on hardware
    size_t iMaxThreads;
    static constexpr const size_t HARD_THREADCOUNT_LIMIT{32};
    // always keep one thread around to avoid unnecessary thread creation/destruction spikes for spurious loads
    static constexpr const size_t MIN_NUM_THREADS{1};

    // how many resources to load on update()
    // default is == max # threads (or 1 during gameplay)
    size_t iLoadsPerUpdate;
    bool bMaxLoadsResetPending{false};

    // thread idle configuration
    static constexpr std::chrono::milliseconds IDLE_GRACE_PERIOD{1000};  // 1 sec
    static constexpr std::chrono::milliseconds IDLE_TIMEOUT{15000};      // 15 sec
    std::chrono::steady_clock::time_point lastCleanupTime;

    // thread pool
    std::unordered_map<size_t, std::unique_ptr<LoaderThread>> threadpool;  // index to thread
    mutable Sync::mutex threadsMutex;

    // thread lifecycle tracking
    std::atomic<size_t> iActiveThreadCount{0};
    std::atomic<size_t> iTotalThreadsCreated{0};

    // separate queues for different work states (avoids O(n) scanning)
    std::queue<std::unique_ptr<LoadingWork>> asyncPendingWork;
    // ASYNC_IN_PROGRESS gets put at the back, ASYNC_COMPLETE gets put at the front (prioritized)
    std::deque<std::unique_ptr<LoadingWork>> syncPendingWork;

    // single mutex for both work queues (they're accessed in sequence, not concurrently)
    mutable Sync::mutex workQueueMutex;

    // fast lookup for checking if a resource is being loaded
    std::unordered_set<Resource *> loadingResourcesSet;
    mutable Sync::mutex loadingResourcesMutex;

    // atomic counters for efficient status queries
    std::atomic<size_t> iActiveWorkCount{0};
    std::atomic<size_t> iWorkIdCounter{0};

    // work notification
    Sync::condition_variable_any workAvailable;
    Sync::mutex workAvailableMutex;

    // async destroy queue
    std::vector<Resource *> asyncDestroyQueue;
    Sync::mutex asyncDestroyMutex;

    // lifecycle flags
    std::atomic<bool> bShuttingDown{false};
};
