//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		raw DirectX 11 graphics interface
//
// $NoKeywords: $dx11i
//===============================================================================//

#pragma once
#ifndef DIRECTX11INTERFACE_H
#define DIRECTX11INTERFACE_H

#include "BaseEnvironment.h"
#include "Graphics.h"

class DirectX11Shader;

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "d3d11.h"

class DirectX11Interface : public Graphics {
   public:
    struct SimpleVertex {
        vec3 pos;
        vec4 col;
        vec2 tex;
    };

   public:
    DirectX11Interface(HWND hwnd, bool minimalistContext = false);
    ~DirectX11Interface() override;

    // scene
    void beginScene() override;
    void endScene() override;

    // depth buffer
    void clearDepthBuffer() override;

    // color
    void setColor(Color color) override;
    void setAlpha(float alpha) override;

    // 2d primitive drawing
    void drawPixel(int x, int y) override;
    void drawPixels(int x, int y, int width, int height, Graphics::DRAWPIXELS_TYPE type, const void *pixels) override;
    void drawLinef(float x1, float y1, float x2, float y2) final;
    void drawRectf(const RectOptions &opt) final;
    void fillRectf(float x, float y, float width, float height) final;

    void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                      Color bottomLeftColor, Color bottomRightColor) override;

    void drawQuad(int x, int y, int width, int height) override;
    void drawQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft, Color topLeftColor,
                  Color topRightColor, Color bottomRightColor, Color bottomLeftColor) override;

    // 2d resource drawing
    void drawImage(const Image *image, AnchorPoint anchor = AnchorPoint::CENTER, float edgeSoftness = 0.0f,
                   McRect clipRect = {}) final;
    void drawString(McFont *font, const UString &text) override;

    // 3d type drawing
    void drawVAO(VertexArrayObject *vao) override;

    // DEPRECATED: 2d clipping
    void setClipRect(McRect clipRect) override;
    void pushClipRect(McRect clipRect) override;
    void popClipRect() override;

    // TODO:
    void fillRoundedRect(int /*x*/, int /*y*/, int /*width*/, int /*height*/, int /*radius*/) override { ; }

    // TODO (?): unused currently
    void pushStencil() override { ; }
    void fillStencil(bool /*inside*/) override { ; }
    void popStencil() override { ; }

    // renderer settings
    void setClipping(bool enabled) override;
    void setAlphaTesting(bool enabled) override;
    void setAlphaTestFunc(COMPARE_FUNC alphaFunc, float ref) override;
    void setBlending(bool enabled) override;
    void setBlendMode(BLEND_MODE blendMode) override;
    void setDepthBuffer(bool enabled) override;
    void setCulling(bool culling) override;
    void setDepthWriting(bool enabled) final;
    void setColorWriting(bool r, bool g, bool b, bool a) final;
    void setColorInversion(bool enabled) final;
    void setAntialiasing(bool aa) override;
    void setWireframe(bool enabled) override;
    void setLineWidth(float width) override;

    // renderer actions
    void flush() override;
    std::vector<u8> getScreenshot(bool withAlpha) override;

    // renderer info
    inline const char *getName() const override { return "DirectX11"; }
    vec2 getResolution() const override { return m_vResolution; }
    UString getVendor() override;
    UString getModel() override;
    UString getVersion() override;
    int getVRAMTotal() override;
    int getVRAMRemaining() override;

    // device settings
    void setVSync(bool vsync) override;

    // callbacks
    void onResolutionChange(vec2 newResolution) override;

    // factory
    Image *createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) override;
    Image *createImage(int width, int height, bool mipmapped, bool keepInSystemMemory) override;
    RenderTarget *createRenderTarget(int x, int y, int width, int height,
                                     Graphics::MULTISAMPLE_TYPE multiSampleType) override;
    Shader *createShaderFromFile(std::string vertexShaderFilePath, std::string fragmentShaderFilePath) override;
    Shader *createShaderFromSource(std::string vertexShader, std::string fragmentShader) override;
    VertexArrayObject *createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage,
                                               bool keepInSystemMemory) override;

    // ILLEGAL:
    void resizeTarget(vec2 newResolution);
    bool enableFullscreen(bool borderlessWindowedFullscreen = false);
    void disableFullscreen();
    void setActiveShader(DirectX11Shader *shader) { m_activeShader = shader; }
    inline bool isReady() const { return m_bReady; }
    inline ID3D11Device *getDevice() const { return m_device; }
    inline ID3D11DeviceContext *getDeviceContext() const { return m_deviceContext; }
    inline IDXGISwapChain *getSwapChain() const { return m_swapChain; }
    inline DirectX11Shader *getShaderGeneric() const { return m_shaderTexturedGeneric; }
    inline DirectX11Shader *getActiveShader() const { return m_activeShader; }
    void setTexturing(bool enabled);

   protected:
    void init() override;
    void onTransformUpdate(Matrix4 &projectionMatrix, Matrix4 &worldMatrix) override;

   private:
    static int primitiveToDirectX(Graphics::PRIMITIVE primitive);
    static int compareFuncToDirectX(Graphics::COMPARE_FUNC compareFunc);

   private:
    bool m_bReady;

    // device context
    HWND m_hwnd;
    bool m_bMinimalistContext;

    // d3d
    ID3D11Device *m_device;
    ID3D11DeviceContext *m_deviceContext;
    DXGI_MODE_DESC m_swapChainModeDesc;
    IDXGISwapChain *m_swapChain;
    ID3D11RenderTargetView *m_frameBuffer;
    ID3D11Texture2D *m_frameBufferDepthStencilTexture;
    ID3D11DepthStencilView *m_frameBufferDepthStencilView;

    // renderer
    bool m_bIsFullscreen;
    bool m_bIsFullscreenBorderlessWindowed;
    vec2 m_vResolution;

    ID3D11RasterizerState *m_rasterizerState;
    D3D11_RASTERIZER_DESC m_rasterizerDesc;

    ID3D11DepthStencilState *m_depthStencilState;
    D3D11_DEPTH_STENCIL_DESC m_depthStencilDesc;

    ID3D11BlendState *m_blendState;
    D3D11_BLEND_DESC m_blendDesc;

    DirectX11Shader *m_shaderTexturedGeneric;

    std::vector<SimpleVertex> m_vertices;
    size_t m_iVertexBufferMaxNumVertices;
    size_t m_iVertexBufferNumVertexOffsetCounter;
    D3D11_BUFFER_DESC m_vertexBufferDesc;
    ID3D11Buffer *m_vertexBuffer;

    // persistent vars
    bool m_bVSync;
    bool m_bColorInversion{false};
    bool m_bTexturingEnabled{false};
    Color m_color;
    DirectX11Shader *m_activeShader;

    // clipping
    std::stack<McRect> m_clipRectStack;

    // stats
    int m_iStatsNumDrawCalls;

    // clipping for drawImage
    std::unique_ptr<Shader> smoothClipShader{nullptr};
    void initSmoothClipShader();
};

#else

class DirectX11Interface : public Graphics {
   public:
    void resizeTarget(vec2) {}
    bool enableFullscreen(bool) { return false; }
    void disableFullscreen() {}
};

#endif

#endif
