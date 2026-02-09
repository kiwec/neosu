//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu implementation of RenderTarget / render to texture
//
// $NoKeywords: $sdlgpu
//===============================================================================//

#pragma once
#ifndef SDLGPURENDERTARGET_H
#define SDLGPURENDERTARGET_H
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU
#include "NullRenderTarget.h"

class SDLGPURenderTarget final : public NullRenderTarget {
    NOCOPY_NOMOVE(SDLGPURenderTarget)
   public:
    SDLGPURenderTarget(int x, int y, int width, int height,
                          MultisampleType multiSampleType = MultisampleType::X0);
    ~SDLGPURenderTarget() override { destroy(); }
};

#endif

#endif
