// Copyright (c) 2025 kiwec, All rights reserved.
#include "DirectoryWatcher.h"

#include <vector>

#include "Thread.h"
#include "Timing.h"
#include "SyncJthread.h"
#include "SyncMutex.h"

namespace fs = std::filesystem;

class DirectoryWatcher::WatcherImpl {
    NOCOPY_NOMOVE(WatcherImpl)
   public:
    WatcherImpl() : thr(directory_watcher_thread, this) {}
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
        Sync::scoped_lock lock(this->finished_events_mtx);
        for(const auto& [cb, event] : this->finished_events) {
            cb(event);
        }
        this->finished_events.clear();
    }

   private:
    Sync::mutex directories_mtx;
    std::vector<std::pair<std::string, FileChangeCallback>> directories_to_add;
    std::vector<std::string> directories_to_remove;

    Sync::mutex finished_events_mtx;
    std::vector<std::pair<FileChangeCallback, FileChangeEvent>> finished_events;

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

    static void directory_watcher_thread(const Sync::stop_token& stoken, WatcherImpl* watcher) {
        McThread::set_current_thread_name("directory_watcher");
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

        std::unordered_map<std::string, DirectoryState> directories;

        while(!stoken.stop_requested()) {
            // Add/remove directories
            std::vector<std::string> directories_to_init;
            {
                Sync::scoped_lock lock(watcher->directories_mtx);
                for(const auto& [path, cb] : watcher->directories_to_add) {
                    directories_to_init.push_back(path);
                    directories[path] = DirectoryState{.path = path, .cb = cb, .unconfirmed_events = {}, .files = {}};
                }
                watcher->directories_to_add.clear();

                for(const auto& path : watcher->directories_to_remove) {
                    if(directories.contains(path)) directories.erase(path);
                }
                watcher->directories_to_remove.clear();
            }
            {
                // Initialize state now (avoiding lock)
                for(const auto& path : directories_to_init) {
                    directories[path].files = getFileTimes(path);
                }
                directories_to_init.clear();
            }

            // Check for changes
            for(auto& [path, dir] : directories) {
                auto latest_files = getFileTimes(path);

                // Deletions
                for(auto& [file, tms] : dir.files) {
                    if(!latest_files.contains(file)) {
                        Sync::scoped_lock lock(watcher->finished_events_mtx);
                        watcher->finished_events.emplace_back(
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
                            Sync::scoped_lock lock(watcher->finished_events_mtx);
                            watcher->finished_events.emplace_back(dir.cb, dir.unconfirmed_events[file]);
                            dir.unconfirmed_events.erase(file);
                        }
                        continue;
                    }
                }

                dir.files = latest_files;
            }

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
