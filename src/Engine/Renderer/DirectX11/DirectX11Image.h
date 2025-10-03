//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX implementation of Image
//
// $NoKeywords: $dximg
//===============================================================================//

#pragma once
#ifndef DIRECTX11IMAGE_H
#define DIRECTX11IMAGE_H

#include "Image.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "d3d11.h"

class DirectX11Interface;

class DirectX11Image final : public Image {
   public:
    DirectX11Image(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    DirectX11Image(int width, int height, bool mipmapped = false, bool keepInSystemMemory = false);
    ~DirectX11Image() override;

    void bind(unsigned int textureUnit = 0) const override;
    void unbind() const override;

    void setFilterMode(Graphics::FILTER_MODE filterMode) override;
    void setWrapMode(Graphics::WRAP_MODE wrapMode) override;

    // ILLEGAL:
    void setDirectX11InterfaceHack(DirectX11Interface *dxi) { m_interfaceOverrideHack = dxi; }
    void setShared(bool shared) { m_bShared = shared; }
    ID3D11Texture2D *getTexture() const { return m_texture; }
    ID3D11ShaderResourceView *getShaderResourceView() const { return m_shaderResourceView; }

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    void createOrUpdateSampler();

   private:
    void deleteDX();

    ID3D11Texture2D *m_texture;
    ID3D11ShaderResourceView *m_shaderResourceView;
    ID3D11SamplerState *m_samplerState;
    D3D11_SAMPLER_DESC m_samplerDesc;

    mutable unsigned int m_iTextureUnitBackup;
    mutable ID3D11ShaderResourceView *m_prevShaderResourceView;

    DirectX11Interface *m_interfaceOverrideHack;
    bool m_bShared;
};

#endif

#endif
