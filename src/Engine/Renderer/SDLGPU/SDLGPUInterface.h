//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		raw SDL_gpu graphics interface
//
// $NoKeywords: $sdlgpui
//===============================================================================//

#pragma once
#ifndef SDLGPUINTERFACE_H
#define SDLGPUINTERFACE_H
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include "NullGraphics.h"

typedef struct SDL_Window SDL_Window;

class SDLGPUInterface final : public NullGraphics {
    NOCOPY_NOMOVE(SDLGPUInterface)
   public:
    SDLGPUInterface(SDL_Window* window);
    ~SDLGPUInterface() override = default;

    // factory
    Image *createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) override;
    Image *createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) override;
    RenderTarget *createRenderTarget(int x, int y, int width, int height, MultisampleType msType) override;
    Shader *createShaderFromFile(std::string vertexShaderFilePath, std::string fragmentShaderFilePath) override;
    Shader *createShaderFromSource(std::string vertexShader, std::string fragmentShader) override;
    VertexArrayObject *createVertexArrayObject(DrawPrimitive primitive, DrawUsageType usage,
                                               bool keepInSystemMemory) override;

   private:
    SDL_Window *m_window;
};

#endif

#endif
