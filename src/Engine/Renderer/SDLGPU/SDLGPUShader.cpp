//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu HLSL implementation of Shader
//
// $NoKeywords: $sdlgpushader
//===============================================================================//

// TODO: prime full cache on load anyway
// TODO: individually remember m_bConstantBuffersUpToDate per constant buffer
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include "SDLGPUShader.h"

SDLGPUShader::SDLGPUShader(std::string vertexShader, std::string fragmentShader, [[maybe_unused]] bool source)
    : NullShader(), sVsh(std::move(vertexShader)), sFsh(std::move(fragmentShader)) {
}

#endif
