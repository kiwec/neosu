#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef SDLGLINTERFACE_H
#define SDLGLINTERFACE_H

#include "BaseEnvironment.h"

#if defined(MCENGINE_FEATURE_GLES32) || defined(MCENGINE_FEATURE_OPENGL)

#include "OpenGLHeaders.h"

class OpenGLSync;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_GLContextState *SDL_GLContext;

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
    ~SDLGLInterface() override;

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
    static void setLog(bool on);
    static void APIENTRY glDebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                   const GLchar *message, const void * /*userParam*/);

   protected:
    static std::unordered_map<Graphics::PRIMITIVE, int> primitiveToOpenGLMap;
    static std::unordered_map<Graphics::COMPARE_FUNC, int> compareFuncToOpenGLMap;
    static std::unordered_map<Graphics::USAGE_TYPE, unsigned int> usageToOpenGLMap;

   private:
    static void load();
    SDL_Window *window;
    SDL_GLContext context;

    // frame queue management
    std::unique_ptr<OpenGLSync> syncobj;
};

#else
class SDLGLInterface {};
#endif

#endif
