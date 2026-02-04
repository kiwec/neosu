// Copyright (c) 2025, WH, All rights reserved.
#include "OpenGLStateCache.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include "OpenGLHeaders.h"

namespace GLStateCache {
namespace detail {
// init static cache
std::array<int, 4> current_viewport{};
std::array<unsigned int, 4> current_state_array{};

unsigned int current_program{INT_MAX};
unsigned int current_framebuffer{INT_MAX};
unsigned int current_arraybuffer{UINT_MAX};

}  // namespace detail
using namespace detail;

void initialize() {
    if(current_program != INT_MAX) return;

    // one-time initialization of cache from actual GL state
    refresh();
}

void refresh() {
    // only do the expensive query when necessary
    glGetIntegerv(GL_VIEWPORT, current_viewport.data());

    glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&current_program));
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&current_framebuffer));

    // glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)&iCurrentArrayBuffer);
}

void bindFramebuffer(unsigned int GLFramebuffer) {
    if(current_framebuffer != GLFramebuffer) {
        current_framebuffer = GLFramebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, GLFramebuffer);
    }
}

void setViewport(const std::array<int, 4> &vp) {
    if(current_viewport != vp) {
        current_viewport = vp;
        glViewport(vp[0], vp[1], vp[2], vp[3]);
    }
}

void bindArrayBuffer(unsigned int GLbuffer) {
    if(current_arraybuffer != GLbuffer) {
        current_arraybuffer = GLbuffer;
        glBindBuffer(GL_ARRAY_BUFFER, GLbuffer);
    }
}

// legacy client state functions - only available in desktop OpenGL, not GLES
#if defined(MCENGINE_FEATURE_OPENGL) && !defined(MCENGINE_FEATURE_GLES32)
void enableClientState(unsigned int GLarray) {
    if(GLarray != GL_VERTEX_ARRAY && GLarray != GL_TEXTURE_COORD_ARRAY && GLarray != GL_COLOR_ARRAY &&
       GLarray != GL_NORMAL_ARRAY) {
        return;
    }

    const auto &it = std::ranges::find(current_state_array, GLarray);
    if(it != current_state_array.end()) {
        return;  // already enabled
    }

    const auto &zero = std::ranges::find(current_state_array, 0);
    assert(zero != current_state_array.end());

    *zero = GLarray;
    glEnableClientState(GLarray);
}

void disableClientState(unsigned int GLarray) {
    if(GLarray != GL_VERTEX_ARRAY && GLarray != GL_TEXTURE_COORD_ARRAY && GLarray != GL_COLOR_ARRAY &&
       GLarray != GL_NORMAL_ARRAY) {
        return;
    }

    const auto &array = std::ranges::find(current_state_array, GLarray);
    if(array != current_state_array.end()) {
        *array = 0;
        glDisableClientState(GLarray);
    }
}
#endif
}  // namespace GLStateCache

#endif
