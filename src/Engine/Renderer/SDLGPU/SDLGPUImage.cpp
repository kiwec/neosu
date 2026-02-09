//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu implementation of Image
//
// $NoKeywords: $sdlgpuimg
//===============================================================================//
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU
#include "SDLGPUImage.h"

SDLGPUImage::SDLGPUImage(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : NullImage(std::move(filepath), mipmapped, keepInSystemMemory) {}

SDLGPUImage::SDLGPUImage(int width, int height, bool mipmapped, bool keepInSystemMemory)
    : NullImage(width, height, mipmapped, keepInSystemMemory) {}

#endif
