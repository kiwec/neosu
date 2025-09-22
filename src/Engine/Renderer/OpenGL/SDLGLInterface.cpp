// Copyright (c) 2025, WH, All rights reserved.
#include "SDLGLInterface.h"

#if defined(MCENGINE_FEATURE_GLES32) || defined(MCENGINE_FEATURE_OPENGL)

#include <SDL3/SDL_video.h>

#include "Engine.h"
#include "OpenGLSync.h"
#include "Logging.h"
#include "ConVar.h"

// resolve GL functions (static, called before construction)
void SDLGLInterface::load() {
#ifndef MCENGINE_PLATFORM_WASM
    if(!gladLoadGL()) {
        debugLog("gladLoadGL() error\n");
        engine->showMessageErrorFatal("OpenGL Error", "Couldn't gladLoadGL()!\nThe engine will exit now.");
        engine->shutdown();
        return;
    }
    debugLog("gladLoadGL() version: {:d}.{:d}, EGL: {:s}\n", GLVersion.major, GLVersion.minor,
             !!SDL_EGL_GetCurrentDisplay() ? "true" : "false");
#endif
    debugLog("GL_VERSION string: {}\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
}

SDLGLInterface::SDLGLInterface(SDL_Window *window)
    : BackendGLInterface(), window(window), syncobj(std::make_unique<OpenGLSync>()) {}

void SDLGLInterface::beginScene() {
    // block on frame queue (if enabled)
    this->syncobj->begin();

    BackendGLInterface::beginScene();
}

void SDLGLInterface::endScene() {
    BackendGLInterface::endScene();

    // create sync obj for the gl commands this frame (if enabled)
    this->syncobj->end();

    SDL_GL_SwapWindow(this->window);
}

void SDLGLInterface::setVSync(bool vsync) { SDL_GL_SetSwapInterval(vsync ? 1 : 0); }

UString SDLGLInterface::getVendor() {
    static const GLubyte *vendor = nullptr;
    if(!vendor) vendor = glGetString(GL_VENDOR);
    return reinterpret_cast<const char *>(vendor);
}

UString SDLGLInterface::getModel() {
    static const GLubyte *model = nullptr;
    if(!model) model = glGetString(GL_RENDERER);
    return reinterpret_cast<const char *>(model);
}

UString SDLGLInterface::getVersion() {
    static const GLubyte *version = nullptr;
    if(!version) version = glGetString(GL_VERSION);
    return reinterpret_cast<const char *>(version);
}

int SDLGLInterface::getVRAMTotal() {
    static GLint totalMem[4]{-1, -1, -1, -1};

    if(totalMem[0] == -1) {
        glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, totalMem);
        if(!(totalMem[0] > 0 && glGetError() != GL_INVALID_ENUM)) totalMem[0] = 0;
    }
    return totalMem[0];
}

int SDLGLInterface::getVRAMRemaining() {
    GLint nvidiaMemory[4]{-1, -1, -1, -1};
    GLint atiMemory[4]{-1, -1, -1, -1};

    glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, nvidiaMemory);

    if(nvidiaMemory[0] > 0) return nvidiaMemory[0];

    glGetIntegerv(TEXTURE_FREE_MEMORY_ATI, atiMemory);
    return atiMemory[0];
}

std::unordered_map<Graphics::PRIMITIVE, int> SDLGLInterface::primitiveToOpenGLMap = {
    {Graphics::PRIMITIVE::PRIMITIVE_LINES, GL_LINES},
    {Graphics::PRIMITIVE::PRIMITIVE_LINE_STRIP, GL_LINE_STRIP},
    {Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES, GL_TRIANGLES},
    {Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN, GL_TRIANGLE_FAN},
    {Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_STRIP, GL_TRIANGLE_STRIP},
    {Graphics::PRIMITIVE::PRIMITIVE_QUADS, Env::cfg(REND::GLES32) ? 0 : GL_QUADS},
};

std::unordered_map<Graphics::COMPARE_FUNC, int> SDLGLInterface::compareFuncToOpenGLMap = {
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_NEVER, GL_NEVER},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_LESS, GL_LESS},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_EQUAL, GL_EQUAL},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_LESSEQUAL, GL_LEQUAL},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_GREATER, GL_GREATER},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_NOTEQUAL, GL_NOTEQUAL},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_GREATEREQUAL, GL_GEQUAL},
    {Graphics::COMPARE_FUNC::COMPARE_FUNC_ALWAYS, GL_ALWAYS},
};

std::unordered_map<Graphics::USAGE_TYPE, unsigned int> SDLGLInterface::usageToOpenGLMap = {
    {Graphics::USAGE_TYPE::USAGE_STATIC, GL_STATIC_DRAW},
    {Graphics::USAGE_TYPE::USAGE_DYNAMIC, GL_DYNAMIC_DRAW},
    {Graphics::USAGE_TYPE::USAGE_STREAM, GL_STREAM_DRAW},
};

namespace {
std::string glDebugSourceString(GLenum source) {
    switch(source) {
        case GL_DEBUG_SOURCE_API:
            return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            return "WINDOW_SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            return "SHADER_COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            return "THIRD_PARTY";
        case GL_DEBUG_SOURCE_APPLICATION:
            return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER:
            return "OTHER";
        default:
            return fmt::format("{:04x}", source);
    }
}
std::string glDebugTypeString(GLenum type) {
    switch(type) {
        case GL_DEBUG_TYPE_ERROR:
            return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY:
            return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE:
            return "PERFORMANCE";
        case GL_DEBUG_TYPE_OTHER:
            return "OTHER";
        case GL_DEBUG_TYPE_MARKER:
            return "MARKER";
        case GL_DEBUG_TYPE_PUSH_GROUP:
            return "PUSH_GROUP";
        case GL_DEBUG_TYPE_POP_GROUP:
            return "POP_GROUP";
        default:
            return fmt::format("{:04x}", type);
    }
}
std::string glDebugSeverityString(GLenum severity) {
    switch(severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            return "HIGH";
        case GL_DEBUG_SEVERITY_MEDIUM:
            return "MEDIUM";
        case GL_DEBUG_SEVERITY_LOW:
            return "LOW";
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "NOTIFICATION";
        default:
            return fmt::format("{:04x}", severity);
    }
}

}  // namespace

namespace cv {
static ConVar debug_opengl_v("debug_opengl_v", false, CLIENT | HIDDEN,
                             [](float val) -> void { SDLGLInterface::setLog(!!static_cast<int>(val)); });
}  // namespace cv

void SDLGLInterface::setLog(bool on) {
    if(!g || !g.get() || !GLAD_GL_ARB_debug_output) return;
    if(on) {
        glEnable(GL_DEBUG_OUTPUT);
    } else {
        glDisable(GL_DEBUG_OUTPUT);
    }
}

void APIENTRY SDLGLInterface::glDebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                        const GLchar *message, const void * /*userParam*/) {
    Logger::logRaw("[GLDebugCB]\n");
    Logger::logRaw("    message: {}\n", std::string(message, length));
    Logger::logRaw("    time: {:.4f}\n", engine->getTime());
    Logger::logRaw("    id: {}\n", id);
    Logger::logRaw("    source: {}\n", glDebugSourceString(source));
    Logger::logRaw("    type: {}\n", glDebugTypeString(type));
    Logger::logRaw("    severity: {}\n", glDebugSeverityString(severity));
}

#endif
