//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu baking support for vao
//
// $NoKeywords: $sdlgpuvao
//===============================================================================//
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include "SDLGPUVertexArrayObject.h"

SDLGPUVertexArrayObject::SDLGPUVertexArrayObject(DrawPrimitive primitive, DrawUsageType usage, bool keepInSystemMemory)
    : NullVertexArrayObject(primitive, usage, keepInSystemMemory) {}

#endif
