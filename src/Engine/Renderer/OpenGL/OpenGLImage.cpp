// Copyright (c) 2016, PG, All rights reserved.
#include "OpenGLImage.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include <utility>

#include "Environment.h"
#include "Engine.h"
#include "ConVar.h"
#include "File.h"
#include "Logging.h"

#include "OpenGLHeaders.h"

OpenGLImage::OpenGLImage(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : Image(std::move(filepath), mipmapped, keepInSystemMemory) {
    this->GLTexture = 0;
    this->iTextureUnitBackup = 0;
}

OpenGLImage::OpenGLImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory)
    : Image(width, height, mipmapped, keepInSystemMemory) {
    this->GLTexture = 0;
    this->iTextureUnitBackup = 0;
}

OpenGLImage::~OpenGLImage() {
    this->destroy();
    this->deleteGL();
    this->rawImage.reset();
}

void OpenGLImage::init() {
    // only load if not:
    // 1. already uploaded to gpu, and we didn't keep the image in system memory
    // 2. failed to async load
    if((this->GLTexture != 0 && !this->bKeepInSystemMemory) || !(this->isAsyncReady())) {
        if(cv::debug_image.getBool()) {
            debugLog(
                "we are already loaded, bReady: {} createdImage: {} GLTexture: {} bKeepInSystemMemory: {} bAsyncReady: "
                "{}",
                this->isReady(), this->bCreatedImage, this->GLTexture, this->bKeepInSystemMemory, this->isAsyncReady());
        }
        return;
    }

    // rawImage cannot be empty here, if it is, we're screwed
    assert(this->totalBytes() != 0);

    // create texture object
    const bool glTextureWasEmpty = this->GLTexture == 0;
    if(glTextureWasEmpty) {
        // FFP compatibility (part 1)
        if constexpr(Env::cfg(REND::GL)) {
            glEnable(GL_TEXTURE_2D);
        }

        // create texture and bind
        glGenTextures(1, &this->GLTexture);
        glBindTexture(GL_TEXTURE_2D, this->GLTexture);

        // set texture filtering mode (mipmapping is disabled by default)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->bMipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // texture wrapping, defaults to clamp
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // upload to gpu
    {
        if(!glTextureWasEmpty) {  // just to avoid redundantly binding
            glBindTexture(GL_TEXTURE_2D, this->GLTexture);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->rawImage->getX(), this->rawImage->getY(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, this->rawImage->data());
        if(this->bMipmapped) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    // free from RAM (it's now in VRAM)
    if(!this->bKeepInSystemMemory) {
        this->rawImage.reset();
    }

    this->setReady(true);

    if(this->filterMode != Graphics::FILTER_MODE::FILTER_MODE_LINEAR) {
        setFilterMode(this->filterMode);
    }

    if(this->wrapMode != Graphics::WRAP_MODE::WRAP_MODE_CLAMP) {
        setWrapMode(this->wrapMode);
    }
}

void OpenGLImage::initAsync() {
    if(this->GLTexture != 0) {
        this->setAsyncReady(true);
        return;  // only load if we are not already loaded
    }

    if(!this->bCreatedImage) {
        if(cv::debug_rm.getBool()) debugLog("Resource Manager: Loading {:s}", this->sFilePath.c_str());

        this->setAsyncReady(loadRawImage());
    } else {
        // created image is always async ready
        this->setAsyncReady(true);
    }
}

void OpenGLImage::destroy() {
    // don't delete the texture if we're keeping it in memory, for reloads
    if(!this->bKeepInSystemMemory) {
        this->deleteGL();
        this->rawImage.reset();
    }
}

void OpenGLImage::deleteGL() {
    if(this->GLTexture != 0 && glDeleteTextures != nullptr && glIsTexture != nullptr) {
        if(!glIsTexture(this->GLTexture)) {
            debugLog("WARNING: tried to glDeleteTexture on {} ({:p}), which is not a valid GL texture!", this->sName,
                     static_cast<const void*>(&this->GLTexture));
        } else {
            glDeleteTextures(1, &this->GLTexture);
        }
    }
    this->GLTexture = 0;
}

void OpenGLImage::bind(unsigned int textureUnit) const {
    if(!this->isReady()) return;

    this->iTextureUnitBackup = textureUnit;

    // switch texture units before enabling+binding
    glActiveTexture(GL_TEXTURE0 + textureUnit);

    // set texture
    glBindTexture(GL_TEXTURE_2D, this->GLTexture);

    // FFP compatibility (part 2)
    if constexpr(Env::cfg(REND::GL)) {
        glEnable(GL_TEXTURE_2D);
    }
}

void OpenGLImage::unbind() const {
    if(!this->isReady() || !cv::r_image_unbind_after_drawimage.getBool()) return;

    // restore texture unit (just in case) and set to no texture
    glActiveTexture(GL_TEXTURE0 + this->iTextureUnitBackup);
    glBindTexture(GL_TEXTURE_2D, 0);

    // restore default texture unit
    if(this->iTextureUnitBackup != 0) glActiveTexture(GL_TEXTURE0);
}

void OpenGLImage::setFilterMode(Graphics::FILTER_MODE filterMode) {
    Image::setFilterMode(filterMode);
    if(!this->isReady()) return;

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
    if(!this->isReady()) return;

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
