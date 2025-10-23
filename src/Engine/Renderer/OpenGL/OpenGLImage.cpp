// Copyright (c) 2016, PG, All rights reserved.
#include "OpenGLImage.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include <utility>

#include "Environment.h"
#include "Engine.h"
#include "ConVar.h"
#include "File.h"
#include "Logging.h"
#include "GPUUploader.h"
#include "Timing.h"
#include "OpenGLHeaders.h"

namespace {
// sentinel value for queued upload
static int uploadPendingSentinel = 0;
GLsync UPLOAD_PENDING = reinterpret_cast<GLsync>(&uploadPendingSentinel);
}  // namespace

OpenGLImage::OpenGLImage(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : Image(std::move(filepath), mipmapped, keepInSystemMemory) {
    this->iTextureUnitBackup = 0;
}

OpenGLImage::OpenGLImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory)
    : Image(width, height, mipmapped, keepInSystemMemory) {
    this->iTextureUnitBackup = 0;
}

OpenGLImage::~OpenGLImage() {
    this->destroy();
    this->deleteGL();
    this->rawImage.reset();
}

bool OpenGLImage::isReadyForSyncInit() const {
    GLsync fence = this->uploadFence.load(std::memory_order_acquire);

    // no async upload in progress
    if(fence == nullptr) {
        return true;
    }

    // upload queued but not yet processed by GPU thread
    if(fence == UPLOAD_PENDING) {
        return false;
    }

    // check if GPU upload is complete
    GLint status = 0;
    glGetSynciv(fence, GL_SYNC_STATUS, sizeof(GLint), nullptr, &status);
    return status == GL_SIGNALED;
}

void OpenGLImage::init() {
    GLsync fence = this->uploadFence.load(std::memory_order_acquire);
    const bool debug = cv::debug_image.getBool() || cv::r_gpuupload_debug.getBool();

    // GPU upload was queued, wait (if we have to) for completion
    if(fence != nullptr) {
        // wait for GPU thread to process the request
        if(fence == UPLOAD_PENDING) {
            logIf(debug, "OpenGLImage: Waiting for GPU upload to be picked up for {:s}", this->sFilePath);

            while((fence = this->uploadFence.load(std::memory_order_acquire)) == UPLOAD_PENDING) {
                // check if uploader is shutting down to avoid infinite loop
                if(gpuUploader && gpuUploader->isShuttingDown()) {
                    logIf(debug, "OpenGLImage: GPU uploader shutting down, falling back to sync upload for {:s}",
                          this->sFilePath);
                    this->uploadFence.store(nullptr, std::memory_order_release);
                    fence = nullptr;
                    break;
                }
                Timing::sleep(0);
            }
        }

        // wait for GPU to complete the upload
        if(fence != nullptr && fence != UPLOAD_PENDING) {
            logIf(debug, "OpenGLImage: Waiting for GPU fence for {:s}",
                  this->bCreatedImage ? this->sName : this->sFilePath);

            glClientWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(fence);
            this->uploadFence.store(nullptr, std::memory_order_release);

            logIf(debug, "OpenGLImage: GPU upload complete for {:s}",
                  this->bCreatedImage ? this->sName : this->sFilePath);
        }

        // if fence was valid, texture is now uploaded and ready
        GLuint texture = this->GLTexture.load(std::memory_order_acquire);
        if(texture != 0) {
            this->setReady(true);
            return;
        }
        debugLog("OpenGLImage WARNING: Texture still 0 after async load for {}",
                 this->bCreatedImage ? this->sName : this->sFilePath);
        // if texture is still 0, something went wrong (fall through to sync path)
    }

    // fallback: synchronous upload path
    // this happens when GPU uploader is unavailable, disabled, or failed
    GLuint texture = this->GLTexture.load(std::memory_order_acquire);
    if((texture != 0 && !this->bKeepInSystemMemory) || !this->isAsyncReady()) {
        // already loaded (and not reloadable) or async failed
        return;
    }

    // we need rawImage for sync upload
    if(!this->rawImage || this->totalBytes() == 0) {
        debugLog("OpenGLImage ERROR: Cannot upload texture for {:s}, no pixel data available",
                 this->bCreatedImage ? this->sName : this->sFilePath);
        return;
    }

    logIf(debug, "OpenGLImage: Performing sync upload for {:s}", this->bCreatedImage ? this->sName : this->sFilePath);

    // FFP compat
    if constexpr(Env::cfg(REND::GL)) {
        glEnable(GL_TEXTURE_2D);
    }

    // create texture object only if it doesn't exist yet
    if(texture == 0) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        // set texture filtering mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->bMipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // texture wrapping
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        // texture exists (reload case with bKeepInSystemMemory)
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    // upload (or re-upload) texture data to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->rawImage->getX(), this->rawImage->getY(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, this->rawImage->data());

    if(this->bMipmapped) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    this->GLTexture.store(texture, std::memory_order_release);

    // free from RAM if not keeping
    if(!this->bKeepInSystemMemory) {
        this->rawImage.reset();
    }

    this->setReady(true);

    // apply filter/wrap modes if non-default (only on first creation)
    if(texture == 0) {
        if(this->filterMode != Graphics::FILTER_MODE::FILTER_MODE_LINEAR) {
            setFilterMode(this->filterMode);
        }

        if(this->wrapMode != Graphics::WRAP_MODE::WRAP_MODE_CLAMP) {
            setWrapMode(this->wrapMode);
        }
    }
}

void OpenGLImage::initAsync() {
    GLuint texture = this->GLTexture.load(std::memory_order_acquire);
    bool reupload = false;
    /* we might be reuploading a changed image */
    if(texture != 0 && !(reupload = (this->bCreatedImage && this->rawImage && this->bKeepInSystemMemory))) {
        this->setAsyncReady(true);
        return;
    }

    // prepare pixel data
    bool asyncReady = false;
    if(!this->bCreatedImage) {
        if(cv::debug_rm.getBool()) debugLog("Resource Manager: Loading {:s}", this->sFilePath.c_str());
        asyncReady = this->loadRawImage();
    } else {
        // created image is always async ready (created during ctor)
        asyncReady = true && !this->isInterrupted();
    }

    this->setAsyncReady(asyncReady);

    // queue GPU upload for all cases if available
    if(asyncReady && this->rawImage && gpuUploader && gpuUploader->isReady() && !gpuUploader->isShuttingDown() &&
       cv::r_async_gpu.getBool()) {
        // set sentinel
        this->uploadFence.store(UPLOAD_PENDING, std::memory_order_release);

        if(!this->bKeepInSystemMemory) {
            // get rid of it
            gpuUploader->queueImageUpload(std::move(this->rawImage), this->bMipmapped, this->filterMode, this->wrapMode,
                                          &this->GLTexture, &this->uploadFence, &this->bInterrupted);
            this->rawImage.reset();
        } else if(reupload /* reuploading from system memory */) {
            gpuUploader->queueImageReupload(this->rawImage, this->bMipmapped, &this->GLTexture, &this->uploadFence,
                                            &this->bInterrupted);
        } else {
            // make a copy
            auto copy = std::make_unique<Image::SizedRGBABytes>(*this->rawImage);
            gpuUploader->queueImageUpload(std::move(copy), this->bMipmapped, this->filterMode, this->wrapMode,
                                          &this->GLTexture, &this->uploadFence, &this->bInterrupted);
        }
    }
    // if GPU uploader not available, init() will fall back to sync upload
}

void OpenGLImage::destroy() {
    this->interruptLoad();
    // don't delete the texture if we're keeping it in memory, for reloads
    if(!this->bKeepInSystemMemory) {
        this->deleteGL();
        this->rawImage.reset();
    }
}

void OpenGLImage::deleteGL() {
    // wait for any pending GPU upload to complete before destroying
    GLsync fence = this->uploadFence.load(std::memory_order_acquire);
    if(fence && fence != UPLOAD_PENDING && !glIsSync(fence)) {
        debugLog("WARNING: {:p} was not a valid sync fence for: {}", static_cast<void*>(fence),
                 this->bCreatedImage ? this->sName : this->sFilePath);
        this->uploadFence.store(nullptr, std::memory_order_release);
    } else if(fence) {
        if(!(gpuUploader && gpuUploader->isShuttingDown()) && fence == UPLOAD_PENDING) {
            // upload queued but not processed, wait for GPU thread to pick it up
            while((fence = this->uploadFence.load(std::memory_order_acquire)) == UPLOAD_PENDING) {
                if((gpuUploader && gpuUploader->isShuttingDown())) {
                    break;
                }
                Timing::sleep(0);
            }
        }

        if(!(gpuUploader && gpuUploader->isShuttingDown()) &&
           (fence = this->uploadFence.load(std::memory_order_acquire)) != nullptr && fence != UPLOAD_PENDING) {
            glClientWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(fence);
            this->uploadFence.store(nullptr, std::memory_order_release);
        }
    }

    GLuint texture = this->GLTexture.load(std::memory_order_acquire);
    if(texture != 0 && glDeleteTextures != nullptr && glIsTexture != nullptr) {
        if(!glIsTexture(texture)) {
            debugLog("WARNING: tried to glDeleteTexture on {} ({:p}), which is not a valid GL texture!", this->sName,
                     static_cast<const void*>(&texture));
        } else {
            glDeleteTextures(1, &texture);
        }
    }
    this->GLTexture.store(0, std::memory_order_release);
}

void OpenGLImage::bind(unsigned int textureUnit) const {
    if(!this->isTextureReady()) return;

    this->iTextureUnitBackup = textureUnit;

    // switch texture units before enabling+binding
    glActiveTexture(GL_TEXTURE0 + textureUnit);

    // set texture
    GLuint texture = this->GLTexture.load(std::memory_order_acquire);
    glBindTexture(GL_TEXTURE_2D, texture);

    // FFP compatibility (part 2)
    if constexpr(Env::cfg(REND::GL)) {
        glEnable(GL_TEXTURE_2D);
    }
}

void OpenGLImage::unbind() const {
    // don't check ready here, just unbind

    // restore texture unit (just in case) and set to no texture
    glActiveTexture(GL_TEXTURE0 + this->iTextureUnitBackup);
    glBindTexture(GL_TEXTURE_2D, 0);

    // restore default texture unit
    if(this->iTextureUnitBackup != 0) glActiveTexture(GL_TEXTURE0);
}

void OpenGLImage::setFilterMode(Graphics::FILTER_MODE filterMode) {
    Image::setFilterMode(filterMode);
    if(!this->isTextureReady()) return;

    bind();
    {
        switch(filterMode) {
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
    }
    unbind();
}

void OpenGLImage::setWrapMode(Graphics::WRAP_MODE wrapMode) {
    Image::setWrapMode(wrapMode);
    if(!this->isTextureReady()) return;

    bind();
    {
        switch(wrapMode) {
            case Graphics::WRAP_MODE::WRAP_MODE_CLAMP:  // NOTE: there is also GL_CLAMP, which works a bit differently
                                                        // concerning the border color
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                break;
            case Graphics::WRAP_MODE::WRAP_MODE_REPEAT:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                break;
        }
    }
    unbind();
}

void OpenGLImage::handleGLErrors() {
    // no
}

#endif
