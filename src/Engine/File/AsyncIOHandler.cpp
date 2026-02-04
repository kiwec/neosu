// Copyright (c) 2025, WH, All rights reserved.
#include "AsyncIOHandler.h"
#include "ConVar.h"
#include "Logging.h"
#include "Timing.h"
#include "File.h"

#include <cstring>
#include <string>

#ifdef MCENGINE_PLATFORM_WASM
// WASM: synchronous I/O shim (SDL_AsyncIO not supported)
#include <cstdio>

class AsyncIOHandler::InternalIOContext final {
    NOCOPY_NOMOVE(InternalIOContext)
   public:
    InternalIOContext() = default;
    ~InternalIOContext() = default;

    void cleanup() {}

    bool read(std::string_view path, ReadCallback callback) {
        std::string pathStr(path);
        FILE* f = fopen(pathStr.c_str(), "rb");
        if(!f) {
            logIfCV(debug_file, "WARNING: failed to open {} for reading", pathStr);
            if(callback) callback({});
            return false;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if(size <= 0 || size > (1L * 1024 * 1024 * 1024)) {
            if(size == 0) {
                logIfCV(debug_file, "WARNING: {} has size 0!", pathStr);
            } else if(size > 0) {
                debugLog("ERROR: {} is over 1GB in size!", pathStr);
            }
            fclose(f);
            if(callback) callback({});
            return false;
        }

        std::vector<u8> data(size);
        size_t bytesRead = fread(data.data(), 1, size, f);
        fclose(f);

        if(bytesRead != static_cast<size_t>(size)) {
            debugLog("ERROR: only read {}/{} bytes from {}", bytesRead, size, pathStr);
            data.resize(bytesRead);
        }

        if(callback) callback(std::move(data));
        return true;
    }

    bool write(std::string_view path, std::vector<u8> data, WriteCallback callback) {
        std::string pathStr(path);
        FILE* f = fopen(pathStr.c_str(), "wb");
        if(!f) {
            debugLog("ERROR: failed to open {} for writing", pathStr);
            if(callback) callback(false);
            return false;
        }

        size_t written = fwrite(data.data(), 1, data.size(), f);
        fclose(f);

        bool success = (written == data.size());
        if(!success) {
            debugLog("ERROR: only wrote {}/{} bytes to {}", written, data.size(), pathStr);
        }

        if(callback) callback(success);
        return true;
    }
};

#else
// native platforms: SDL_AsyncIO
#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL_error.h>

#include <atomic>
#include <unordered_set>

class AsyncIOHandler::InternalIOContext final {
    NOCOPY_NOMOVE(InternalIOContext)
   public:
    InternalIOContext() : m_queue(SDL_CreateAsyncIOQueue()) {
        if(!m_queue) {
            debugLog("failed to create async I/O queue: {}", SDL_GetError());
        }
    }

    ~InternalIOContext() { cleanup(); }

    // convoluted mechanism needed for handling nested callbacks which might refer to the global "io"
    void cleanup() {
        if(m_queue) {
            const auto startTime = Timing::getTicksMS();
            bool sdlIOResult = false;

            while(((Timing::getTicksMS() - startTime) < 10000) &&
                  (sdlIOResult == true || m_activeCallbacks.load(std::memory_order_acquire) > 0 ||
                   m_activeFiles.size() > 0)) {
                SDL_AsyncIOOutcome outcome{};
                if((sdlIOResult = SDL_WaitAsyncIOResult(m_queue, &outcome, 50))) {
                    // take care of any pending async operations (callbacks etc.)
                    drainIoResults(outcome);
                }
            }

            logIfCV(debug_file,
                    "destroying async I/O queue, sdlIOResult: {} activeFiles.size(): {} activeCallbacks: {}",
                    sdlIOResult, m_activeFiles.size(), m_activeCallbacks.load(std::memory_order_acquire));

            SDL_DestroyAsyncIOQueue(m_queue);
        }
        m_queue = nullptr;
    }

    struct OperationContext {
        OperationContext() = delete;
        OperationContext(std::string path) : path(std::move(path)) {}
        std::string path;
        SDL_AsyncIO* handle{nullptr};  // contains the handle that made the request, NULL for close completion

        std::vector<u8> operationBuffer;

        ReadCallback readCallback;
        WriteCallback writeCallback;

        // for reads:
        // if partial, the returned buffer will be smaller than the actual file contents
        // if complete, the buffer will contain the full contents

        // for writes: only pending/complete are valid states, partial counts as a fail
        // only useful for knowing whether read/write was a success after closing and calling callbacks
        // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
        enum OpStatus : u8 { OP_FAIL, OP_PARTIAL, OP_COMPLETE };

        OpStatus status{OP_FAIL};
    };

    void drainIoResults(const SDL_AsyncIOOutcome& outcome) {
        auto* context = static_cast<OperationContext*>(outcome.userdata);
        auto type = outcome.type;

        switch(type) {
            case SDL_ASYNCIO_TASK_READ:
                handleReadComplete(outcome, context);
                break;
            case SDL_ASYNCIO_TASK_WRITE:
                handleWriteComplete(outcome, context);
                break;
            case SDL_ASYNCIO_TASK_CLOSE:
                handleCloseComplete(outcome, context);
                break;
        }
    }

    void update() {
        assert(!!m_queue);

        SDL_AsyncIOOutcome outcome;
        while(SDL_GetAsyncIOResult(m_queue, &outcome)) {
            drainIoResults(outcome);
        }
    }

#define PERFORM_CALLBACK(cb__)                                 \
    m_activeCallbacks.fetch_add(1, std::memory_order_relaxed); \
    cb__;                                                      \
    m_activeCallbacks.fetch_sub(1, std::memory_order_acq_rel);

    bool read(std::string_view path, ReadCallback callback) {
        assert(!!m_queue);

        std::string pathStr(path);
        if(m_activeFiles.contains(pathStr)) {
            // TODO: multiple actions on the same file at the same time
            logIfCV(debug_file, "WARNING: cannot read from {}, file is in use", path);
            if(callback) {
                PERFORM_CALLBACK(callback({}));
            }
            return false;
        }

        SDL_AsyncIO* handle = SDL_AsyncIOFromFile(pathStr.c_str(), "r");
        if(!handle) {
            // if it doesn't exist, it's not that big of a deal, an error is expected
            if(File::exists(pathStr) == File::FILETYPE::FILE) {
                debugLog("ERROR: failed to open {} for reading: {}", pathStr, SDL_GetError());
            } else if(cv::debug_file.getBool()) {
                debugLog("WARNING: failed to open {} for reading: {}", pathStr, SDL_GetError());
            }

            if(callback) {
                PERFORM_CALLBACK(callback({}));
            }
            return false;
        }

        i64 readSize = SDL_GetAsyncIOSize(handle);
        if(readSize <= 0 || readSize > (2ULL * 1024 * 1024 * 1024)) {
            if(readSize < 0) {
                debugLog("ERROR: failed to open {} for reading: {}", pathStr, SDL_GetError());
            } else if(readSize == 0) {
                logIfCV(debug_file, "WARNING: {} has size 0!", pathStr);
            } else {
                // arbitrary size limit sanity check
                debugLog("ERROR: failed to open {} for reading, over 2GB in size!", pathStr);
            }
            if(callback) {
                PERFORM_CALLBACK(callback({}));
            }

            // close it but don't add a context/check for errors
            SDL_CloseAsyncIO(handle, false, m_queue, nullptr);

            return false;
        }

        auto* context = new OperationContext(pathStr);
        context->handle = handle;
        context->operationBuffer = std::vector<u8>(readSize);
        context->readCallback = std::move(callback);

        if(!SDL_ReadAsyncIO(handle, context->operationBuffer.data(), 0, readSize, m_queue, context)) {
            debugLog("ERROR: SDL_ReadAsyncIO failed for {}: {}", pathStr, SDL_GetError());
            if(context->readCallback) {
                PERFORM_CALLBACK(context->readCallback({}));
            }
            SDL_CloseAsyncIO(handle, false, m_queue, nullptr);

            delete context;
            return false;
        }

        m_activeFiles.insert(pathStr);

        return true;
    }

    bool write(std::string_view path, std::vector<u8> data, WriteCallback callback) {
        assert(!!m_queue);

        std::string pathStr(path);
        if(m_activeFiles.contains(pathStr)) {
            // TODO: multiple actions on the same file at the same time
            logIfCV(debug_file, "WARNING: cannot write to {}, file is in use", path);
            if(callback) {
                PERFORM_CALLBACK(callback(false));
            }
            return false;
        }

        SDL_AsyncIO* handle = SDL_AsyncIOFromFile(pathStr.c_str(), "w");
        if(!handle) {
            debugLog("ERROR: failed to open {} for writing: {}", pathStr, SDL_GetError());
            if(callback) {
                PERFORM_CALLBACK(callback(false));
            }
            return false;
        }

        auto* context = new OperationContext(pathStr);
        context->handle = handle;
        context->operationBuffer = std::move(data);
        context->writeCallback = std::move(callback);

        if(!SDL_WriteAsyncIO(handle, context->operationBuffer.data(), 0, context->operationBuffer.size(), m_queue,
                             context)) {
            debugLog("ERROR: SDL_WriteAsyncIO failed for {}: {}", pathStr, SDL_GetError());
            if(context->writeCallback) {
                PERFORM_CALLBACK(context->writeCallback(false));
            }
            SDL_CloseAsyncIO(handle, false, m_queue, nullptr);

            delete context;
            return false;
        }

        m_activeFiles.insert(pathStr);

        return true;
    }

    void handleReadComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);
        if(!context) return;  // nothing to do

        OperationContext::OpStatus status = OperationContext::OP_COMPLETE;

        // always resize to how much we actually got
        context->operationBuffer.resize(outcome.bytes_transferred);

        if(outcome.result == SDL_ASYNCIO_COMPLETE) {
            assert(outcome.bytes_requested == outcome.bytes_transferred &&
                   "AsyncIOHandler::handleReadComplete(SDL_ASYNCIO_COMPLETE): bytes_requested != bytes_transferred");
            logIfCV(debug_file,
                    "DEBUG: completed transfer, bytes_requested: {}, bytes_transferred: {}, operationBuffer.size(): "
                    "{}, operationBuffer pointer: {:p}, outcome buffer pointer: {:p}, file: {}",
                    outcome.bytes_requested, outcome.bytes_transferred, context->operationBuffer.size(),
                    static_cast<void*>(context->operationBuffer.data()), static_cast<void*>(outcome.buffer),
                    context->path);

        } else if(outcome.result == SDL_ASYNCIO_CANCELED) {
            logIfCV(debug_file, "WARNING: only read {}/{} bytes from {}!", outcome.bytes_transferred,
                    outcome.bytes_requested, context->path);
            status = OperationContext::OP_PARTIAL;
        } else if(outcome.result == SDL_ASYNCIO_FAILURE) {
            debugLog("ERROR: read failed for {}: {}", context->path, SDL_GetError());
            status = OperationContext::OP_FAIL;
        }

        // initiate close operation
        auto* closeContext = new OperationContext(context->path);
        // null handle for close
        closeContext->status = status;
        closeContext->readCallback = std::move(context->readCallback);
        closeContext->operationBuffer = std::move(context->operationBuffer);

        // don't flush on closing read operations
        if(!SDL_CloseAsyncIO(context->handle, false, m_queue, closeContext)) {
            // we will never be able to free the context in the close complete callback if this somehow happens
            // so run the callback now
            debugLog("ERROR: failed to close {}: {}", closeContext->path, SDL_GetError());
            m_activeFiles.erase(closeContext->path);
            if(closeContext->readCallback) {
                PERFORM_CALLBACK(closeContext->readCallback(std::move(closeContext->operationBuffer)));
            }
            delete closeContext;
        }

        // delete old context
        delete context;
    }

    void handleWriteComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);
        if(!context) return;  // nothing to do

        OperationContext::OpStatus status = OperationContext::OP_COMPLETE;

        if(outcome.result == SDL_ASYNCIO_CANCELED) {
            logIfCV(debug_file, "WARNING: partially wrote {}/{} bytes for {}!", outcome.bytes_requested,
                    outcome.bytes_transferred, context->path);
            status = OperationContext::OP_FAIL;
        } else if(outcome.result == SDL_ASYNCIO_FAILURE) {
            debugLog("ERROR: write failed for {}: {}", context->path, SDL_GetError());
            status = OperationContext::OP_FAIL;
        }

        // initiate close operation
        auto* closeContext = new OperationContext(context->path);
        // null handle for close
        closeContext->status = status;
        closeContext->writeCallback = std::move(context->writeCallback);

        // flush to make sure data reaches disk
        if(!SDL_CloseAsyncIO(context->handle, true, m_queue, closeContext)) {
            debugLog("ERROR: failed to close {}: {}", closeContext->path, SDL_GetError());
            m_activeFiles.erase(closeContext->path);
            if(closeContext->writeCallback) {
                PERFORM_CALLBACK(
                    closeContext->writeCallback(status == OperationContext::OP_COMPLETE));  // probably not fatal?
            }
            delete closeContext;
        }

        // delete old context (which had our to-be-written data buffer in it)
        delete context;
    }

    void handleCloseComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);
        if(!context) return;  // nothing to do

        m_activeFiles.erase(context->path);

        if(outcome.result != SDL_ASYNCIO_COMPLETE) {
            logIfCV(debug_file, "WARNING: close failed for {}: {}", context->path, SDL_GetError());
        }

        if(context->writeCallback) {
            PERFORM_CALLBACK(context->writeCallback(context->status == OperationContext::OP_COMPLETE));
        } else if(context->readCallback) {
            // we don't really propagate errors here besides the log in
            // handleReadComplete and an empty/partially filled buffer here...
            PERFORM_CALLBACK(context->readCallback(std::move(context->operationBuffer)));
        }

        delete context;
    }

#undef PERFORM_CALLBACK

    SDL_AsyncIOQueue* m_queue{nullptr};
    std::unordered_set<std::string> m_activeFiles;

    std::atomic<size_t> m_activeCallbacks{0};
};
#endif  // MCENGINE_PLATFORM_WASM

AsyncIOHandler::AsyncIOHandler() : m_impl() {}
AsyncIOHandler::~AsyncIOHandler() { cleanup(); }

void AsyncIOHandler::cleanup() { m_impl->cleanup(); }

// if this doesn't succeed (checked once on startup), the engine immediately exits
#ifdef MCENGINE_PLATFORM_WASM
bool AsyncIOHandler::succeeded() const { return true; }
void AsyncIOHandler::update() {}
#else
bool AsyncIOHandler::succeeded() const { return m_impl->m_queue != nullptr; }
void AsyncIOHandler::update() { m_impl->update(); }
#endif

bool AsyncIOHandler::read(std::string_view path, ReadCallback callback) {
    return m_impl->read(path, std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, std::vector<u8> data, WriteCallback callback) {
    return m_impl->write(path, std::move(data), std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, std::string data, WriteCallback callback) {
    return m_impl->write(path,
                         std::vector<u8>{std::make_move_iterator(data.begin()), std::make_move_iterator(data.end())},
                         std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, const u8* data, size_t amount, WriteCallback callback) {
    return m_impl->write(path, std::vector<u8>(data, data + amount), std::move(callback));
}
