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
        debugLog("gladLoadGL() error");
        engine->showMessageErrorFatal("OpenGL Error", "Couldn't gladLoadGL()!\nThe engine will exit now.");
        engine->shutdown();
        return;
    }
    debugLog("gladLoadGL() version: {:d}.{:d}, EGL: {:s}", GLVersion.major, GLVersion.minor,
             !!SDL_EGL_GetCurrentDisplay() ? "true" : "false");
#endif
    debugLog("GL_VERSION string: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

    const auto &argMap = env->getLaunchArgs();
    if(Env::cfg(BUILD::DEBUG) || argMap.contains("-info") || argMap.contains("-print") ||
       argMap.contains("-printinfo")) {
        dumpGLContextInfo();
    }
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

std::unordered_map<DrawPrimitive, int> SDLGLInterface::primitiveToOpenGLMap = {
    {DrawPrimitive::PRIMITIVE_LINES, GL_LINES},
    {DrawPrimitive::PRIMITIVE_LINE_STRIP, GL_LINE_STRIP},
    {DrawPrimitive::PRIMITIVE_TRIANGLES, GL_TRIANGLES},
    {DrawPrimitive::PRIMITIVE_TRIANGLE_FAN, GL_TRIANGLE_FAN},
    {DrawPrimitive::PRIMITIVE_TRIANGLE_STRIP, GL_TRIANGLE_STRIP},
    {DrawPrimitive::PRIMITIVE_QUADS, Env::cfg(REND::GLES32) ? 0 : GL_QUADS},
};

std::unordered_map<DrawCompareFunc, int> SDLGLInterface::compareFuncToOpenGLMap = {
    {DrawCompareFunc::COMPARE_FUNC_NEVER, GL_NEVER},
    {DrawCompareFunc::COMPARE_FUNC_LESS, GL_LESS},
    {DrawCompareFunc::COMPARE_FUNC_EQUAL, GL_EQUAL},
    {DrawCompareFunc::COMPARE_FUNC_LESSEQUAL, GL_LEQUAL},
    {DrawCompareFunc::COMPARE_FUNC_GREATER, GL_GREATER},
    {DrawCompareFunc::COMPARE_FUNC_NOTEQUAL, GL_NOTEQUAL},
    {DrawCompareFunc::COMPARE_FUNC_GREATEREQUAL, GL_GEQUAL},
    {DrawCompareFunc::COMPARE_FUNC_ALWAYS, GL_ALWAYS},
};

std::unordered_map<DrawUsageType, unsigned int> SDLGLInterface::usageToOpenGLMap = {
    {DrawUsageType::USAGE_STATIC, GL_STATIC_DRAW},
    {DrawUsageType::USAGE_DYNAMIC, GL_DYNAMIC_DRAW},
    {DrawUsageType::USAGE_STREAM, GL_STREAM_DRAW},
};

void SDLGLInterface::dumpGLContextInfo() {
    std::string info = "Initial OpenGL Context:\n";
    int current;
    for(int i = 0; auto [enm, str] : std::array<std::pair<SDL_GLAttr, std::string_view>, 28>{
                       {{SDL_GL_RED_SIZE, "GL_RED_SIZE"sv},
                        {SDL_GL_GREEN_SIZE, "GL_GREEN_SIZE"sv},
                        {SDL_GL_BLUE_SIZE, "GL_BLUE_SIZE"sv},
                        {SDL_GL_ALPHA_SIZE, "GL_ALPHA_SIZE"sv},
                        {SDL_GL_BUFFER_SIZE, "GL_BUFFER_SIZE"sv},
                        {SDL_GL_DOUBLEBUFFER, "GL_DOUBLEBUFFER"sv},
                        {SDL_GL_DEPTH_SIZE, "GL_DEPTH_SIZE"sv},
                        {SDL_GL_STENCIL_SIZE, "GL_STENCIL_SIZE"sv},
                        {SDL_GL_ACCUM_RED_SIZE, "GL_ACCUM_RED_SIZE"sv},
                        {SDL_GL_ACCUM_GREEN_SIZE, "GL_ACCUM_GREEN_SIZE"sv},
                        {SDL_GL_ACCUM_BLUE_SIZE, "GL_ACCUM_BLUE_SIZE"sv},
                        {SDL_GL_ACCUM_ALPHA_SIZE, "GL_ACCUM_ALPHA_SIZE"sv},
                        {SDL_GL_STEREO, "GL_STEREO"sv},
                        {SDL_GL_MULTISAMPLEBUFFERS, "GL_MULTISAMPLEBUFFERS"sv},
                        {SDL_GL_MULTISAMPLESAMPLES, "GL_MULTISAMPLESAMPLES"sv},
                        {SDL_GL_ACCELERATED_VISUAL, "GL_ACCELERATED_VISUAL"sv},
                        {SDL_GL_RETAINED_BACKING, "GL_RETAINED_BACKING"sv},
                        {SDL_GL_CONTEXT_MAJOR_VERSION, "GL_CONTEXT_MAJOR_VERSION"sv},
                        {SDL_GL_CONTEXT_MINOR_VERSION, "GL_CONTEXT_MINOR_VERSION"sv},
                        {SDL_GL_CONTEXT_FLAGS, "GL_CONTEXT_FLAGS"sv},
                        {SDL_GL_CONTEXT_PROFILE_MASK, "GL_CONTEXT_PROFILE_MASK"sv},
                        {SDL_GL_SHARE_WITH_CURRENT_CONTEXT, "GL_SHARE_WITH_CURRENT_CONTEXT"sv},
                        {SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, "GL_FRAMEBUFFER_SRGB_CAPABLE"sv},
                        {SDL_GL_CONTEXT_RELEASE_BEHAVIOR, "GL_CONTEXT_RELEASE_BEHAVIOR"sv},
                        {SDL_GL_CONTEXT_RESET_NOTIFICATION, "GL_CONTEXT_RESET_NOTIFICATION"sv},
                        {SDL_GL_CONTEXT_NO_ERROR, "GL_CONTEXT_NO_ERROR"sv},
                        {SDL_GL_FLOATBUFFERS, "GL_FLOATBUFFERS"sv},
                        {SDL_GL_EGL_PLATFORM, "GL_EGL_PLATFORM"sv}}}) {
        if(SDL_GL_GetAttribute(enm, &current)) {
            i++;
            info += fmt::format(" {:<30}: {:<3}", str, current);
            if(!(i % 4)) info.push_back('\n');
        }
    }

    info.pop_back();  // remove trailing newline
    Logger::logRaw(info);
}

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
    if(!g || !g.get() || !glDebugMessageCallbackARB) return;
    if(on) {
        glEnable(GL_DEBUG_OUTPUT);
    } else {
        glDisable(GL_DEBUG_OUTPUT);
    }
}

void APIENTRY SDLGLInterface::glDebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                        const GLchar *message, const void * /*userParam*/) {
    Logger::logRaw("[GLDebugCB]");
    Logger::logRaw("    message: {}", std::string(message, length));
    Logger::logRaw("    time: {:.4f}", engine->getTime());
    Logger::logRaw("    id: {}", id);
    Logger::logRaw("    source: {}", glDebugSourceString(source));
    Logger::logRaw("    type: {}", glDebugTypeString(type));
    Logger::logRaw("    severity: {}", glDebugSeverityString(severity));
}

#endif
