// Copyright (c) 2016, PG, All rights reserved.
#include "OpenGLRenderTarget.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#include "OpenGLHeaders.h"

#include "OpenGLStateCache.h"
#include "ConVar.h"
#include "Engine.h"
#include "VertexArrayObject.h"
#include "Logging.h"

#if (defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)) && !defined(MCENGINE_PLATFORM_WASM)
#define HAS_MULTISAMPLING
#endif

int OpenGLRenderTarget::iMaxMultiSamples{-1};

void OpenGLRenderTarget::init() {
    debugLog("Building RenderTarget ({}x{}) ...", (int)this->vSize.x, (int)this->vSize.y);

    this->iFrameBuffer = 0;
    this->iRenderTexture = 0;
    this->iDepthBuffer = 0;
    this->iResolveTexture = 0;
    this->iResolveFrameBuffer = 0;

    int numMultiSamples = 2;

    if(iMaxMultiSamples == -1) {
        if((GLVersion.major < 3) || !glTexImage2DMultisample || !glRenderbufferStorageMultisample) {
            iMaxMultiSamples = 0;
        } else {
            glGetIntegerv(GL_MAX_SAMPLES, &iMaxMultiSamples);
        }
    }

    if(iMaxMultiSamples == 0) {
        this->multiSampleType = Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X;
    }

    switch(this->multiSampleType) {
        case Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X:
            break;
        case Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_2X:
            numMultiSamples = 2;
            break;
        case Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_4X:
            numMultiSamples = 4;
            break;
        case Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_8X:
            numMultiSamples = 8;
            break;
        case Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_16X:
            numMultiSamples = 16;
            break;
    }

    if(iMaxMultiSamples < numMultiSamples) {
        numMultiSamples = iMaxMultiSamples;
    }

    // create framebuffer
    glGenFramebuffers(1, &this->iFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, this->iFrameBuffer);
    if(this->iFrameBuffer == 0) {
        engine->showMessageError("RenderTarget Error", "Couldn't glGenFramebuffers() or glBindFramebuffer()!");
        return;
    }

    // create depth buffer
    glGenRenderbuffers(1, &this->iDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, this->iDepthBuffer);
    if(this->iDepthBuffer == 0) {
        engine->showMessageError("RenderTarget Error", "Couldn't glGenRenderBuffers() or glBindRenderBuffer()!");
        return;
    }

    // fill depth buffer
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, numMultiSamples, GL_DEPTH_COMPONENT24, (int)this->vSize.x,
                                         (int)this->vSize.y);
    } else
#endif
    {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (int)this->vSize.x, (int)this->vSize.y);
    }

    // set depth buffer as depth attachment on the framebuffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->iDepthBuffer);

    // create texture
    glGenTextures(1, &this->iRenderTexture);

#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        glBindTexture(isMultiSampled() ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, this->iRenderTexture);
    } else
#endif
    {
        glBindTexture(GL_TEXTURE_2D, this->iRenderTexture);
    }

    if(this->iRenderTexture == 0) {
        engine->showMessageError("RenderTarget Error", "Couldn't glGenTextures() or glBindTexture()!");
        return;
    }

    // fill texture
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        if constexpr(Env::cfg(REND::GL))
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numMultiSamples, GL_RGBA8, (int)this->vSize.x,
                                    (int)this->vSize.y, true);  // use fixed sample locations
        else
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numMultiSamples, GL_RGBA8, (int)this->vSize.x,
                                      (int)this->vSize.y, true);
    } else
#endif
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)this->vSize.x, (int)this->vSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);

        // set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);              // no mipmapping atm
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // disable texture wrap
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // set render texture as color attachment0 on the framebuffer
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, this->iRenderTexture,
                               0);
    } else
#endif
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->iRenderTexture, 0);
    }

    // if multisampled, create resolve framebuffer/texture
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        if(this->iResolveFrameBuffer == 0) {
            // create resolve framebuffer
            glGenFramebuffers(1, &this->iResolveFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, this->iResolveFrameBuffer);
            if(this->iResolveFrameBuffer == 0) {
                engine->showMessageError("RenderTarget Error",
                                         "Couldn't glGenFramebuffers() or glBindFramebuffer() multisampled!");
                return;
            }

            // create resolve texture
            glGenTextures(1, &this->iResolveTexture);
            glBindTexture(GL_TEXTURE_2D, this->iResolveTexture);
            if(this->iResolveTexture == 0) {
                engine->showMessageError("RenderTarget Error",
                                         "Couldn't glGenTextures() or glBindTexture() multisampled!");
                return;
            }

            // set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);  // no mips

            // fill texture
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)this->vSize.x, (int)this->vSize.y, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);

            // set resolve texture as color attachment0 on the resolve framebuffer
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->iResolveTexture, 0);
        }
    }
#endif

    // put this behind a flag because glCheckFramebufferStatus causes unnecessary command queue syncs
    if(cv::debug_opengl.getBool()) {
        // error checking
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(status != GL_FRAMEBUFFER_COMPLETE) {
            engine->showMessageError(
                "RenderTarget Error",
                UString::format("!GL_FRAMEBUFFER_COMPLETE, size = (%ix%i), multisampled = %i, status = %u",
                                (int)this->vSize.x, (int)this->vSize.y, (int)isMultiSampled(), status));
            return;
        }
    }

    // reset bound texture and framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // check if the default framebuffer is active first before setting viewport
    if(OpenGLStateCache::getCurrentFramebuffer() == 0) {
        std::array<int, 4> viewport;  // NOLINT
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        OpenGLStateCache::setCurrentViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

    this->bReady = true;
}

void OpenGLRenderTarget::initAsync() { this->bAsyncReady = true; }

void OpenGLRenderTarget::destroy() {
    if(this->iResolveTexture != 0) glDeleteTextures(1, &this->iResolveTexture);
    if(this->iResolveFrameBuffer != 0) glDeleteFramebuffers(1, &this->iResolveFrameBuffer);
    if(this->iRenderTexture != 0) glDeleteTextures(1, &this->iRenderTexture);
    if(this->iDepthBuffer != 0) glDeleteRenderbuffers(1, &this->iDepthBuffer);
    if(this->iFrameBuffer != 0) glDeleteFramebuffers(1, &this->iFrameBuffer);

    this->iFrameBuffer = 0;
    this->iRenderTexture = 0;
    this->iDepthBuffer = 0;
    this->iResolveTexture = 0;
    this->iResolveFrameBuffer = 0;
}

void OpenGLRenderTarget::enable() {
    if(!this->bReady) return;

    // use the state cache instead of querying OpenGL directly
    this->iFrameBufferBackup = OpenGLStateCache::getCurrentFramebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, this->iFrameBuffer);
    OpenGLStateCache::setCurrentFramebuffer(this->iFrameBuffer);

    OpenGLStateCache::getCurrentViewport(this->iViewportBackup[0], this->iViewportBackup[1], this->iViewportBackup[2],
                                         this->iViewportBackup[3]);

    // set new viewport
    int newViewX = -this->vPos.x;
    int newViewY = (this->vPos.y - g->getResolution().y) + this->vSize.y;
    int newViewWidth = g->getResolution().x;
    int newViewHeight = g->getResolution().y;

    glViewport(newViewX, newViewY, newViewWidth, newViewHeight);

    // update cache
    OpenGLStateCache::setCurrentViewport(newViewX, newViewY, newViewWidth, newViewHeight);

    // clear
    if(cv::debug_rt.getBool())
        glClearColor(0.0f, 0.5f, 0.0f, 0.5f);
    else
        glClearColor(this->clearColor.Rf(), this->clearColor.Gf(), this->clearColor.Bf(), this->clearColor.Af());

    if(this->bClearColorOnDraw || this->bClearDepthOnDraw)
        glClear((this->bClearColorOnDraw ? GL_COLOR_BUFFER_BIT : 0) |
                (this->bClearDepthOnDraw ? GL_DEPTH_BUFFER_BIT : 0));
}

void OpenGLRenderTarget::disable() {
    if(!this->bReady) return;

    // if multisampled, blit content for multisampling into resolve texture
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        // HACKHACK: force disable antialiasing
        g->setAntialiasing(false);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, this->iFrameBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->iResolveFrameBuffer);

        // for multisampled, the sizes MUST be the same! you can't blit from multisampled into non-multisampled or different size
        glBlitFramebuffer(0, 0, (int)this->vSize.x, (int)this->vSize.y, 0, 0, (int)this->vSize.x, (int)this->vSize.y,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        // update cache for the current framebuffer (now 0)
        OpenGLStateCache::setCurrentFramebuffer(0);
    }
#endif

    // restore viewport
    glViewport(this->iViewportBackup[0], this->iViewportBackup[1], this->iViewportBackup[2], this->iViewportBackup[3]);

    // update the cache with restored viewport
    OpenGLStateCache::setCurrentViewport(this->iViewportBackup[0], this->iViewportBackup[1], this->iViewportBackup[2],
                                         this->iViewportBackup[3]);

    // restore framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, this->iFrameBufferBackup);

    // update cache for the restored framebuffer
    OpenGLStateCache::setCurrentFramebuffer(this->iFrameBufferBackup);
}

void OpenGLRenderTarget::bind(unsigned int textureUnit) {
    if(!this->bReady) return;

    this->iTextureUnitBackup = textureUnit;

    // switch texture units before enabling+binding
    glActiveTexture(GL_TEXTURE0 + textureUnit);

    // set texture
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        glBindTexture(GL_TEXTURE_2D, this->iResolveTexture);
    } else
#endif
    {
        glBindTexture(GL_TEXTURE_2D, this->iRenderTexture);
    }
    // needed for legacy support (OpenGLLegacyInterface)
    // DEPRECATED LEGACY
    if constexpr(Env::cfg(REND::GL)) glEnable(GL_TEXTURE_2D);
}

void OpenGLRenderTarget::unbind() {
    if(!this->bReady) return;

    // restore texture unit (just in case) and set to no texture
    glActiveTexture(GL_TEXTURE0 + this->iTextureUnitBackup);
    glBindTexture(GL_TEXTURE_2D, 0);

    // restore default texture unit
    if(this->iTextureUnitBackup != 0) glActiveTexture(GL_TEXTURE0);
}

void OpenGLRenderTarget::blitResolveFrameBufferIntoFrameBuffer(OpenGLRenderTarget *rt) {
#ifdef HAS_MULTISAMPLING
    if(isMultiSampled()) {
        // HACKHACK: force disable antialiasing
        g->setAntialiasing(false);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, this->iResolveFrameBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt->getFrameBuffer());

        glBlitFramebuffer(0, 0, (int)this->vSize.x, (int)this->vSize.y, 0, 0, (int)rt->getWidth(), (int)rt->getHeight(),
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }
#endif
}

void OpenGLRenderTarget::blitFrameBufferIntoFrameBuffer(OpenGLRenderTarget *rt) {
    // HACKHACK: force disable antialiasing
    g->setAntialiasing(false);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, this->iFrameBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt->getFrameBuffer());

    glBlitFramebuffer(0, 0, (int)this->vSize.x, (int)this->vSize.y, 0, 0, (int)rt->getWidth(), (int)rt->getHeight(),
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

#endif
