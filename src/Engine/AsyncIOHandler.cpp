// Copyright (c) 2025, WH, All rights reserved.
#include "AsyncIOHandler.h"
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
            // drain all completed results to avoid leaking contexts
            SDL_AsyncIOOutcome outcome;
            while(SDL_GetAsyncIOResult(m_queue, &outcome)) {
                auto* context = static_cast<OperationContext*>(outcome.userdata);
                if(outcome.buffer) {
                    SDL_free(outcome.buffer);
                }
                delete context;
            }

            // blocks until all pending tasks complete
            SDL_DestroyAsyncIOQueue(m_queue);
        }
    }

    struct OperationContext {
        // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
        enum OpType : uint8_t { READ, WRITE, CLOSE_AFTER_WRITE };

        OperationContext() = delete;
        OperationContext(OpType type, std::string path) : type(type), path(std::move(path)) {}

        OpType type;
        std::string path;

        // READ-specific
        std::function<void(std::vector<u8>)> readCallback;

        // WRITE-specific
        std::shared_ptr<std::vector<u8>> writeData;
        SDL_AsyncIO* handle{nullptr};

        // CLOSE_AFTER_WRITE-specific
        bool writeSucceeded{false};
        std::function<void(bool)> writeCallback;
    };

    void update() {
        assert(!!m_queue);

        SDL_AsyncIOOutcome outcome;
        while(SDL_GetAsyncIOResult(m_queue, &outcome)) {
            auto* context = static_cast<OperationContext*>(outcome.userdata);

            switch(context->type) {
                case OperationContext::READ:
                    handleReadComplete(outcome, context);
                    break;
                case OperationContext::WRITE:
                    handleWriteComplete(outcome, context);
                    break;
                case OperationContext::CLOSE_AFTER_WRITE:
                    handleCloseComplete(outcome, context);
                    break;
            }
        }
    }

    bool read(std::string_view path, std::function<void(std::vector<u8>)> callback) {
        assert(!!m_queue);

        std::string pathStr(path);
        if(m_activeFiles.contains(pathStr)) {
            return false;
        }

        auto context = std::make_unique<OperationContext>(OperationContext::READ, pathStr);
        context->readCallback = std::move(callback);

        m_activeFiles.insert(pathStr);

        if(!SDL_LoadFileAsync(pathStr.c_str(), m_queue, context.get())) {
            debugLog("SDL_LoadFileAsync failed for {}: {}", pathStr, SDL_GetError());
            m_activeFiles.erase(pathStr);
            return false;
        }

        return !!context.release();
    }

    bool write(std::string_view path, std::vector<u8> data, std::function<void(bool)> callback) {
        assert(!!m_queue);

        std::string pathStr(path);
        if(m_activeFiles.contains(pathStr)) {
            debugLog("cannot write to {}, file is in use", path);
            if(callback) {
                callback(false);
            }
            return false;
        }

        SDL_AsyncIO* handle = SDL_AsyncIOFromFile(pathStr.c_str(), "w");
        if(!handle) {
            debugLog("failed to open {} for writing: {}", pathStr, SDL_GetError());
            if(callback) {
                callback(false);
            }
            return false;
        }

        auto context = std::make_unique<OperationContext>(OperationContext::WRITE, pathStr);
        context->handle = handle;
        context->writeData = std::make_shared<std::vector<u8>>(std::move(data));
        context->writeCallback = std::move(callback);

        m_activeFiles.insert(pathStr);

        if(!SDL_WriteAsyncIO(handle, context->writeData->data(), 0, context->writeData->size(), m_queue,
                             context.get())) {
            debugLog("SDL_WriteAsyncIO failed for {}: {}", pathStr, SDL_GetError());
            if(callback) {
                callback(false);
            }
            // attempt cleanup, but don't wait for result
            SDL_CloseAsyncIO(handle, false, m_queue, nullptr);
            m_activeFiles.erase(pathStr);
            return false;
        }

        return !!context.release();
    }

    void handleReadComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);

        std::vector<u8> data;

        if(outcome.result == SDL_ASYNCIO_COMPLETE && outcome.buffer) {
            data.resize(outcome.bytes_transferred);
            std::memcpy(data.data(), outcome.buffer, outcome.bytes_transferred);
            SDL_free(outcome.buffer);
        } else if(outcome.result == SDL_ASYNCIO_FAILURE) {
            debugLog("read failed for {}: {}", context->path, SDL_GetError());
        }

        context->readCallback(std::move(data));
        m_activeFiles.erase(context->path);
        delete context;
    }

    void handleWriteComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);

        bool writeSucceeded = (outcome.result == SDL_ASYNCIO_COMPLETE);

        if(!writeSucceeded) {
            debugLog("write failed for {}: {}", context->path, SDL_GetError());
        }

        // initiate close operation
        auto closeContext = std::make_unique<OperationContext>(OperationContext::CLOSE_AFTER_WRITE, context->path);
        closeContext->writeSucceeded = writeSucceeded;
        closeContext->writeCallback = std::move(context->writeCallback);

        // flush to ensure data reaches disk
        SDL_CloseAsyncIO(context->handle, true, m_queue, closeContext.get());
        auto unused = closeContext.release();
        (void)unused;

        delete context;
    }

    void handleCloseComplete(const SDL_AsyncIOOutcome& outcome, OperationContext* context) {
        assert(!!m_queue);

        bool closeSucceeded = (outcome.result == SDL_ASYNCIO_COMPLETE);
        bool overallSuccess = context->writeSucceeded && closeSucceeded;

        if(!closeSucceeded) {
            debugLog("close failed for {}: {}", context->path, SDL_GetError());
        }

        if(context->writeCallback) {
            context->writeCallback(overallSuccess);
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

bool AsyncIOHandler::read(std::string_view path, std::function<void(std::vector<u8>)> callback) {
    return m_io->read(path, std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, std::vector<u8> data, std::function<void(bool)> callback) {
    return m_io->write(path, std::move(data), std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, std::string data, std::function<void(bool)> callback) {
    return m_io->write(path, std::vector<u8>(data.begin(), data.end()), std::move(callback));
}

bool AsyncIOHandler::write(std::string_view path, const u8* data, size_t amount, std::function<void(bool)> callback) {
    return m_io->write(path, std::vector<u8>(data, data + amount), std::move(callback));
}
