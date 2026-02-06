#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef SDLGLINTERFACE_H
#define SDLGLINTERFACE_H

#include "BaseEnvironment.h"

#if defined(MCENGINE_FEATURE_GLES32) || defined(MCENGINE_FEATURE_OPENGL)

#ifdef MCENGINE_FEATURE_OPENGL
#include "OpenGLInterface.h"
#include "OpenGLVertexArrayObject.h"
#include "OpenGLShader.h"

using BackendGLInterface = OpenGLInterface;
using BackendGLVAO = OpenGLVertexArrayObject;
using BackendGLShader = OpenGLShader;
#elif defined(MCENGINE_FEATURE_GLES32)
#include "OpenGLES32Interface.h"
#include "OpenGLES32VertexArrayObject.h"
#include "OpenGLES32Shader.h"

using BackendGLInterface = OpenGLES32Interface;
using BackendGLVAO = OpenGLES32VertexArrayObject;
using BackendGLShader = OpenGLES32Shader;
#endif

#ifndef GLAPIENTRY
#ifdef APIENTRY
#define GLAPIENTRY APIENTRY
#elif defined(MCENGINE_PLATFORM_WINDOWS)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
#endif

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLchar = char;
using GLsizei = int;

#include "OpenGLSync.h"

typedef struct SDL_Window SDL_Window;
class SDLGLInterface final : public BackendGLInterface {
    friend class Environment;
    friend class OpenGLInterface;
    friend class OpenGLVertexArrayObject;
    friend class OpenGLShader;
    friend class OpenGLES32Interface;
    friend class OpenGLES32VertexArrayObject;
    friend class OpenGLES32Shader;
    friend class OpenGLShader;

   public:
    SDLGLInterface(SDL_Window *window);

    // scene
    void beginScene() override;
    void endScene() override;

    // device settings
    void setVSync(bool vsync) override;

    // device info
    UString getVendor() override;
    UString getModel() override;
    UString getVersion() override;
    int getVRAMRemaining() override;
    int getVRAMTotal() override;

    // debugging
    static void setGLLog(bool on);
    static void GLAPIENTRY glDebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                     const GLchar *message, const void * /*userParam*/);

   protected:
    static std::unordered_map<DrawPrimitive, int> primitiveToOpenGLMap;
    static std::unordered_map<DrawCompareFunc, int> compareFuncToOpenGLMap;
    static std::unordered_map<DrawUsageType, unsigned int> usageToOpenGLMap;

   private:
    static void load();
    static void unload();
    static void dumpGLContextInfo();

    SDL_Window *window;

    // frame queue management
    std::unique_ptr<OpenGLSync> syncobj;
};

#else
class SDLGLInterface {};
#endif

#endif
