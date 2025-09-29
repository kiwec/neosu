#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#ifndef OPENGLIMAGE_H
#define OPENGLIMAGE_H

#include "Image.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

class OpenGLImage final : public Image {
   public:
    OpenGLImage(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    OpenGLImage(i32 width, i32 height, bool mipmapped = false, bool keepInSystemMemory = false);
    ~OpenGLImage() override;

    void bind(unsigned int textureUnit = 0) const override;
    void unbind() const override;

    void setFilterMode(Graphics::FILTER_MODE filterMode) override;
    void setWrapMode(Graphics::WRAP_MODE wrapMode) override;

   private:
    void init() override;
    void initAsync() override;
    void destroy() override;

    void handleGLErrors();
    void deleteGL();

    mutable unsigned int GLTexture;
    mutable unsigned int iTextureUnitBackup;
};

#endif

#endif
