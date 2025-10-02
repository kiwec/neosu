// Copyright (c) 2025, WH, All rights reserved.
#include "AsyncIOHandler.h"
#include "ConVar.h"
#include "Logging.h"

#include <SDL3/SDL_asyncio.h>
#include <SDL3/SDL_error.h>

#include <cstring>
#include <string>
#include <unordered_set>

class AsyncIOHandler::InternalIOContext final {
    NOCOPY_NOMOVE(InternalIOContext)
   public:
    InternalIOContext() : m_queue(SDL_CreateAsyncIOQueue()) {
        if(!m_queue) {
            debugLog("failed to create async I/O queue: {}", SDL_GetError());
        }
    }

    ~InternalIOContext() {
        if(m_queue) {
            SDL_AsyncIOOutcome outcome;
            while(m_activeFiles.size() > 0 && SDL_WaitAsyncIOResult(m_queue, &outcome, 5000)) {
                // drain the queue and free our contexts
                drainIoResults(outcome);
            }
            SDL_DestroyAsyncIOQueue(m_queue);
        }
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

    bool read(std::string_view path, ReadCallback callback) {
        assert(!!m_queue);

        std::string pathStr(path);
        if(m_activeFiles.contains(pathStr)) {
            // TODO: multiple actions on the same file at the same time
            if(cv::debug_file.getBool()) debugLog("WARNING: cannot read from {}, file is in use", path);
            if(callback) {
                callback({});
            }
            return false;
        }

        SDL_AsyncIO* handle = SDL_AsyncIOFromFile(pathStr.c_str(), "r");
        if(!handle) {
            debugLog("ERROR: failed to open {} for reading: {}", pathStr, SDL_GetError());
            if(callback) {
                callback({});
            }
            return false;
        }

        i64 readSize = SDL_GetAsyncIOSize(handle);
        if(readSize < 0 || readSize > (2ULL * 1024 * 1024 * 1024)) {
            if(readSize < 0) {
                debugLog("ERROR: failed to open {} for reading: {}", pathStr, SDL_GetError());
            } else {
                // arbitrary size limit sanity check
                debugLog("ERROR: failed to open {} for reading, over 2GB in size!", pathStr);
            }
            if(callback) {
                callback({});
            }

            // close it but don't add a context/check for errors
            SDL_CloseAsyncIO(handle, false, m_queue, nullptr);

            return false;
        }

        auto* context = new OperationContext(pathStr);
        context->handle = handle;
        context->operationBuffer.reserve(readSize);
        context->readCallback = std::move(callback);

        if(!SDL_ReadAsyncIO(handle, context->operationBuffer.data(), 0, readSize, m_queue, context)) {
            debugLog("ERROR: SDL_ReadAsyncIO failed for {}: {}", pathStr, SDL_GetError());
            if(context->readCallback) {
                context->readCallback({});
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
            if(cv::debug_file.getBool()) debugLog("WARNING: cannot write to {}, file is in use", path);
            if(callback) {
                callback(false);
            }
            return false;
        }

        SDL_AsyncIO* handle = SDL_AsyncIOFromFile(pathStr.c_str(), "w");
        if(!handle) {
            debugLog("ERROR: failed to open {} for writing: {}", pathStr, SDL_GetError());
            if(callback) {
                callback(false);
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
                context->writeCallback(false);
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

        if(outcome.result == SDL_ASYNCIO_COMPLETE) {
            assert(outcome.bytes_requested == outcome.bytes_transferred);
        } else if(outcome.result == SDL_ASYNCIO_CANCELED) {
            context->operationBuffer.resize(outcome.bytes_transferred);
            if(cv::debug_file.getBool())
                debugLog("WARNING: only read {}/{} bytes from {}!", outcome.bytes_transferred, outcome.bytes_requested,
                         context->path);
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
            if(closeContext->readCallback) {
                closeContext->readCallback(closeContext->operationBuffer);
            }
            m_activeFiles.erase(closeContext->path);
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
            if(cv::debug_file.getBool())
                debugLog("WARNING: partially wrote {}/{} bytes for {}!", outcome.bytes_requested,
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
            if(closeContext->writeCallback) {
                closeContext->writeCallback(status == OperationContext::OP_COMPLETE);  // probably not fatal?
            }
            m_activeFiles.erase(closeContext->path);
            delete closeContext;
        }

        // delete old context (which had our to-be-written data buffer in it)
        delete context;
    }

    void handleCloseComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);
        if(!context) return;  // nothing to do

        if(outcome.result != SDL_ASYNCIO_COMPLETE) {
            if(cv::debug_file.getBool()) debugLog("WARNING: close failed for {}: {}", context->path, SDL_GetError());
        }

        if(context->writeCallback) {
            context->writeCallback(context->status == OperationContext::OP_COMPLETE);
        } else if(context->readCallback) {
            // we don't really propagate errors here besides the log in
            // handleReadComplete and an empty/partially filled buffer here...
            context->readCallback(context->operationBuffer);
        }

        m_activeFiles.erase(context->path);
        delete context;
    }

    SDL_AsyncIOQueue* m_queue{nullptr};
    std::unordered_set<std::string> m_activeFiles;
};

AsyncIOHandler::AsyncIOHandler() : m_io(std::make_unique<InternalIOContext>()) {}
AsyncIOHandler::~AsyncIOHandler() = default;

// if this doesn't succeed (checked once on startup), the engine immediately exits
bool AsyncIOHandler::succeeded() const { return !!m_io && m_io->m_queue != nullptr; }

// passthroughs
void AsyncIOHandler::update() { m_io->update(); }

bool AsyncIOHandler::read(std::string_view path, ReadCallback callback) {
    return m_io->read(path, std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, std::vector<u8> data, WriteCallback callback) {
    return m_io->write(path, std::move(data), std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, std::string data, WriteCallback callback) {
    return m_io->write(path, std::vector<u8>(data.begin(), data.end()), std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, const u8* data, size_t amount, WriteCallback callback) {
    return m_io->write(path, std::vector<u8>(data, data + amount), std::move(callback));
}
