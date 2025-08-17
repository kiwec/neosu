#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#ifndef LEGACYOPENGLINTERFACE_H
#define LEGACYOPENGLINTERFACE_H

#include "cbase.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "OpenGLSync.h"

class Image;

class OpenGLLegacyInterface : public Graphics {
   public:
    OpenGLLegacyInterface();
    ~OpenGLLegacyInterface() override = default;

    // scene
    void beginScene() final;
    void endScene() override;

    // depth buffer
    void clearDepthBuffer() final;

    // color
    void setColor(Color color) final;
    void setAlpha(float alpha) final;

    // 2d primitive drawing
    void drawPixels(int x, int y, int width, int height, Graphics::DRAWPIXELS_TYPE type, const void *pixels) final;
    void drawPixel(int x, int y) final;
    void drawLinef(float x1, float y1, float x2, float y2) final;
    void drawRectf(float x, float y, float width, float height, bool withColor = false, Color top = -1,
                   Color right = -1, Color bottom = -1, Color left = -1) final;
    void fillRectf(float x, float y, float width, float height) final;

    void fillRoundedRect(int x, int y, int width, int height, int radius) final;
    void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                      Color bottomLeftColor, Color bottomRightColor) final;

    void drawQuad(int x, int y, int width, int height) final;
    void drawQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft, Color topLeftColor,
                  Color topRightColor, Color bottomRightColor, Color bottomLeftColor) final;

    // 2d resource drawing
    void drawImage(Image *image, AnchorPoint anchor = AnchorPoint::CENTER, float edgeSoftness = 0.0f, McRect clipRect = {}) final;
    void drawString(McFont *font, const UString &text) final;

    // 3d type drawing
    void drawVAO(VertexArrayObject *vao) final;

    // DEPRECATED: 2d clipping
    void setClipRect(McRect clipRect) final;
    void pushClipRect(McRect clipRect) final;
    void popClipRect() final;

    // stencil
    void pushStencil() final;
    void fillStencil(bool inside) final;
    void popStencil() final;

    // renderer settings
    void setClipping(bool enabled) final;
    void setAlphaTesting(bool enabled) final;
    void setAlphaTestFunc(COMPARE_FUNC alphaFunc, float ref) final;
    void setBlending(bool enabled) final;
    void setBlendMode(BLEND_MODE blendMode) final;
    void setDepthBuffer(bool enabled) final;
    void setCulling(bool culling) final;
    void setAntialiasing(bool aa) final;
    void setWireframe(bool enabled) final;
    void setLineWidth(float width) final;

    // renderer actions
    void flush() final;
    std::vector<u8> getScreenshot(bool withAlpha = false) final;

    // renderer info
    vec2 getResolution() const final { return this->vResolution; }

    // callbacks
    void onResolutionChange(vec2 newResolution) final;

    // factory
    Image *createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) final;
    Image *createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) final;
    RenderTarget *createRenderTarget(int x, int y, int width, int height,
                                     Graphics::MULTISAMPLE_TYPE multiSampleType) final;
    Shader *createShaderFromFile(std::string vertexShaderFilePath,
                                 std::string fragmentShaderFilePath) final;                      // DEPRECATED
    Shader *createShaderFromSource(std::string vertexShader, std::string fragmentShader) final;  // DEPRECATED
    VertexArrayObject *createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage,
                                               bool keepInSystemMemory) final;

   protected:
    void onTransformUpdate(Matrix4 &projectionMatrix, Matrix4 &worldMatrix) final;

   private:
    std::unique_ptr<Shader> smoothClipShader{nullptr};
    void initSmoothClipShader();

    // renderer
    bool bInScene{false};
    vec2 vResolution{0.f};

    // persistent vars
    bool bAntiAliasing{true};
    Color color{0xffffffff};
    //float fZ{1};
    //float fClearZ{1};

    // synchronization
    std::unique_ptr<OpenGLSync> syncobj{nullptr};

    // clipping
    std::stack<McRect> clipRectStack;
};

#else
class OpenGLLegacyInterface : public Graphics {};
#endif

#endif
