#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#ifndef OPENGLIMAGE_H
#define OPENGLIMAGE_H

#include "Image.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

class OpenGLImage final : public Image {
    NOCOPY_NOMOVE(OpenGLImage)
   public:
    OpenGLImage(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false)
        : Image(std::move(filepath), mipmapped, keepInSystemMemory) {}
    OpenGLImage(i32 width, i32 height, bool mipmapped = false, bool keepInSystemMemory = false)
        : Image(width, height, mipmapped, keepInSystemMemory) {}
    ~OpenGLImage() override;

    void bind(unsigned int textureUnit = 0) const override;
    void unbind() const override;

    void setFilterMode(TextureFilterMode filterMode) override;
    void setWrapMode(TextureWrapMode wrapMode) override;

   private:
    void init() override;
    void initAsync() override;
    void destroy() override;

    void handleGLErrors();
    void deleteGL();

    mutable unsigned int GLTexture{0};
    mutable unsigned int iTextureUnitBackup{0};
};

#endif

#endif
