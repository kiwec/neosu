// Copyright (c) 2025 kiwec, All rights reserved.
#include "DirectoryWatcher.h"

#include "Thread.h"
#include "Timing.h"
#include "SyncJthread.h"
#include "SyncMutex.h"

#include <algorithm>
#include <vector>
#include <atomic>

namespace fs = std::filesystem;

class DirectoryWatcher::WatcherImpl {
    NOCOPY_NOMOVE(WatcherImpl)
   public:
    WatcherImpl() : thr([this](const Sync::stop_token& stoken) { this->worker_loop(stoken); }) {}
    ~WatcherImpl() = default;

    void watch_directory(std::string path, FileChangeCallback cb) {
        Sync::scoped_lock lock(this->directories_mtx);
        this->directories_to_add.emplace_back(std::move(path), std::move(cb));
    }

    void stop_watching(std::string path) {
        Sync::scoped_lock lock(this->directories_mtx);
        this->directories_to_remove.push_back(std::move(path));
    }

    void update() {
        if(this->finished_events_count.load(std::memory_order_acquire) == 0) return;

        Sync::scoped_lock lock(this->finished_events_mtx);
        for(const auto& [cb, event] : this->finished_events) {
            cb(event);
        }
        this->finished_events.clear();
        this->finished_events_count.store(0, std::memory_order_release);
    }

   private:
    Sync::mutex directories_mtx;
    std::vector<std::pair<std::string, FileChangeCallback>> directories_to_add;
    std::vector<std::string> directories_to_remove;

    Sync::mutex finished_events_mtx;
    std::vector<std::pair<FileChangeCallback, FileChangeEvent>> finished_events;
    std::atomic<uSz> finished_events_count{0};

    Sync::jthread thr;

    static std::unordered_map<std::string, fs::file_time_type> getFileTimes(const std::string& dir_path) {
        // TODO: does this work with unicode paths on windows?
        std::unordered_map<std::string, fs::file_time_type> files;

        std::error_code ec;
        for(const auto& entry : fs::directory_iterator(dir_path, ec)) {
            if(ec) continue;
            auto fileType = entry.status(ec).type();
            if(fileType != fs::file_type::regular) continue;

            files[entry.path().string()] = fs::last_write_time(entry);
        }

        return files;
    }

    void worker_loop(const Sync::stop_token& stoken) {
        McThread::set_current_thread_name("dir_watcher");
        McThread::set_current_thread_prio(McThread::Priority::LOW);

        // Windows & OSX do not provide APIs that tell you when a file is
        // *done* being written to; and thus, they require debouncing, i.e.
        // waiting 100ms to make sure it's no longer getting writes.

        // To keep things "simple" for now, we'll just do the simplest method
        // that works on all platforms: manually checking for changes.

        // The downside is that this doesn't work recursively, so we can't
        // just monitor the entire Skins/ and Songs/ directories.

        struct DirectoryState {
            std::string path;
            FileChangeCallback cb;
            std::unordered_map<std::string, FileChangeEvent> unconfirmed_events;
            std::unordered_map<std::string, fs::file_time_type> files;
        };

        std::unordered_map<std::string, DirectoryState> active_directories;

        while(!stoken.stop_requested()) {
            // Add/remove directories
            std::vector<std::string> directories_to_init;
            {
                Sync::scoped_lock lock(this->directories_mtx);
                for(const auto& [path, cb] : this->directories_to_add) {
                    // Don't add if it's going to be removed
                    if(std::ranges::contains(this->directories_to_remove, path)) continue;

                    directories_to_init.push_back(path);
                    active_directories[path] =
                        DirectoryState{.path = path, .cb = cb, .unconfirmed_events = {}, .files = {}};
                }
                this->directories_to_add.clear();

                for(const auto& path : this->directories_to_remove) {
                    if(active_directories.contains(path)) active_directories.erase(path);
                }
                this->directories_to_remove.clear();
            }
            {
                // Initialize state now (avoiding lock)
                for(const auto& path : directories_to_init) {
                    active_directories[path].files = getFileTimes(path);
                }
                directories_to_init.clear();
            }

            // Check for changes
            for(auto& [path, dir] : active_directories) {
                auto latest_files = getFileTimes(path);

                // Deletions
                for(auto& [file, tms] : dir.files) {
                    if(!latest_files.contains(file)) {
                        Sync::scoped_lock lock(this->finished_events_mtx);
                        this->finished_events.emplace_back(
                            dir.cb, FileChangeEvent{.path = file, .type = FileChangeType::DELETED, .tms = tms});
                        continue;
                    }
                }

                for(auto& [file, tms] : latest_files) {
                    // Creations
                    if(!dir.files.contains(file)) {
                        dir.unconfirmed_events[file] = FileChangeEvent{
                            .path = file,
                            .type = FileChangeType::CREATED,
                            .tms = tms,
                        };
                        continue;
                    }

                    // Modifications
                    if(dir.files[file] != tms) {
                        dir.unconfirmed_events[file] = FileChangeEvent{
                            .path = file,
                            .type = FileChangeType::MODIFIED,
                            .tms = tms,
                        };
                        continue;
                    }

                    // Finalization (no new writes)
                    if(dir.files[file] == tms) {
                        if(dir.unconfirmed_events.contains(file)) {
                            Sync::scoped_lock lock(this->finished_events_mtx);
                            this->finished_events.emplace_back(dir.cb, dir.unconfirmed_events[file]);
                            dir.unconfirmed_events.erase(file);
                        }
                        continue;
                    }
                }

                dir.files = latest_files;
            }

            // Update finished event count
            this->finished_events_count.store(this->finished_events.size(), std::memory_order_release);

            Timing::sleepMS(100);
        }
    }
};

DirectoryWatcher::DirectoryWatcher() : pImpl(std::make_unique<WatcherImpl>()) {}
DirectoryWatcher::~DirectoryWatcher() = default;

void DirectoryWatcher::watch_directory(std::string path, FileChangeCallback cb) {
    return pImpl->watch_directory(std::move(path), std::move(cb));
}

void DirectoryWatcher::stop_watching(std::string path) { return pImpl->stop_watching(std::move(path)); }

void DirectoryWatcher::update() { return pImpl->update(); }
