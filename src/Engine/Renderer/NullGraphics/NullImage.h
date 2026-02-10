// Copyright (c) 2026, WH, All rights reserved.
// image that does CPU-side pixel loading but never uploads to GPU
#pragma once

#include "Image.h"

class NullImage : public Image {
   public:
    NullImage(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    NullImage(i32 width, i32 height, bool mipmapped = false, bool keepInSystemMemory = false);

    void bind(unsigned int textureUnit) const override;
    void unbind() const override;

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;
};
