#pragma once
// Copyright (c) 2025 kiwec, All rights reserved.

#include "noinclude.h"
#include "types.h"

#include <filesystem>
#include <functional>

enum class FileChangeType : u8 {
    CREATED,
    MODIFIED,
    DELETED,
};

struct FileChangeEvent {
    std::string path;
    FileChangeType type;
    std::filesystem::file_time_type tms;
};

// Consider this API "temporary" until a better solution is implemented
// XXX: can't have multiple callbacks per path

using FileChangeCallback = std::function<void(FileChangeEvent)>;

class DirectoryWatcher {
    NOCOPY_NOMOVE(DirectoryWatcher);

   public:
    DirectoryWatcher();
    ~DirectoryWatcher();

    void watch_directory(std::string path, FileChangeCallback cb);
    void stop_watching(std::string path);

   private:
    friend class Engine;

    // Similar to other neosu async APIs, let us control when callbacks are fired
    // to avoid race condition issues.
    void update();

    class WatcherImpl;
    std::unique_ptr<WatcherImpl> pImpl;
};

extern std::unique_ptr<DirectoryWatcher> directoryWatcher;
