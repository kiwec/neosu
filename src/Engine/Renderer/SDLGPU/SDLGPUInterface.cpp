//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		raw SDL_gpu graphics interface
//
// $NoKeywords: $sdlgpui
//===============================================================================//
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU
#include "SDLGPUInterface.h"
#include "SDLGPUShader.h"
#include "SDLGPUImage.h"
#include "SDLGPURenderTarget.h"
#include "SDLGPUVertexArrayObject.h"

SDLGPUInterface::SDLGPUInterface(SDL_Window *window) : NullGraphics(), m_window(window) { (void)m_window; }

// factory
Image *SDLGPUInterface::createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) {
    return new SDLGPUImage(std::move(filePath), mipmapped, keepInSystemMemory);
}

Image *SDLGPUInterface::createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) {
    return new SDLGPUImage(width, height, mipmapped, keepInSystemMemory);
}

RenderTarget *SDLGPUInterface::createRenderTarget(int x, int y, int width, int height, MultisampleType msType) {
    return new SDLGPURenderTarget(x, y, width, height, msType);
}

Shader *SDLGPUInterface::createShaderFromFile(std::string vertexShaderFilePath, std::string fragmentShaderFilePath) {
    return new SDLGPUShader(vertexShaderFilePath, fragmentShaderFilePath, false);
}

Shader *SDLGPUInterface::createShaderFromSource(std::string vertexShader, std::string fragmentShader) {
    return new SDLGPUShader(vertexShader, fragmentShader);
}

VertexArrayObject *SDLGPUInterface::createVertexArrayObject(DrawPrimitive primitive, DrawUsageType usage,
                                                            bool keepInSystemMemory) {
    return new SDLGPUVertexArrayObject(primitive, usage, keepInSystemMemory);
}

#endif
