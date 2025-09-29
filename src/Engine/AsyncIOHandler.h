// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "noinclude.h"
#include "types.h"

#include <functional>
#include <string_view>
#include <vector>
#include <memory>

class AsyncIOHandler final {
    NOCOPY_NOMOVE(AsyncIOHandler)
   public:
    AsyncIOHandler();
    ~AsyncIOHandler();

    // returns true if initialization succeeded
    [[nodiscard]] bool succeeded() const;

    // must be called regularly (e.g., once per frame) to process completed I/O tasks
    void update();

    // read entire file asynchronously
    // callback receives data vector (empty on failure)
    // returns false if file already has a pending operation
    bool read(std::string_view path, std::function<void(std::vector<u8>)> callback);

    // write data to file asynchronously
    // optional callback receives success status after write completes
    // returns false if file already has a pending operation
    bool write(std::string_view path, std::vector<u8> data, std::function<void(bool)> callback = nullptr);
    bool write(std::string_view path, std::string data, std::function<void(bool)> callback = nullptr);
    bool write(std::string_view path, const u8 *data, size_t amount, std::function<void(bool)> callback = nullptr);

   private:
    class InternalIOContext;
    std::unique_ptr<InternalIOContext> m_io;
};
