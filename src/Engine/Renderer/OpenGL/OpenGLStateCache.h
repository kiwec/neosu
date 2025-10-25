#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef OPENGLSTATECACHE_H
#define OPENGLSTATECACHE_H

#include "BaseEnvironment.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)
#include <array>

class OpenGLStateCache final {
   public:
    // entirely static class so no ctors/dtors necessary
    OpenGLStateCache() = delete;
    ~OpenGLStateCache() = delete;

    OpenGLStateCache &operator=(const OpenGLStateCache &) = delete;
    OpenGLStateCache &operator=(OpenGLStateCache &&) = delete;
    OpenGLStateCache(const OpenGLStateCache &) = delete;
    OpenGLStateCache(OpenGLStateCache &&) = delete;

    // program state
    static void setCurrentProgram(unsigned int program);
    [[nodiscard]] static unsigned int getCurrentProgram();

    // framebuffer state
    static void setCurrentFramebuffer(unsigned int framebuffer);
    [[nodiscard]] static unsigned int getCurrentFramebuffer();

    // viewport state
    static void setCurrentViewport(int x, int y, int width, int height);
    static void getCurrentViewport(int &x, int &y, int &width, int &height);

    static inline void setCurrentViewport(const std::array<int, 4> &vp) { iViewport = vp; }
    [[nodiscard]] static inline const std::array<int, 4> &getCurrentViewport() { return iViewport; }

    static void bindArrayBuffer(unsigned int GLbuffer);

    static void enableClientState(unsigned int GLarray);
    static void disableClientState(unsigned int GLarray);

    // initialize cache with actual GL states (once at startup)
    static void initialize();

    // force a refresh of cached states from actual GL state (expensive, avoid)
    static void refresh();

   private:
    // cache
    static std::array<int, 4> iViewport;
    static std::array<unsigned int, 4> iEnabledStateArray;

    static unsigned int iCurrentProgram;
    static unsigned int iCurrentFramebuffer;

    static unsigned int iCurrentArrayBuffer;
};

#endif

#endif
