#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef OPENGLSTATECACHE_H
#define OPENGLSTATECACHE_H

#include "BaseEnvironment.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)
#include <array>
#include <climits>

namespace GLStateCache {
namespace detail {
// cache
extern std::array<int, 4> current_viewport;
extern std::array<unsigned int, 4> current_state_array;

extern unsigned int current_program;
extern unsigned int current_framebuffer;

extern unsigned int current_arraybuffer;
}  // namespace detail

// program state
inline void setCurrentProgram(unsigned int program) { detail::current_program = program; }
[[nodiscard]] inline unsigned int getCurrentProgram() { return detail::current_program; }

// framebuffer state
void bindFramebuffer(unsigned int framebuffer);
[[nodiscard]] inline unsigned int getCurrentFramebuffer() { return detail::current_framebuffer; }

// viewport state
void setViewport(const std::array<int, 4> &vp);
[[nodiscard]] inline const std::array<int, 4> &getCurrentViewport() { return detail::current_viewport; }

inline void setViewport(int x, int y, int width, int height) {
    return setViewport({x, y, width, height});
};

void bindArrayBuffer(unsigned int GLbuffer);

void enableClientState(unsigned int GLarray);
void disableClientState(unsigned int GLarray);

// initialize cache with actual GL states (once at startup)
void initialize();

// force a refresh of cached states from actual GL state (expensive, avoid)
void refresh();
};  // namespace GLStateCache

#endif

#endif
