#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#ifndef OPENGLIMAGE_H
#define OPENGLIMAGE_H

#include "Image.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include <atomic>

typedef struct __GLsync *GLsync;
typedef unsigned int GLuint;

class OpenGLImage final : public Image {
   public:
    OpenGLImage(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    OpenGLImage(i32 width, i32 height, bool mipmapped = false, bool keepInSystemMemory = false);
    ~OpenGLImage() override;

    void bind(unsigned int textureUnit = 0) const override;
    void unbind() const override;

    void setFilterMode(Graphics::FILTER_MODE filterMode) override;
    void setWrapMode(Graphics::WRAP_MODE wrapMode) override;
    [[nodiscard]] bool isReadyForSyncInit() const override;

   private:
    void init() override;
    void initAsync() override;
    void destroy() override;

    [[nodiscard]] inline bool isTextureReady() const {
        return this->isReady() || (this->GLTexture.load(std::memory_order_acquire) != 0 && this->isAsyncReady());
    }

    void handleGLErrors();
    void deleteGL();

    std::atomic<GLsync> uploadFence{nullptr};
    std::atomic<GLuint> GLTexture{0};
    mutable unsigned int iTextureUnitBackup{0};
};

#endif

#endif
