// Copyright (c) 2025, WH, All rights reserved.

#include "BaseEnvironment.h"

#ifdef MCENGINE_FEATURE_SOLOUD

#include "Thread.h"
#include "Timing.h"
#include "SyncCV.h"
#include "SyncJthread.h"

#include "soloud.h"

#include <atomic>
#include <queue>
#include <future>
#include <functional>
#include <chrono>
#include <cassert>

class SoLoudThreadWrapper {
    NOCOPY_NOMOVE(SoLoudThreadWrapper)

    // base class for type-erased tasks
    struct TaskBase {
        NOCOPY_NOMOVE(TaskBase)
       public:
        TaskBase() noexcept = default;
        virtual ~TaskBase() noexcept = default;
        virtual void execute() noexcept = 0;
    };

    // concrete task implementation
    template <typename T>
    struct Task : TaskBase {
        std::function<T()> work;
        std::promise<T> promise;

        Task(std::function<T()> w) noexcept : work(std::move(w)) {}

        void execute() noexcept override {
            if constexpr(std::is_void_v<T>) {
                this->work();
                this->promise.set_value();
            } else {
                this->promise.set_value(this->work());
            }
        }

        std::future<T> get_future() { return this->promise.get_future(); }
    };

    // fire-and-forget task
    struct FireAndForgetTask : TaskBase {
        std::function<void()> work;

        FireAndForgetTask(std::function<void()> w) noexcept : work(std::move(w)) {}

        void execute() noexcept override { this->work(); }
    };

   public:
    SoLoudThreadWrapper(bool threaded = false) noexcept : threaded(threaded) {
        if(this->threaded) {
            this->start_worker_thread();
        } else {
            this->soloud = std::make_unique<SoLoud::Soloud>();
            this->initialized = true;
        }
    }

    ~SoLoudThreadWrapper() noexcept {
        if(this->threaded) {
            this->shutdown_worker_thread();
        } else {
            if(this->soloud) {
                this->soloud->deinit();
                this->soloud.reset();
            }
        }
    }

    // synchronous access: executes on audio thread but waits for completion
    template <typename F>
    auto sync(F&& func) -> std::invoke_result_t<F> {
        if(likely(!this->threaded)) return func();
        using ReturnType = std::invoke_result_t<F>;

        if constexpr(std::is_void_v<ReturnType>) {
            auto task = std::make_unique<Task<void>>(std::forward<F>(func));
            auto future = task->get_future();

            {
                Sync::scoped_lock<Sync::mutex> lock(queue_mutex);
                this->task_queue.push(std::move(task));
            }
            this->queue_cv.notify_one();

            future.wait();
        } else {
            auto task = std::make_unique<Task<ReturnType>>(std::forward<F>(func));
            auto future = task->get_future();

            {
                Sync::scoped_lock<Sync::mutex> lock(queue_mutex);
                this->task_queue.push(std::move(task));
            }
            this->queue_cv.notify_one();

            return future.get();
        }
    }

    // for future reference: example for async play for cases where we don't need the handle immediately
    /*
    std::future<SOUNDHANDLE> SoLoudSoundEngine::playAsync(Sound *snd, f32 pan, f32 pitch, f32 playVolume) {
        if(!this->isReady() || snd == nullptr || !snd->isReady()) {
            // return a ready future with invalid handle
            std::promise<SOUNDHANDLE> promise;
            promise.set_value(0);
            return promise.get_future();
        }

        auto *soloudSound = snd->as<SoLoudSound>();
        if(!soloudSound) {
            std::promise<SOUNDHANDLE> promise;
            promise.set_value(0);
            return promise.get_future();
        }

        pitch += 1.0f;
        pan = std::clamp<float>(pan, -1.0f, 1.0f);
        pitch = std::clamp<float>(pitch, 0.01f, 2.0f);

        // return future for async execution
        return soloud->play_async(*soloudSound->audioSource, soloudSound->fBaseVolume * playVolume, pan, true);
    }
    */
    // asynchronous access: returns future for result
    template <typename F>
    auto async(F&& func) -> std::future<std::invoke_result_t<F>> {
        assert(this->threaded && "can't run SoLoud calls async without threading.");
        using ReturnType = std::invoke_result_t<F>;

        auto task = std::make_unique<Task<ReturnType>>(std::forward<F>(func));
        auto future = task->get_future();

        {
            Sync::scoped_lock<Sync::mutex> lock(queue_mutex);
            this->task_queue.push(std::move(task));
        }
        this->queue_cv.notify_one();

        return future;
    }

    // fire-and-forget: no return value, no waiting
    template <typename F>
    void fire_and_forget(F&& func) {
        if(likely(!this->threaded)) {
            func();
            return;
        }
        auto task = std::make_unique<FireAndForgetTask>(std::forward<F>(func));

        {
            Sync::scoped_lock<Sync::mutex> lock(queue_mutex);
            this->task_queue.push(std::move(task));
        }
        this->queue_cv.notify_one();
    }

    // convenience passthroughs for the current methods we need
    void deinit() {
        this->sync([this]() {
            if(this->soloud) {
                this->soloud->deinit();
            }
        });
    }

    SoLoud::result init(unsigned int aFlags = SoLoud::Soloud::CLIP_ROUNDOFF,
                        unsigned int aBackend = SoLoud::Soloud::AUTO, unsigned int aSamplerate = SoLoud::Soloud::AUTO,
                        unsigned int aBufferSize = SoLoud::Soloud::AUTO, unsigned int aChannels = 2) {
        if(likely(!this->threaded)) {
            return this->soloud->init(aFlags, aBackend, aSamplerate, aBufferSize, aChannels);
        }
        // create task manually to implement timeout
        auto task =
            std::make_unique<Task<SoLoud::result>>([this, aFlags, aBackend, aSamplerate, aBufferSize, aChannels]() {
                return this->init_with_name(aFlags, aBackend, aSamplerate, aBufferSize, aChannels);
            });
        auto future = task->get_future();

        {
            Sync::scoped_lock<Sync::mutex> lock(queue_mutex);
            this->task_queue.push(std::move(task));
        }
        this->queue_cv.notify_one();

        // wait with 10 second timeout
        if(future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
            // restart the entire worker thread to recover from hung init
            this->restart_worker_thread();
            return SoLoud::UNKNOWN_ERROR;
        }

        return future.get();
    }

    SoLoud::handle play(SoLoud::AudioSource& aSound, float aVolume = -1.0f, float aPan = 0.0f, bool aPaused = false) {
        return this->sync([&, aVolume, aPan, aPaused]() { return this->soloud->play(aSound, aVolume, aPan, aPaused); });
    }

    // NOTE: currently unused
    std::future<SoLoud::handle> play_async(SoLoud::AudioSource& aSound, float aVolume = -1.0f, float aPan = 0.0f,
                                           bool aPaused = false) {
        return this->async(
            [&, aVolume, aPan, aPaused]() { return this->soloud->play(aSound, aVolume, aPan, aPaused); });
    }

    void setPause(SoLoud::handle aVoiceHandle, bool aPause) {
        this->fire_and_forget([this, aVoiceHandle, aPause]() { this->soloud->setPause(aVoiceHandle, aPause); });
    }

    void setVolume(SoLoud::handle aVoiceHandle, float aVolume) {
        this->fire_and_forget([this, aVoiceHandle, aVolume]() { this->soloud->setVolume(aVoiceHandle, aVolume); });
    }

    void fadeVolume(SoLoud::handle aVoiceHandle, float aTo, float aTime) {
        this->fire_and_forget(
            [this, aVoiceHandle, aTo, aTime]() { this->soloud->fadeVolume(aVoiceHandle, aTo, aTime); });
    }

    void setRelativePlaySpeed(SoLoud::handle aVoiceHandle, float aSpeed) {
        this->fire_and_forget(
            [this, aVoiceHandle, aSpeed]() { this->soloud->setRelativePlaySpeed(aVoiceHandle, aSpeed); });
    }

    void setProtectVoice(SoLoud::handle aVoiceHandle, bool aProtect) {
        this->fire_and_forget(
            [this, aVoiceHandle, aProtect]() { this->soloud->setProtectVoice(aVoiceHandle, aProtect); });
    }

    void setSamplerate(SoLoud::handle aVoiceHandle, float aSamplerate) {
        this->fire_and_forget(
            [this, aVoiceHandle, aSamplerate]() { this->soloud->setSamplerate(aVoiceHandle, aSamplerate); });
    }

    void setPan(SoLoud::handle aVoiceHandle, float aPan) {
        this->fire_and_forget([this, aVoiceHandle, aPan]() { this->soloud->setPan(aVoiceHandle, aPan); });
    }

    void setLooping(SoLoud::handle aVoiceHandle, bool aLooping) {
        this->fire_and_forget([this, aVoiceHandle, aLooping]() { this->soloud->setLooping(aVoiceHandle, aLooping); });
    }

    void setGlobalVolume(float aVolume) {
        this->fire_and_forget([this, aVolume]() { this->soloud->setGlobalVolume(aVolume); });
    }

    void setPostClipScaler(float aScaler) {
        this->fire_and_forget([this, aScaler]() { this->soloud->setPostClipScaler(aScaler); });
    }

    void stop(SoLoud::handle aVoiceHandle) {
        this->fire_and_forget([this, aVoiceHandle]() { this->soloud->stop(aVoiceHandle); });
    }

    void seek_async(SoLoud::handle aVoiceHandle, SoLoud::time aSeconds) {
        this->fire_and_forget([this, aVoiceHandle, aSeconds]() { this->soloud->seek(aVoiceHandle, aSeconds); });
    }

    // use sync for methods that need return values (or where we want the effect to be applied immediately)
    void seek(SoLoud::handle aVoiceHandle, SoLoud::time aSeconds) {
        this->sync([this, aVoiceHandle, aSeconds]() { this->soloud->seek(aVoiceHandle, aSeconds); });
    }

    bool isValidVoiceHandle(SoLoud::handle aVoiceHandle) {
        return this->sync([this, aVoiceHandle]() { return this->soloud->isValidVoiceHandle(aVoiceHandle); });
    }

    SoLoud::time getStreamPosition(SoLoud::handle aVoiceHandle) {
        return this->sync([this, aVoiceHandle]() { return this->soloud->getStreamPosition(aVoiceHandle); });
    }

    bool getPause(SoLoud::handle aVoiceHandle) {
        return this->sync([this, aVoiceHandle]() { return this->soloud->getPause(aVoiceHandle); });
    }

    unsigned int getBackendSamplerate() {
        return this->sync([this]() { return this->soloud->getBackendSamplerate(); });
    }

    unsigned int getBackendBufferSize() {
        return this->sync([this]() { return this->soloud->getBackendBufferSize(); });
    }

    unsigned int getBackendChannels() {
        return this->sync([this]() { return this->soloud->getBackendChannels(); });
    }

    const char* getBackendString() {
        return this->sync([this]() { return this->soloud->getBackendString(); });
    }

    unsigned int getMaxActiveVoiceCount() {
        return this->sync([this]() { return this->soloud->getMaxActiveVoiceCount(); });
    }

    SoLoud::result setMaxActiveVoiceCount(unsigned int aVoiceCount) {
        return this->sync([aVoiceCount, this]() { return this->soloud->setMaxActiveVoiceCount(aVoiceCount); });
    }

    unsigned int getActiveVoiceCount() {
        return this->sync([this]() { return this->soloud->getActiveVoiceCount(); });
    }

    SoLoud::result getCurrentDevice(SoLoud::DeviceInfo* aInfo) {
        return this->sync([this, aInfo]() { return this->soloud->getCurrentDevice(aInfo); });
    }

    SoLoud::result enumerateDevices(SoLoud::DeviceInfo** aDevices, unsigned int* aCount) {
        return this->sync([this, aDevices, aCount]() { return this->soloud->enumerateDevices(aDevices, aCount); });
    }

    SoLoud::result setDevice(const char* aDeviceIdentifier) {
        return this->sync([this, aDeviceIdentifier]() { return this->soloud->setDevice(aDeviceIdentifier); });
    }

    // async position update helper (so we don't need to run tasks recursively)
    void updateCachedPosition(SoLoud::handle aVoiceHandle, std::atomic<double>& cacheTime,
                              std::atomic<double>& cachedPosition) {
        this->fire_and_forget([this, aVoiceHandle, &cacheTime, &cachedPosition]() {
            cachedPosition.store(this->soloud->getStreamPosition(aVoiceHandle), std::memory_order_release);
            cacheTime.store(Timing::getTimeReal(), std::memory_order_release);
        });
    }

    [[nodiscard]] constexpr bool isThreaded() const { return this->threaded; }

   private:
    void start_worker_thread() {
        this->worker_thread = Sync::jthread([this](const Sync::stop_token& stoken) { this->worker_loop(stoken); });

        // wait for initialization to complete
        Sync::unique_lock<Sync::mutex> lock(this->init_mutex);
        this->init_cv.wait(lock, [this] { return this->initialized; });
    }

    void shutdown_worker_thread() {
        this->shutting_down.store(true, std::memory_order_release);
        this->queue_cv.notify_all();

        // wait for worker to finish (this sets the stop_token)
        if(this->worker_thread.joinable()) {
            this->worker_thread.join();
        }
    }

    void restart_worker_thread() {
        // shutdown current thread (it may be hung, so detach if needed)
        this->shutting_down.store(true, std::memory_order_release);
        this->queue_cv.notify_all();

        if(this->worker_thread.joinable()) {
            // give it a bit to shutdown normally
            if(this->worker_thread.get_stop_source().stop_requested()) {
                this->worker_thread.join();
            } else {
                // if it doesn't respond to stop request, it probably hung (so detach it)
                // this is probably dangerous and can leave a zombie thread that we can't do anything about,
                // but it's the best we can do at the moment
                this->worker_thread.detach();
            }
        }

        // clear any remaining tasks from the old thread
        {
            Sync::scoped_lock<Sync::mutex> lock(this->queue_mutex);
            while(!this->task_queue.empty()) {
                this->task_queue.pop();
            }
        }

        // reset state and start a new worker
        this->shutting_down.store(false, std::memory_order_release);
        this->initialized = false;

        this->start_worker_thread();
    }

    void worker_loop(const Sync::stop_token& stoken) noexcept {
        McThread::set_current_thread_name("soloud_mixer");
        McThread::set_current_thread_prio(McThread::Priority::REALTIME); // raise priority to the max

        // initialize SoLoud on the audio thread
        this->soloud = std::make_unique<SoLoud::Soloud>();

        // signal completion
        {
            Sync::scoped_lock<Sync::mutex> lock(this->init_mutex);
            this->initialized = true;
        }
        this->init_cv.notify_one();

        // main processing loop
        while(!stoken.stop_requested() && !this->shutting_down.load(std::memory_order_acquire)) {
            Sync::unique_lock<Sync::mutex> lock(this->queue_mutex);

            // wait for tasks or stop signal
            this->queue_cv.wait(lock, stoken, [&] { return !this->task_queue.empty() || this->shutting_down.load(std::memory_order_acquire); });

            // process all available tasks
            while(!this->task_queue.empty() && !stoken.stop_requested() && !this->shutting_down.load(std::memory_order_acquire)) {
                auto task = std::move(this->task_queue.front());
                this->task_queue.pop();

                // unlock while executing task
                lock.unlock();
                task->execute();
                lock.lock();

                // yield after execution to give other threads time
                Timing::sleep(0);
            }
            Timing::sleep(0);
        }

        // cleanup/process remaining tasks before shutdown
        while(!this->task_queue.empty()) {
            auto task = std::move(this->task_queue.front());
            this->task_queue.pop();
            task->execute();
        }

        // deinitialize SoLoud
        if(this->soloud) {
            this->soloud->deinit();
            this->soloud.reset();
        }
    }

    // since the backend may create a thread of its own
    SoLoud::result init_with_name(unsigned int aFlags, unsigned int aBackend, unsigned int aSamplerate,
                                  unsigned int aBufferSize, unsigned int aChannels) {
        const char* old_thread_name = McThread::get_current_thread_name();
        McThread::set_current_thread_name("soloud_output");
        const auto result = this->soloud->init(aFlags, aBackend, aSamplerate, aBufferSize, aChannels);
        McThread::set_current_thread_name(old_thread_name);
        return result;
    }

    std::unique_ptr<SoLoud::Soloud> soloud{nullptr};
    Sync::jthread worker_thread;

    // task queue
    std::queue<std::unique_ptr<TaskBase>> task_queue;
    mutable Sync::mutex queue_mutex;
    Sync::condition_variable_any queue_cv;

    // init/shutdown signaling

    // this extra flag shouldn't be required, but a (likely) glibc bug prevents the stop_token from reliably signaling the condition_variable_all
    // https://sourceware.org/pipermail/glibc-bugs/2020-April/047751.html
    std::atomic<bool> shutting_down{false};

    mutable Sync::mutex init_mutex;
    Sync::condition_variable init_cv;

    bool initialized{false};
    bool threaded{false};
};

#endif
