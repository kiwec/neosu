//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu implementation of Image
//
// $NoKeywords: $sdlgpuimg
//===============================================================================//

#pragma once
#ifndef SDLGPUIMAGE_H
#define SDLGPUIMAGE_H
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU
#include "NullImage.h"

class SDLGPUImage final : public NullImage {
    NOCOPY_NOMOVE(SDLGPUImage)
   public:
    SDLGPUImage(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    SDLGPUImage(int width, int height, bool mipmapped = false, bool keepInSystemMemory = false);
    ~SDLGPUImage() override = default;
};

#endif

#endif
