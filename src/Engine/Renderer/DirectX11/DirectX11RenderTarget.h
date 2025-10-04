//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX implementation of RenderTarget / render to texture
//
// $NoKeywords: $drt
//===============================================================================//

#pragma once
#ifndef DIRECTX11RENDERTARGET_H
#define DIRECTX11RENDERTARGET_H

#include "RenderTarget.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "d3d11.h"

class DirectX11Interface;

class DirectX11RenderTarget final : public RenderTarget {
    NOCOPY_NOMOVE(DirectX11RenderTarget)
   public:
    DirectX11RenderTarget(int x, int y, int width, int height,
                          Graphics::MULTISAMPLE_TYPE multiSampleType = Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);
    ~DirectX11RenderTarget() override { destroy(); }

    void draw(int x, int y) override;
    void draw(int x, int y, int width, int height) override;
    void drawRect(int x, int y, int width, int height) override;

    void enable() override;
    void disable() override;

    void bind(unsigned int textureUnit = 0) override;
    void unbind() override;

    // ILLEGAL:
    [[nodiscard]] inline ID3D11Texture2D *getRenderTexture() const { return this->renderTexture; }

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    ID3D11Texture2D *renderTexture;
    ID3D11Texture2D *depthStencilTexture;
    ID3D11RenderTargetView *renderTargetView;
    ID3D11DepthStencilView *depthStencilView;
    ID3D11ShaderResourceView *shaderResourceView;

    ID3D11RenderTargetView *prevRenderTargetView;
    ID3D11DepthStencilView *prevDepthStencilView;

    unsigned int iTextureUnitBackup;
    ID3D11ShaderResourceView *prevShaderResourceView;
};

#endif

#endif
