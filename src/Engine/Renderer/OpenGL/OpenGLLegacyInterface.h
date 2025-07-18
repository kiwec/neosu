//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		raw legacy opengl graphics interface
//
// $NoKeywords: $lgli
//===============================================================================//

#pragma once
#ifndef LEGACYOPENGLINTERFACE_H
#define LEGACYOPENGLINTERFACE_H

#include "cbase.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "OpenGLSync.h"

class Image;

class OpenGLLegacyInterface : public Graphics {
   public:
    OpenGLLegacyInterface();
    ~OpenGLLegacyInterface() override;

    void init() override;

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
    void drawLine(int x1, int y1, int x2, int y2) final;
    void drawLine(Vector2 pos1, Vector2 pos2) final;
    void drawRect(int x, int y, int width, int height) final;
    void drawRect(int x, int y, int width, int height, Color top, Color right, Color bottom, Color left) final;

    void fillRect(int x, int y, int width, int height) final;
    void fillRoundedRect(int x, int y, int width, int height, int radius) final;
    void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                      Color bottomLeftColor, Color bottomRightColor) final;

    void drawQuad(int x, int y, int width, int height) final;
    void drawQuad(Vector2 topLeft, Vector2 topRight, Vector2 bottomRight, Vector2 bottomLeft, Color topLeftColor,
                  Color topRightColor, Color bottomRightColor, Color bottomLeftColor) final;

    // 2d resource drawing
    void drawImage(Image *image, AnchorPoint anchor = AnchorPoint::CENTER) final;
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

    // renderer actions
    void flush() final;
    std::vector<unsigned char> getScreenshot() final;

    // renderer info
    Vector2 getResolution() const final { return this->vResolution; }
    UString getVendor() override;
    UString getModel() override;
    UString getVersion() override;
    int getVRAMTotal() override;
    int getVRAMRemaining() override;

    // callbacks
    void onResolutionChange(Vector2 newResolution) final;

    // factory
    Image *createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) final;
    Image *createImage(int width, int height, bool mipmapped, bool keepInSystemMemory) final;
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
    static int primitiveToOpenGL(Graphics::PRIMITIVE primitive);
    static int compareFuncToOpenGL(Graphics::COMPARE_FUNC compareFunc);

    void handleGLErrors();

    // renderer
    bool bInScene;
    Vector2 vResolution;

    // persistent vars
    bool bAntiAliasing;
    Color color;
    float fZ;
    float fClearZ;

    // synchronization
    OpenGLSync *syncobj;

    // clipping
    std::stack<McRect> clipRectStack;
};

#else
class OpenGLLegacyInterface : public Graphics {};
#endif

#endif
