// Copyright (c) 2025 kiwec, All rights reserved.
#include "DirectoryWatcher.h"

#include "Logging.h"
#include "Thread.h"
#include "Timing.h"
#include "SyncJthread.h"
#include "SyncMutex.h"
#include "UString.h"

#include <algorithm>
#include <utility>
#include <vector>
#include <atomic>

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <windows.h>
#endif

namespace fs = std::filesystem;

// The Windows implementation does not poll, only wakes up when files in folders on disk have changed
struct DirWatcherImpl {
   private:
    NOCOPY_NOMOVE(DirWatcherImpl)
   public:
    DirWatcherImpl() {
        this->init_wakeup_notification();
        this->thr = Sync::jthread([this](const Sync::stop_token& stoken) { return this->worker_loop(stoken); });
    }
    ~DirWatcherImpl() { this->destroy_wakeup_notification(); }

    void watch_directory(std::string path, FileChangeCallback cb) {
        Sync::scoped_lock lock(this->directories_mtx);
        this->directories_to_add.emplace_back(std::move(path), std::move(cb));
        this->notify_thread();
    }

    void stop_watching(std::string path) {
        Sync::scoped_lock lock(this->directories_mtx);
        this->directories_to_remove.push_back(std::move(path));
        this->notify_thread();
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
    struct UnconfirmedEvent {
        UnconfirmedEvent() = default;
        UnconfirmedEvent(FileChangeEvent fevent) : event(std::move(fevent)) {}
        FileChangeEvent event{};
        u8 stable_checks{0};  // consecutive checks where timestamp was stable
    };

    Sync::mutex directories_mtx;
    std::vector<std::pair<std::string, FileChangeCallback>> directories_to_add;
    std::vector<std::string> directories_to_remove;

    Sync::mutex finished_events_mtx;
    std::vector<std::pair<FileChangeCallback, FileChangeEvent>> finished_events;
    std::atomic<uSz> finished_events_count{0};

    Sync::jthread thr;

#ifdef MCENGINE_PLATFORM_WINDOWS
   private:
    HANDLE wakeup_event{INVALID_HANDLE_VALUE};

    void init_wakeup_notification() {
        this->wakeup_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        assert(wakeup_event != INVALID_HANDLE_VALUE);
    }
    void destroy_wakeup_notification() { CloseHandle(this->wakeup_event); }
    void notify_thread() { SetEvent(this->wakeup_event); }

    struct DirectoryState {
        DirectoryState(FileChangeCallback cb) : cb(std::move(cb)) {}

        FileChangeCallback cb;

        std::unordered_map<std::string, UnconfirmedEvent> unconfirmed_events{};

        struct WinDirState {
            WinDirState() = default;
            ~WinDirState() {
                if(this->dir_handle != INVALID_HANDLE_VALUE) {
                    CancelIo(this->dir_handle);
                    CloseHandle(this->dir_handle);
                }
                if(this->overlapped.hEvent) CloseHandle(this->overlapped.hEvent);
            }

            WinDirState(const WinDirState&) = delete;

            WinDirState(WinDirState&& other) noexcept : dir_handle(other.dir_handle), overlapped(other.overlapped) {
                other.dir_handle = INVALID_HANDLE_VALUE;
                other.overlapped = {};
            }

            WinDirState& operator=(const WinDirState&) = delete;

            WinDirState& operator=(WinDirState&& other) noexcept {
                if(this != &other) {
                    if(this->dir_handle != INVALID_HANDLE_VALUE) {
                        CancelIo(this->dir_handle);
                        CloseHandle(this->dir_handle);
                    }
                    this->dir_handle = other.dir_handle;
                    other.dir_handle = INVALID_HANDLE_VALUE;

                    if(this->overlapped.hEvent) CloseHandle(this->overlapped.hEvent);
                    this->overlapped = other.overlapped;
                    other.overlapped = {};
                }
                return *this;
            }

            HANDLE dir_handle{INVALID_HANDLE_VALUE};
            OVERLAPPED overlapped{};
        } w;

        alignas(DWORD) std::array<u8, 4096> buffer{};
        bool read_pending{false};
    };

    void worker_loop(const Sync::stop_token& stoken) {
        McThread::set_current_thread_name(ULITERAL("dir_watcher"));
        McThread::set_current_thread_prio(McThread::Priority::LOW);

        std::unordered_map<std::string, DirectoryState> active_directories;

        // Create manual-reset event for stop signaling
        HANDLE stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if(!stop_event) {
            debugLog("DirectoryWatcher: failed to create stop event");
            return;
        }

        // Signal stop event when stop is requested
        Sync::stop_callback stop_cb(stoken, [stop_event]() { SetEvent(stop_event); });

        while(!stoken.stop_requested()) {
            // Add/remove directories
            std::vector<std::unordered_map<std::string, DirectoryState>::iterator> directories_to_init;
            {
                Sync::scoped_lock lock(this->directories_mtx);
                for(auto& [path, cb] : this->directories_to_add) {
                    if(std::ranges::contains(this->directories_to_remove, path)) continue;
                    if(!path.ends_with('/')) path.push_back('/');  // make sure it ends with a /

                    auto [it, added] = active_directories.emplace(path, DirectoryState(cb));
                    if(added) {
                        // This should always be true
                        directories_to_init.push_back(it);
                    }
                }
                this->directories_to_add.clear();

                for(auto& path : this->directories_to_remove) {
                    if(!path.ends_with('/')) path.push_back('/');

                    if(active_directories.contains(path)) active_directories.erase(path);
                }
                this->directories_to_remove.clear();
            }

            // Initialize new directories
            for(const auto& it : directories_to_init) {
                auto& state = it->second;
                const auto& path = it->first;
                const UString uni_path{path};

                state.w.dir_handle = CreateFileW(
                    uni_path.wchar_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

                if(state.w.dir_handle == INVALID_HANDLE_VALUE) {
                    debugLog("DirectoryWatcher: failed to open directory: {}", path);
                    continue;
                }

                state.w.overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if(!state.w.overlapped.hEvent) {
                    debugLog("DirectoryWatcher: failed to create event for directory: {}", path);
                    CloseHandle(state.w.dir_handle);
                    state.w.dir_handle = INVALID_HANDLE_VALUE;
                    continue;
                }
            }
            directories_to_init.clear();

            // Issue ReadDirectoryChangesW calls for directories that don't have one pending
            for(auto& [path, state] : active_directories) {
                if(state.w.dir_handle == INVALID_HANDLE_VALUE) continue;
                if(state.read_pending) continue;

                ResetEvent(state.w.overlapped.hEvent);
                BOOL result = ReadDirectoryChangesW(
                    state.w.dir_handle, state.buffer.data(), static_cast<DWORD>(state.buffer.size()), FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE, nullptr,
                    &state.w.overlapped, nullptr);

                if(result || GetLastError() == ERROR_IO_PENDING) {
                    state.read_pending = true;
                } else {
                    debugLog("DirectoryWatcher: ReadDirectoryChangesW failed for {}: error {}", path, GetLastError());
                }
            }

            // Collect event handles to wait on
            std::vector<HANDLE> wait_handles;

            // Always include stop event and wakeup event
            wait_handles.push_back(stop_event);
            wait_handles.push_back(this->wakeup_event);

            for(auto& [path, state] : active_directories) {
                if(state.read_pending && state.w.overlapped.hEvent) {
                    wait_handles.push_back(state.w.overlapped.hEvent);
                }
            }

            // Check if we have any unconfirmed events that need debouncing
            bool has_unconfirmed = false;
            for(const auto& [path, state] : active_directories) {
                if(!state.unconfirmed_events.empty()) {
                    has_unconfirmed = true;
                    break;
                }
            }

            // Wait for events: use timeout only when we have unconfirmed events to debounce
            DWORD wait_timeout = has_unconfirmed ? 100 : INFINITE;
            DWORD wait_result = WaitForMultipleObjects(static_cast<DWORD>(wait_handles.size()), wait_handles.data(),
                                                       FALSE, wait_timeout);

            // Check if stop was signaled
            if(wait_result == WAIT_OBJECT_0) {
                break;
            }

            // wakeup_event (for directory add/remove) is at index 1, but it's auto-reset so we don't have to do anything

            // Process completed notifications
            for(auto& [path, state] : active_directories) {
                if(!state.read_pending) continue;

                DWORD bytes_transferred = 0;
                BOOL completed =
                    GetOverlappedResult(state.w.dir_handle, &state.w.overlapped, &bytes_transferred, FALSE);

                if(!completed) {
                    if(GetLastError() != ERROR_IO_INCOMPLETE) {
                        debugLog("DirectoryWatcher: GetOverlappedResult failed for {}: error {}", path, GetLastError());
                        state.read_pending = false;
                    }
                    // Still pending, will check next iteration
                    continue;
                }

                state.read_pending = false;

                if(bytes_transferred == 0) continue;

                // Process notifications
                uSz offset = 0;
                FILE_NOTIFY_INFORMATION* notify = nullptr;
                do {
                    notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(state.buffer.data() + offset);

                    UString uni_filename{&notify->FileName[0],
                                         static_cast<int>(notify->FileNameLength / sizeof(WCHAR))};

                    // The directory is guaranteed to end with a '/', since we made sure when we added it.
                    UString uni_filepath{fmt::format("{}{}", path, uni_filename)};
                    std::string std_filepath{uni_filepath.utf8View()};

                    std::error_code ec;
                    auto file_status = fs::status(uni_filepath.wstringView(), ec);

                    // Handle deletions immediately
                    if(ec || file_status.type() != fs::file_type::regular) {
                        if(notify->Action == FILE_ACTION_REMOVED || notify->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                            Sync::scoped_lock lock(this->finished_events_mtx);
                            this->finished_events.emplace_back(state.cb,
                                                               FileChangeEvent{.path = std_filepath,
                                                                               .type = FileChangeType::DELETED,
                                                                               .tms = fs::file_time_type{}});
                            state.unconfirmed_events.erase(std_filepath);
                        }
                    } else {
                        // For creations and modifications, add to unconfirmed (needs debouncing)
                        auto file_time = fs::last_write_time(uni_filepath.wstringView(), ec);
                        if(!ec) {
                            FileChangeType change_type = FileChangeType::MODIFIED;
                            if(notify->Action == FILE_ACTION_ADDED || notify->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                                change_type = FileChangeType::CREATED;
                            }

                            auto it = state.unconfirmed_events.find(std_filepath);
                            if(it != state.unconfirmed_events.end()) {
                                auto& existing = it->second;
                                // File was already an unconfirmed event, so update timestamp and reset stability counter
                                // but preserve CREATED type if that's what we saw first
                                if(existing.event.type != FileChangeType::CREATED) {
                                    existing.event.type = change_type;
                                }
                                existing.event.tms = file_time;
                                existing.stable_checks = 0;
                            } else {
                                // New unconfirmed event
                                state.unconfirmed_events[std_filepath] =
                                    UnconfirmedEvent({std_filepath, change_type, file_time});
                            }
                        }
                    }

                    offset += notify->NextEntryOffset;
                } while(notify->NextEntryOffset != 0);
            }

            // Check unconfirmed events for stability (debouncing)
            // Require at least 2 consecutive stable checks (200ms) before confirming
            for(auto& [path, state] : active_directories) {
                std::vector<std::string> to_confirm;

                for(auto& [file, unconfirmed] : state.unconfirmed_events) {
                    UString uni_path{file};
                    std::error_code ec;
                    auto current_time = fs::last_write_time(uni_path.wstringView(), ec);

                    if(ec) {
                        // File was deleted or became inaccessible
                        to_confirm.push_back(file);
                        continue;
                    }

                    // Check if timestamp is stable
                    if(current_time == unconfirmed.event.tms) {
                        unconfirmed.stable_checks++;

                        // Only confirm after 2+ consecutive stable checks
                        if(unconfirmed.stable_checks >= 2) {
                            to_confirm.push_back(file);
                            Sync::scoped_lock lock(this->finished_events_mtx);
                            this->finished_events.emplace_back(state.cb, unconfirmed.event);
                        }
                    } else {
                        // Timestamp changed, reset counter and update
                        unconfirmed.event.tms = current_time;
                        unconfirmed.stable_checks = 0;
                    }
                }

                for(const auto& file : to_confirm) {
                    state.unconfirmed_events.erase(file);
                }
            }

            this->finished_events_count.store(this->finished_events.size(), std::memory_order_release);
        }

        CloseHandle(stop_event);
    }
#else
   private:
    // Generic implementation (polling)

    // These currently don't do anything
    void init_wakeup_notification() {}
    void destroy_wakeup_notification() {}
    void notify_thread() {}

    struct DirectoryState {
        DirectoryState(FileChangeCallback cb) : cb(std::move(cb)) {}
        FileChangeCallback cb;

        std::unordered_map<std::string, UnconfirmedEvent> unconfirmed_events{};
        std::unordered_map<std::string, fs::file_time_type> files{};
    };

    void worker_loop(const Sync::stop_token& stoken) {
        McThread::set_current_thread_name(ULITERAL("dir_watcher"));
        McThread::set_current_thread_prio(McThread::Priority::LOW);

        // Windows & OSX do not provide APIs that tell you when a file is
        // *done* being written to; and thus, they require debouncing, i.e.
        // waiting 100ms to make sure it's no longer getting writes.

        // To keep things "simple" for now, we'll just do the simplest method
        // that works on all platforms: manually checking for changes.

        // The downside is that this doesn't work recursively, so we can't
        // just monitor the entire Skins/ and Songs/ directories.

        static auto getFileTimes =
            [](const std::string& dir_path) -> std::unordered_map<std::string, fs::file_time_type> {
            std::unordered_map<std::string, fs::file_time_type> files;

            std::error_code ec;
            for(const auto& entry : fs::directory_iterator(dir_path, ec)) {
                if(ec) continue;
                auto fileType = entry.status(ec).type();
                // we only care about files, not directories right now
                if(fileType != fs::file_type::regular) continue;

                auto time = fs::last_write_time(entry, ec);
                if(ec) continue;
                files[entry.path().string()] = time;
            }

            return files;
        };

        std::unordered_map<std::string, DirectoryState> active_directories;

        while(!stoken.stop_requested()) {
            // Add/remove directories
            std::vector<std::unordered_map<std::string, DirectoryState>::iterator> directories_to_init;
            {
                Sync::scoped_lock lock(this->directories_mtx);
                for(auto& [path, cb] : this->directories_to_add) {
                    // Don't add if it's going to be removed
                    if(std::ranges::contains(this->directories_to_remove, path)) continue;
                    if(!path.ends_with('/')) path.push_back('/');

                    auto [it, added] = active_directories.emplace(path, DirectoryState(cb));
                    if(added) {
                        // This should always be true
                        directories_to_init.push_back(it);
                    }
                }
                this->directories_to_add.clear();

                for(auto& path : this->directories_to_remove) {
                    if(!path.ends_with('/')) path.push_back('/');

                    if(active_directories.contains(path)) active_directories.erase(path);
                }
                this->directories_to_remove.clear();
            }
            {
                // Initialize state now (avoiding lock)
                for(const auto& it : directories_to_init) {
                    auto& path = it->first;
                    auto& state = it->second;
                    state.files = getFileTimes(path);
                }
                directories_to_init.clear();
            }

            // Check for changes
            for(auto& [path, state] : active_directories) {
                auto latest_files = getFileTimes(path);

                // Deletions
                for(auto& [file, tms] : state.files) {
                    if(!latest_files.contains(file)) {
                        Sync::scoped_lock lock(this->finished_events_mtx);
                        this->finished_events.emplace_back(
                            state.cb, FileChangeEvent{.path = file, .type = FileChangeType::DELETED, .tms = tms});
                        continue;
                    }
                }

                for(auto& [file, tms] : latest_files) {
                    // Creations
                    if(!state.files.contains(file)) {
                        state.unconfirmed_events[file] = UnconfirmedEvent({file, FileChangeType::CREATED, tms});

                        continue;
                    }

                    // Modifications
                    if(state.files[file] != tms) {
                        auto it = state.unconfirmed_events.find(file);
                        if(it != state.unconfirmed_events.end()) {
                            auto& existing = it->second;
                            // Update timestamp and reset stability, but preserve CREATED type
                            if(existing.event.type != FileChangeType::CREATED) {
                                existing.event.type = FileChangeType::MODIFIED;
                            }
                            existing.event.tms = tms;
                            existing.stable_checks = 0;
                        } else {
                            state.unconfirmed_events[file] = UnconfirmedEvent({file, FileChangeType::MODIFIED, tms});
                        }
                        continue;
                    }

                    // Finalization check (timestamp stable)
                    if(state.files[file] == tms) {
                        auto it = state.unconfirmed_events.find(file);
                        if(it != state.unconfirmed_events.end()) {
                            auto& existing = it->second;
                            existing.stable_checks++;

                            // Only confirm after 2+ consecutive stable checks
                            if(existing.stable_checks >= 2) {
                                Sync::scoped_lock lock(this->finished_events_mtx);
                                this->finished_events.emplace_back(state.cb, existing.event);
                                state.unconfirmed_events.erase(file);
                            }
                        }
                        continue;
                    }
                }

                state.files = latest_files;
            }

            this->finished_events_count.store(this->finished_events.size(), std::memory_order_release);

            Timing::sleepMS(100);
        }
    }
#endif
};

DirectoryWatcher::DirectoryWatcher() : implementation() {}

DirectoryWatcher::~DirectoryWatcher() = default;

void DirectoryWatcher::watch_directory(std::string path, FileChangeCallback cb) {
    return implementation->watch_directory(std::move(path), std::move(cb));
}

void DirectoryWatcher::stop_watching(std::string path) { return implementation->stop_watching(std::move(path)); }

void DirectoryWatcher::update() { return implementation->update(); }
