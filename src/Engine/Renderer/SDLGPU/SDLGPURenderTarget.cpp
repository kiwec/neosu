//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu implementation of RenderTarget / render to texture
//
// $NoKeywords: $sdlgpurt
//===============================================================================//
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include "SDLGPURenderTarget.h"

SDLGPURenderTarget::SDLGPURenderTarget(int x, int y, int width, int height, MultisampleType multiSampleType)
    : NullRenderTarget(x, y, width, height, multiSampleType) {}

#endif
