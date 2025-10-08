// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "noinclude.h"
#include "types.h"

#include <functional>
#include <string_view>
#include <string>
#include <vector>
#include <memory>

class AsyncIOHandler final {
    NOCOPY_NOMOVE(AsyncIOHandler)
   public:
    AsyncIOHandler();
    ~AsyncIOHandler();

    // read entire file asynchronously
    // callback receives data vector (empty on failure)
    // returns false if file already has a pending operation
    using ReadCallback = std::function<void(std::vector<u8>)>;
    bool read(std::string_view path, ReadCallback callback);

    // write data to file asynchronously
    // optional callback receives success status after write completes
    // returns false if file already has a pending operation
    using WriteCallback = std::function<void(bool)>;
    bool write(std::string_view path, std::vector<u8> data, WriteCallback callback = nullptr);
    bool write(std::string_view path, std::string data, WriteCallback callback = nullptr);
    bool write(std::string_view path, const u8 *data, size_t amount, WriteCallback callback = nullptr);

   private:
    friend class Engine;  // only to be used by engine

    // returns true if initialization succeeded
    [[nodiscard]] bool succeeded() const;

    // clean up all i/o before shutdown
    void cleanup();

    // must be called regularly (e.g., once per frame) to process completed I/O tasks
    void update();

   private:
    class InternalIOContext;
    std::unique_ptr<InternalIOContext> m_io;
};

extern std::unique_ptr<AsyncIOHandler> io;
