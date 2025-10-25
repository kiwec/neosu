// Copyright (c) 2025, WH, All rights reserved.
#include "OpenGLStateCache.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include "OpenGLHeaders.h"

// init static cache
std::array<int, 4> OpenGLStateCache::iViewport{};
std::array<unsigned int, 4> OpenGLStateCache::iEnabledStateArray{};

unsigned int OpenGLStateCache::iCurrentProgram{INT_MAX};
unsigned int OpenGLStateCache::iCurrentFramebuffer{INT_MAX};

unsigned int OpenGLStateCache::iCurrentArrayBuffer{UINT_MAX};

void OpenGLStateCache::initialize() {
    if(OpenGLStateCache::iCurrentProgram != INT_MAX) return;

    // one-time initialization of cache from actual GL state
    OpenGLStateCache::refresh();
}

void OpenGLStateCache::refresh() {
    // only do the expensive query when necessary
    glGetIntegerv(GL_VIEWPORT, OpenGLStateCache::iViewport.data());

    glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&OpenGLStateCache::iCurrentProgram));
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&OpenGLStateCache::iCurrentFramebuffer));

    // glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *)&OpenGLStateCache::iCurrentArrayBuffer);
}

void OpenGLStateCache::setCurrentProgram(unsigned int program) { OpenGLStateCache::iCurrentProgram = program; }

unsigned int OpenGLStateCache::getCurrentProgram() { return OpenGLStateCache::iCurrentProgram; }

void OpenGLStateCache::setCurrentFramebuffer(unsigned int framebuffer) {
    OpenGLStateCache::iCurrentFramebuffer = framebuffer;
}

unsigned int OpenGLStateCache::getCurrentFramebuffer() { return OpenGLStateCache::iCurrentFramebuffer; }

void OpenGLStateCache::setCurrentViewport(int x, int y, int width, int height) {
    OpenGLStateCache::iViewport[0] = x;
    OpenGLStateCache::iViewport[1] = y;
    OpenGLStateCache::iViewport[2] = width;
    OpenGLStateCache::iViewport[3] = height;
}

void OpenGLStateCache::getCurrentViewport(int &x, int &y, int &width, int &height) {
    x = OpenGLStateCache::iViewport[0];
    y = OpenGLStateCache::iViewport[1];
    width = OpenGLStateCache::iViewport[2];
    height = OpenGLStateCache::iViewport[3];
}

void OpenGLStateCache::bindArrayBuffer(unsigned int GLbuffer) {
    if(OpenGLStateCache::iCurrentArrayBuffer != GLbuffer) {
        OpenGLStateCache::iCurrentArrayBuffer = GLbuffer;
        glBindBuffer(GL_ARRAY_BUFFER, GLbuffer);
    }
}

void OpenGLStateCache::enableClientState(unsigned int GLarray) {
    if(GLarray != GL_VERTEX_ARRAY && GLarray != GL_TEXTURE_COORD_ARRAY && GLarray != GL_COLOR_ARRAY &&
       GLarray != GL_NORMAL_ARRAY) {
        return;
    }

    for(auto &array : OpenGLStateCache::iEnabledStateArray) {
        if(array == GLarray) {
            // already enabled
            return;
        } else if(array == 0) {
            // wasn't enabled
            array = GLarray;
            glEnableClientState(GLarray);
            return;
        } else {
            // search for 0 or end
            continue;
        }
    }
}

void OpenGLStateCache::disableClientState(unsigned int GLarray) {
    if(GLarray != GL_VERTEX_ARRAY && GLarray != GL_TEXTURE_COORD_ARRAY && GLarray != GL_COLOR_ARRAY &&
       GLarray != GL_NORMAL_ARRAY) {
        return;
    }

    const auto &array = std::ranges::find(OpenGLStateCache::iEnabledStateArray, GLarray);
    if(array != OpenGLStateCache::iEnabledStateArray.end()) {
        *array = 0;
        glDisableClientState(GLarray);
    }
}

#endif
