//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu baking support for vao
//
// $NoKeywords: $sdlgpuvao
//===============================================================================//

#pragma once
#ifndef SDLGPUVERTEXARRAYOBJECT_H
#define SDLGPUVERTEXARRAYOBJECT_H

#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include "NullVertexArrayObject.h"

class SDLGPUVertexArrayObject final : public NullVertexArrayObject {
    NOCOPY_NOMOVE(SDLGPUVertexArrayObject)
   public:
    SDLGPUVertexArrayObject(DrawPrimitive primitive = DrawPrimitive::TRIANGLES,
                               DrawUsageType usage = DrawUsageType::STATIC,
                               bool keepInSystemMemory = false);
    ~SDLGPUVertexArrayObject() override { destroy(); }
};

#endif

#endif
