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

#if defined(__GNUC__) || defined(__clang__)
MC_DO_PRAGMA(GCC diagnostic ignored "-Wpragmas")
MC_DO_PRAGMA(GCC diagnostic ignored "-Wextern-c-compat")
MC_DO_PRAGMA(GCC diagnostic push)
#endif

#include "d3d11.h"

#if defined(__GNUC__) || defined(__clang__)
MC_DO_PRAGMA(GCC diagnostic pop)
#endif

class DirectX11Interface;

class DirectX11Image final : public Image {
    NOCOPY_NOMOVE(DirectX11Image)
   public:
    DirectX11Image(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    DirectX11Image(int width, int height, bool mipmapped = false, bool keepInSystemMemory = false);
    ~DirectX11Image() override;

    void bind(unsigned int textureUnit = 0) const override;
    void unbind() const override;

    void setFilterMode(Graphics::FILTER_MODE filterMode) override;
    void setWrapMode(Graphics::WRAP_MODE wrapMode) override;

    // ILLEGAL:
    inline void setShared(bool shared) { this->bShared = shared; }
    inline ID3D11Texture2D *getTexture() const { return this->texture; }
    inline ID3D11ShaderResourceView *getShaderResourceView() const { return this->shaderResourceView; }

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    void createOrUpdateSampler();

   private:
    void deleteDX();

    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *shaderResourceView;
    ID3D11SamplerState *samplerState;
    D3D11_SAMPLER_DESC samplerDesc;

    mutable unsigned int iTextureUnitBackup;
    mutable ID3D11ShaderResourceView *prevShaderResourceView;

    bool bShared;
};

#endif

#endif
