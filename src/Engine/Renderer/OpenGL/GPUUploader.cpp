// Copyright (c) 2025, WH, All rights reserved.
#include "GPUUploader.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include "Image.h"
#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"
#include "OpenGLHeaders.h"
#include "Thread.h"

#include <SDL3/SDL_video.h>

#ifdef MCENGINE_PLATFORM_LINUX
#include <SDL3/SDL_egl.h>
namespace {
PFNEGLMAKECURRENTPROC peglMakeCurrent;
}
#endif

std::unique_ptr<GPUUploader> gpuUploader{nullptr};

GPUUploader::GPUUploader(SDL_Window *window, SDL_GLContext mainContext)
    : window(window),
      sharedContext(nullptr)
#ifdef MCENGINE_PLATFORM_LINUX
      ,
      eglDisplay(SDL_EGL_GetCurrentDisplay())
#endif
{
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    // create shared context on main thread
    this->sharedContext = SDL_GL_CreateContext(window);

    if(!this->sharedContext) {
        debugLog("GPUUploader ERROR: Failed to create shared GL context: {:s}", SDL_GetError());
        return;
    }

    // restore main context (SDL automatically makes a context current when you create it)
    if(!SDL_GL_MakeCurrent(window, mainContext)) {
        debugLog("GPUUploader ERROR: Failed to restore main GL context: {:s}", SDL_GetError());
        return;
    }
    logIfCV(r_gpuupload_debug, "GPUUploader: Created shared GL context");

    // start upload thread
    this->uploadThread = Sync::jthread([this]() { this->uploadThreadFunc(); });
}

GPUUploader::~GPUUploader() {
    logIfCV(r_gpuupload_debug, "GPUUploader: Shutting down...");

    this->bReady.store(false, std::memory_order_release);
    this->bShuttingDown.store(true, std::memory_order_release);
    this->workAvailable.notify_all();

    // thread will join automatically via jthread destructor

    // destroy shared context
    if(this->sharedContext) {
        SDL_GL_DestroyContext(this->sharedContext);
        this->sharedContext = nullptr;
    }
#ifdef MCENGINE_PLATFORM_LINUX
    if(this->eglDisplay) {
        peglMakeCurrent(this->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
#endif
    logIfCV(r_gpuupload_debug, "GPUUploader: Shutdown complete");
}

void GPUUploader::queueImageUpload(std::unique_ptr<Image::SizedRGBABytes> &&pixelData, bool mipmapped,
                                   Graphics::FILTER_MODE filterMode, Graphics::WRAP_MODE wrapMode,
                                   std::atomic<GLuint> *outTextureId, std::atomic<GLsync> *outFence,
                                   const std::atomic<bool> *interruption) {
    if(this->bShuttingDown.load()) {
        debugLog("GPUUploader WARNING: Attempted to queue upload during shutdown");
        return;
    }

    i32 reqWidth = pixelData->getX(), reqHeight = pixelData->getY();
    UploadRequest request{
        .payload =
            UploadRequest::Unique{
                .pixelData = std::move(pixelData),
                .filterMode = filterMode,
                .wrapMode = wrapMode,
            },
        .textureId = outTextureId,
        .outFence = outFence,
        .interrupted = interruption,
        .mipmapped = mipmapped,
    };

    {
        Sync::scoped_lock lock(this->queueMutex);
        this->uploadQueue.push(std::move(request));
        this->iQueueSize.fetch_add(1);
    }

    this->workAvailable.notify_one();

    logIfCV(r_gpuupload_debug, "GPUUploader: Queued upload {}x{}, queue size: {}", reqWidth, reqHeight,
            this->iQueueSize.load());
}

void GPUUploader::queueImageReupload(const std::unique_ptr<Image::SizedRGBABytes> &pixelData, bool mipmapped,
                                     std::atomic<GLuint> *inTextureId, std::atomic<GLsync> *outFence,
                                     const std::atomic<bool> *interruption) {
    if(this->bShuttingDown.load()) {
        debugLog("GPUUploader WARNING: Attempted to queue upload during shutdown");
        return;
    }

    i32 reqWidth = pixelData->getX(), reqHeight = pixelData->getY();
    UploadRequest request{.payload = UploadRequest::Reupload{.pixelData = pixelData.get()},
                          .textureId = inTextureId,
                          .outFence = outFence,
                          .interrupted = interruption,
                          .mipmapped = mipmapped};

    {
        Sync::scoped_lock lock(this->queueMutex);
        this->uploadQueue.push(std::move(request));
        this->iQueueSize.fetch_add(1);
    }

    this->workAvailable.notify_one();

    logIfCV(r_gpuupload_debug, "GPUUploader: Queued reupload {}x{} (image ID: {}), queue size: {}", reqWidth, reqHeight,
            inTextureId->load(std::memory_order_acquire), this->iQueueSize.load());
}

void GPUUploader::uploadThreadFunc() {
    logIfCV(r_gpuupload_debug, "GPUUploader: Upload thread started");

    McThread::set_current_thread_name("gpu_upload");
    McThread::set_current_thread_prio(false);

    bool success = false;

#ifdef MCENGINE_PLATFORM_LINUX
    // check if we're using EGL (e.g. wayland)
    if(this->eglDisplay) {
        logIfCV(r_gpuupload_debug, "GPUUploader: Detected EGL, using surfaceless context");

        // save this funcptr for shutdown
        peglMakeCurrent = reinterpret_cast<PFNEGLMAKECURRENTPROC>(SDL_EGL_GetProcAddress("eglMakeCurrent"));
        auto eglGetError = reinterpret_cast<PFNEGLGETERRORPROC>(SDL_EGL_GetProcAddress("eglGetError"));

        if(!peglMakeCurrent || !eglGetError) {
            debugLog("GPUUploader ERROR: Failed to get EGL function pointers");
            return;
        }

        auto eglContext = static_cast<EGLContext>(this->sharedContext);

        // make current without any surface (surfaceless context)
        if(peglMakeCurrent(this->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext) == EGL_TRUE) {
            success = true;
            logIfCV(r_gpuupload_debug, "GPUUploader: EGL context made current (surfaceless)");
        } else {
            SDL_EGLint error = eglGetError();
            debugLog("GPUUploader ERROR: eglMakeCurrent failed with error: 0x{:x}", error);
        }
    } else
#endif  // GLX/WGL path: SDL handles it normally
        if(SDL_GL_MakeCurrent(this->window, this->sharedContext)) {
            success = true;
            logIfCV(r_gpuupload_debug, "GPUUploader: Shared context made current on upload thread");
        } else {
            debugLog("GPUUploader ERROR: Failed to make shared context current: {:s}", SDL_GetError());
        }

    if(!success) {
        return;
    }

    this->bReady.store(true, std::memory_order_release);

    while(!this->bShuttingDown.load(std::memory_order_acquire)) {
        UploadRequest request;

        // wait for work
        {
            Sync::unique_lock lock(this->queueMutex);

            this->workAvailable.wait(lock, [this] {
                return this->bShuttingDown.load(std::memory_order_acquire) || !this->uploadQueue.empty();
            });

            if(this->bShuttingDown.load(std::memory_order_acquire) && this->uploadQueue.empty()) {
                break;
            }

            if(!this->uploadQueue.empty()) {
                request = std::move(this->uploadQueue.front());
                this->uploadQueue.pop();
                this->iQueueSize.fetch_sub(1);
            } else {
                continue;
            }
        }

        // ignore it if it was interrupted
        if(request.interrupted->load(std::memory_order_acquire)) {
            logIfCV(r_gpuupload_debug, "GPUUploader: A{}pload was interrupted before processing began, exiting early",
                    request.textureId->load(std::memory_order_acquire) == 0 ? "n u" : " reu");
            request.outFence->store(nullptr, std::memory_order_release);
            continue;
        }

        // process upload outside of lock
        processUpload(request);
    }

    logIfCV(r_gpuupload_debug, "GPUUploader: Upload thread exiting");
}

void GPUUploader::processUpload(const UploadRequest &request) {
    const bool debug = cv::debug_rm.getBool() || cv::r_gpuupload_debug.getBool();

    const UploadRequest::Unique *uniqueRequest = std::get_if<UploadRequest::Unique>(&request.payload);
    const UploadRequest::Reupload *reuploadRequest = std::get_if<UploadRequest::Reupload>(&request.payload);
    const bool isReupload = reuploadRequest != nullptr;

    const auto *pixelData = isReupload ? reuploadRequest->pixelData : uniqueRequest->pixelData.get();

    GLsync fence = nullptr;
    GLuint texture = isReupload ? request.textureId->load(std::memory_order_acquire) : 0;
    GLenum error = GL_NO_ERROR;
    bool interrupted = false;

    logIf(debug, "GPUUploader: Processing {}upload {}x{}{}", isReupload ? "re" : "", pixelData->getX(),
          pixelData->getY(), isReupload ? fmt::format(" for texture id {}", texture) : "");

    // ffp compatibility
    if constexpr(Env::cfg(REND::GL)) {
        glEnable(GL_TEXTURE_2D);
    }

    if(isReupload) {
        // still need to bind to existing texture
        glBindTexture(GL_TEXTURE_2D, texture);
    } else {
        // create and upload texture
        glGenTextures(1, &texture);
        if((interrupted = request.interrupted->load(std::memory_order_acquire))) goto interrupted;

        glBindTexture(GL_TEXTURE_2D, texture);

        // set filter mode
        switch(uniqueRequest->filterMode) {
            case Graphics::FILTER_MODE::FILTER_MODE_NONE:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case Graphics::FILTER_MODE::FILTER_MODE_LINEAR:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            case Graphics::FILTER_MODE::FILTER_MODE_MIPMAP:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
        }

        // set wrap mode
        switch(uniqueRequest->wrapMode) {
            case Graphics::WRAP_MODE::WRAP_MODE_CLAMP:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                break;
            case Graphics::WRAP_MODE::WRAP_MODE_REPEAT:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                break;
        }
        if((interrupted = request.interrupted->load(std::memory_order_acquire))) goto interrupted;
    }

    // upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixelData->getX(), pixelData->getY(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelData->data());

    if((interrupted = request.interrupted->load(std::memory_order_acquire))) goto interrupted;

    // generate mipmaps if requested
    if(request.mipmapped) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    if((interrupted = request.interrupted->load(std::memory_order_acquire))) goto interrupted;

    // check for errors
    error = glGetError();
    if(error != GL_NO_ERROR) {
        debugLog("GPUUploader ERROR: GL error during {}upload: 0x{:x}", isReupload ? "re" : "", error);
        glDeleteTextures(1, &texture);
        texture = 0;
    }

    // write output texture ID
    if(!isReupload) {
        request.textureId->store(texture, std::memory_order_release);
    }

    if(interrupted) {
    interrupted:
        logIf(debug, "GPUUploader: {}pload interrupted for {}, exiting early", isReupload ? "Reu" : "U", texture);
        // for reuploads just signal the fence, don't delete anything
        if(!isReupload && texture != 0) {
            // delete incomplete request textures
            glDeleteTextures(1, &texture);
            request.textureId->store(0, std::memory_order_release);
        }
        // remove the fence's sentinel value
        fence = nullptr;
    } else {
        // create fence to signal completion
        fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    request.outFence->store(fence, std::memory_order_release);

    logIf(debug, "GPUUploader: {}pload {}, texture ID: {}, fence: {:p}", isReupload ? "Reu" : "U", texture,
          interrupted ? "interrupted" : "complete", static_cast<void *>(fence));
    return;
}

#endif
