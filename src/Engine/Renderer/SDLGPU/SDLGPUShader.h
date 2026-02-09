//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu HLSL implementation of Shader
//
// $NoKeywords: $sdlgpushader
//===============================================================================//

#pragma once

#ifndef SDLGPUSHADER_H
#define SDLGPUSHADER_H
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include "NullShader.h"

#include <string>

class SDLGPUShader final : public NullShader {
    NOCOPY_NOMOVE(SDLGPUShader);

   public:
    SDLGPUShader(std::string vertexShader, std::string fragmentShader, bool source = true);
    ~SDLGPUShader() override { destroy(); }

private:
    std::string sVsh;
    std::string sFsh;
};

#endif

#endif
