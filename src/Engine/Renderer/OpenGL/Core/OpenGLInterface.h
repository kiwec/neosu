#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#ifndef LEGACYOPENGLINTERFACE_H
#define LEGACYOPENGLINTERFACE_H

#include "config.h"

#ifdef MCENGINE_FEATURE_OPENGL
#include "Graphics.h"

class Image;

class OpenGLInterface : public Graphics {
    NOCOPY_NOMOVE(OpenGLInterface)
   public:
    OpenGLInterface();
    ~OpenGLInterface() override;

    // scene
    void beginScene() override;
    void endScene() override;

    // depth buffer
    void clearDepthBuffer() final;

    // color
    void setColor(Color color) final;
    inline void setAlpha(float alpha) final {
        if(this->color.Af() == alpha) return;
        Color newColor = this->color;
        this->setColor(newColor.setA(alpha));
    }

    // 2d primitive drawing
    void drawPixels(int x, int y, int width, int height, DrawPixelsType type, const void *pixels) final;
    void drawPixel(int x, int y) final;
    void drawLinef(float x1, float y1, float x2, float y2) final;
    void drawRectf(const RectOptions &opt) final;
    void fillRectf(float x, float y, float width, float height) final;

    void fillRoundedRect(int x, int y, int width, int height, int radius) final;
    void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                      Color bottomLeftColor, Color bottomRightColor) final;

    void drawQuad(int x, int y, int width, int height) final;
    void drawQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft, Color topLeftColor,
                  Color topRightColor, Color bottomRightColor, Color bottomLeftColor) final;

    // 2d resource drawing
    void drawImage(const Image *image, AnchorPoint anchor = AnchorPoint::CENTER, float edgeSoftness = 0.0f,
                   McRect clipRect = {}) final;
    void drawString(McFont *font, const UString &text, std::optional<TextShadow> shadow = std::nullopt) final;

    // 3d type drawing
    void drawVAO(VertexArrayObject *vao) final;

    // 2d clipping
    void setClipRect(McRect clipRect) final;
    void pushClipRect(McRect clipRect) final;
    void popClipRect() final;

    // viewport modification
    void pushViewport() final;
    void setViewport(int x, int y, int width, int height) final;
    void popViewport() final;

    // stencil
    void pushStencil() final;
    void fillStencil(bool inside) final;
    void popStencil() final;

    // renderer settings
    void setClipping(bool enabled) final;
    void setAlphaTesting(bool enabled) final;
    void setAlphaTestFunc(DrawCompareFunc alphaFunc, float ref) final;
    void setBlending(bool enabled) final;
    void setBlendMode(DrawBlendMode blendMode) final;
    void setDepthBuffer(bool enabled) final;
    void setColorWriting(bool r, bool g, bool b, bool a) final;
    void setColorInversion(bool enabled) final;
    void setCulling(bool culling) final;
    void setAntialiasing(bool aa) final;
    void setWireframe(bool enabled) final;

    // renderer actions
    void flush() final;
    std::vector<u8> getScreenshot(bool withAlpha = false) final;

    // renderer info
    vec2 getResolution() const final { return this->vResolution; }
    inline const char *getName() const override { return "OpenGL Legacy"; }

    // callbacks
    void onResolutionChange(vec2 newResolution) final;

    // factory
    Image *createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) final;
    Image *createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) final;
    RenderTarget *createRenderTarget(int x, int y, int width, int height, MultisampleType multiSampleType) final;
    Shader *createShaderFromFile(std::string vertexShaderFilePath,
                                 std::string fragmentShaderFilePath) final;                      // DEPRECATED
    Shader *createShaderFromSource(std::string vertexShader, std::string fragmentShader) final;  // DEPRECATED
    VertexArrayObject *createVertexArrayObject(DrawPrimitive primitive, DrawUsageType usage,
                                               bool keepInSystemMemory) final;

   protected:
    void onTransformUpdate() final;

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

    // clipping
    std::vector<McRect> clipRectStack;
};

#endif

#endif
