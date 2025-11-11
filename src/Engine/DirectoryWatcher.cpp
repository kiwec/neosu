// Copyright (c) 2025 kiwec, All rights reserved.
#include "DirectoryWatcher.h"

#include <vector>

#include "Thread.h"
#include "Timing.h"
#include "SyncMutex.h"

namespace fs = std::filesystem;

static std::atomic<bool> dead = false;

static Sync::mutex directories_mtx;
static std::vector<std::pair<std::string, FileChangeCallback>> directories_to_add;
static std::vector<std::string> directories_to_remove;

static Sync::mutex finished_events_mtx;
static std::vector<std::pair<FileChangeCallback, FileChangeEvent>> finished_events;

static std::unordered_map<std::string, fs::file_time_type> getFileTimes(std::string dir_path) {
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

void directory_watcher_thread() {
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

    // TODO: terminate thread with neosu
    while(!dead.load(std::memory_order_acquire)) {
        // Add/remove directories
        std::vector<std::string> directories_to_init;
        {
            Sync::scoped_lock lock(directories_mtx);
            for(auto [path, cb] : directories_to_add) {
                directories_to_init.push_back(path);
                directories[path] = DirectoryState{
                    .path = path,
                    .cb = cb,
                };
            }
            directories_to_add.clear();

            for(auto path : directories_to_remove) {
                if(directories.contains(path)) directories.erase(path);
            }
            directories_to_remove.clear();
        }
        {
            // Initialize state now (avoiding lock)
            for(auto path : directories_to_init) {
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
                    Sync::scoped_lock lock(finished_events_mtx);
                    finished_events.push_back({dir.cb, FileChangeEvent{
                                                           .path = file,
                                                           .type = FileChangeType::DELETED,
                                                       }});
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
                        Sync::scoped_lock lock(finished_events_mtx);
                        finished_events.push_back({dir.cb, dir.unconfirmed_events[file]});
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

void watch_directory(std::string path, FileChangeCallback cb) {
    Sync::scoped_lock lock(directories_mtx);
    directories_to_add.push_back({path, cb});
}

void stop_watching_directory(std::string path) {
    Sync::scoped_lock lock(directories_mtx);
    directories_to_remove.push_back(path);
}

void collect_directory_events() {
    Sync::scoped_lock lock(finished_events_mtx);
    for(auto [cb, event] : finished_events) {
        cb(event);
    }
    finished_events.clear();
}

static auto thr = std::thread(directory_watcher_thread);

void stop_directory_watcher() {
    dead.store(true, std::memory_order_release);
    thr.join();
}