#pragma once
// Copyright (c) 2025 kiwec, All rights reserved.
#include <filesystem>
#include <functional>

#include "types.h"

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

void watch_directory(std::string path, FileChangeCallback cb);
void stop_watching_directory(std::string path);

// Similar to other neosu async APIs, let us control when callbacks are fired
// to avoid race condition issues.
void collect_directory_events();
void stop_directory_watcher();
