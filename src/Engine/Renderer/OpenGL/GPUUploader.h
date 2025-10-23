// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "config.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include "Graphics.h"
#include "SyncCV.h"
#include "SyncJthread.h"
#include "Image.h"

#include <atomic>
#include <queue>

typedef struct __GLsync *GLsync;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_GLContextState *SDL_GLContext;
typedef unsigned int GLuint;
typedef void *SDL_EGLDisplay;

class GPUUploader final {
    NOCOPY_NOMOVE(GPUUploader)

   public:
    struct UploadRequest {
        struct Unique {
            std::unique_ptr<Image::SizedRGBABytes> pixelData;
            Graphics::FILTER_MODE filterMode;
            Graphics::WRAP_MODE wrapMode;
        };
        struct Reupload {
            // pixelData must be kept in system memory, it won't be copied
            const Image::SizedRGBABytes *pixelData;
        };
        using ReqData = std::variant<Unique, Reupload>;
        ReqData payload;
        std::atomic<GLuint> *textureId; /* in param for reloads, out param for full loads */
        std::atomic<GLsync> *outFence;
        const std::atomic<bool> *interrupted;
        bool mipmapped;
    };

    GPUUploader(SDL_Window *window, SDL_GLContext mainContext);
    ~GPUUploader();

    // queue an image upload, returns immediately
    // outTextureId and outFence will be written by the GPU thread
    void queueImageUpload(std::unique_ptr<Image::SizedRGBABytes> &&pixelData, bool mipmapped,
                          Graphics::FILTER_MODE filterMode, Graphics::WRAP_MODE wrapMode,
                          std::atomic<GLuint> *outTextureId, std::atomic<GLsync> *outFence, const std::atomic<bool> *interruption);

    void queueImageReupload(const std::unique_ptr<Image::SizedRGBABytes> &pixelData, bool mipmapped,
                            std::atomic<GLuint> *inTextureId, std::atomic<GLsync> *outFence, const std::atomic<bool> *interruption);

    [[nodiscard]] inline size_t getQueueSize() const { return this->iQueueSize.load(std::memory_order_acquire); }
    [[nodiscard]] inline bool isShuttingDown() const { return this->bShuttingDown.load(std::memory_order_acquire); }
    [[nodiscard]] inline bool isReady() const { return this->bReady.load(std::memory_order_acquire); }

   private:
    void uploadThreadFunc();
    void processUpload(const UploadRequest &request);

    SDL_Window *window;
    SDL_GLContext sharedContext;
#ifdef MCENGINE_PLATFORM_LINUX
    SDL_EGLDisplay eglDisplay;
#endif

    std::queue<UploadRequest> uploadQueue;
    Sync::mutex queueMutex;
    Sync::condition_variable workAvailable;

    std::atomic<size_t> iQueueSize{0};
    std::atomic<bool> bShuttingDown{false};
    std::atomic<bool> bReady{false};

    Sync::jthread uploadThread;
};

extern std::unique_ptr<GPUUploader> gpuUploader;

#endif
