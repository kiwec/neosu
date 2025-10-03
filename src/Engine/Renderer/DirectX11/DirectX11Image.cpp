//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX implementation of Image
//
// $NoKeywords: $dximg
//===============================================================================//

#include "DirectX11Image.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "Engine.h"
#include "ConVar.h"
#include "Logging.h"

#include "DirectX11Interface.h"
#include "DirectX11Shader.h"

DirectX11Image::DirectX11Image(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : Image(std::move(filepath), mipmapped, keepInSystemMemory) {
    m_texture = nullptr;
    m_shaderResourceView = nullptr;
    m_samplerState = nullptr;

    m_iTextureUnitBackup = 0;
    m_prevShaderResourceView = nullptr;

    m_interfaceOverrideHack = nullptr;
    m_bShared = false;
}

DirectX11Image::DirectX11Image(int width, int height, bool mipmapped, bool keepInSystemMemory)
    : Image(width, height, mipmapped, keepInSystemMemory) {
    m_texture = nullptr;
    m_shaderResourceView = nullptr;
    m_samplerState = nullptr;

    m_iTextureUnitBackup = 0;
    m_prevShaderResourceView = nullptr;

    m_interfaceOverrideHack = nullptr;
    m_bShared = false;
}

DirectX11Image::~DirectX11Image() {
    this->destroy();
    this->deleteDX();
    this->rawImage.clear();
}

void DirectX11Image::init() {
    if((m_texture != nullptr && !this->bKeepInSystemMemory) || !this->bAsyncReady)
        return;  // only load if we are not already loaded

    HRESULT hr;

    auto* graphics = static_cast<DirectX11Interface*>(g.get());
    if(m_interfaceOverrideHack != nullptr) graphics = m_interfaceOverrideHack;

    // create texture (with initial data)
    D3D11_TEXTURE2D_DESC textureDesc;
    D3D11_SUBRESOURCE_DATA initData;
    {
        // default desc
        {
            textureDesc.Width = (UINT)this->iWidth;
            textureDesc.Height = (UINT)this->iHeight;
            textureDesc.MipLevels = (this->bMipmapped ? 0 : 1);
            textureDesc.ArraySize = 1;
            textureDesc.Format =
                Image::NUM_CHANNELS == 4
                    ? DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM
                    : (Image::NUM_CHANNELS == 3 ? DXGI_FORMAT::DXGI_FORMAT_R8_UNORM
                                                : (Image::NUM_CHANNELS == 1 ? DXGI_FORMAT::DXGI_FORMAT_R8_UNORM
                                                                            : DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM));
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Usage =
                (this->bKeepInSystemMemory ? D3D11_USAGE::D3D11_USAGE_DYNAMIC : D3D11_USAGE::D3D11_USAGE_DEFAULT);
            textureDesc.BindFlags = (this->bMipmapped ? D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET : 0) |
                                    D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
            textureDesc.CPUAccessFlags =
                (this->bKeepInSystemMemory ? D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE : 0);
            textureDesc.MiscFlags =
                (this->bMipmapped ? D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_GENERATE_MIPS : 0) |
                (m_bShared ? D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_SHARED : 0);
        }

        // upload new/overwrite data (not mipmapped) (1/2)
        if(m_texture == nullptr) {
            // initData
            {
                initData.pSysMem = (void*)&this->rawImage[0];
                initData.SysMemPitch = static_cast<UINT>(this->iWidth * Image::NUM_CHANNELS * sizeof(unsigned char));
                initData.SysMemSlicePitch = 0;
            }
            hr = graphics->getDevice()->CreateTexture2D(
                &textureDesc,
                (!this->bMipmapped && this->rawImage.size() >= this->iWidth * this->iHeight * Image::NUM_CHANNELS
                     ? &initData
                     : nullptr),
                &m_texture);
            if(FAILED(hr) || m_texture == nullptr) {
                debugLog("DirectX Image Error: Couldn't CreateTexture2D({}, {:x}, {:x}) on file {:s}!", hr, hr,
                         MAKE_DXGI_HRESULT(hr), this->sFilePath);
                engine->showMessageError(
                    "Image Error",
                    UString::format("DirectX Image error, couldn't CreateTexture2D(%ld, %x, %x) on file %s", hr, hr,
                                    MAKE_DXGI_HRESULT(hr), this->sFilePath));
                return;
            }
        } else {
            // TODO: Map(), upload this->rawImage, Unmap()
        }
    }

    // free memory (not mipmapped) (1/2)
    if(!this->bKeepInSystemMemory && !this->bMipmapped) this->rawImage.clear();

    // create shader resource view
    if(m_shaderResourceView == nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        {
            ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));

            shaderResourceViewDesc.Format = textureDesc.Format;
            shaderResourceViewDesc.ViewDimension = D3D_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MipLevels =
                (this->bMipmapped ? (UINT)(std::log2((double)std::max(this->iWidth, this->iHeight))) : 1);
            shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        }
        hr = graphics->getDevice()->CreateShaderResourceView(m_texture, &shaderResourceViewDesc, &m_shaderResourceView);
        if(FAILED(hr) || m_shaderResourceView == nullptr) {
            m_texture->Release();
            m_texture = nullptr;

            debugLog("DirectX Image Error: Couldn't CreateShaderResourceView({}, {:x}, {:x}) on file {:s}!", hr, hr,
                     MAKE_DXGI_HRESULT(hr), this->sFilePath);
            engine->showMessageError(
                "Image Error",
                UString::format("DirectX Image error, couldn't CreateShaderResourceView(%ld, %x, %x) on file %s", hr,
                                hr, MAKE_DXGI_HRESULT(hr), this->sFilePath));

            return;
        }

        // upload new/overwrite data (mipmapped) (2/2)
        if(this->bMipmapped)
            graphics->getDeviceContext()->UpdateSubresource(m_texture, 0, nullptr, initData.pSysMem,
                                                            initData.SysMemPitch,
                                                            initData.SysMemPitch * (UINT)this->iHeight);
    }

    // free memory (mipmapped) (2/2)
    if(!this->bKeepInSystemMemory && this->bMipmapped) this->rawImage.clear();

    // create mipmaps
    if(this->bMipmapped) graphics->getDeviceContext()->GenerateMips(m_shaderResourceView);

    // create sampler
    {
        // default sampler
        if(m_samplerState == nullptr) {
            ZeroMemory(&m_samplerDesc, sizeof(m_samplerDesc));

            m_samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
            m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
            m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
            m_samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
            m_samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
            m_samplerDesc.MipLODBias =
                0.0f;  // TODO: make this configurable somehow (per texture, but also some kind of global override convar?)
            m_samplerDesc.MaxAnisotropy = 1;  // TODO: anisotropic filtering support (valid range 1 to 16)
            m_samplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER;
            m_samplerDesc.BorderColor[0] = 1.0f;
            m_samplerDesc.BorderColor[1] = 1.0f;
            m_samplerDesc.BorderColor[2] = 1.0f;
            m_samplerDesc.BorderColor[3] = 1.0f;
        }

        // customize sampler
        // NOTE: this concatenates into one single actual createOrUpdateSampler() call below because we are not this->bReady yet here on purpose
        {
            if(this->filterMode != Graphics::FILTER_MODE::FILTER_MODE_LINEAR) setFilterMode(this->filterMode);

            if(this->wrapMode != Graphics::WRAP_MODE::WRAP_MODE_CLAMP) setWrapMode(this->wrapMode);
        }

        // actually create the (customized) sampler now
        createOrUpdateSampler();
        if(m_samplerState == nullptr) {
            debugLog("DirectX Image Error: Couldn't CreateSamplerState() on file {:s}!", this->sFilePath);
            engine->showMessageError("Image Error",
                                     UString::format("Couldn't CreateSamplerState() on file %s!", this->sFilePath));
            return;
        }
    }

    this->bReady = true;
}

void DirectX11Image::initAsync() {
    if(m_texture != nullptr) {
        this->bAsyncReady = true;
        return;  // only load if we are not already loaded
    }

    if(!this->bCreatedImage) {
        if(cv::debug_rm.getBool()) debugLog("Resource Manager: Loading {:s}", this->sFilePath);

        this->bAsyncReady = loadRawImage();

        // rewrite all non-4-channels-per-pixel formats, because directx11 doesn't have any fucking 24bpp formats ffs
        if(this->bAsyncReady) {
            const int numTargetChannels = 4;
            const int numMissingChannels = numTargetChannels - Image::NUM_CHANNELS;

            if(numMissingChannels > 0) {
                std::vector<unsigned char> newRawImage;
                newRawImage.reserve(this->iWidth * this->iHeight * numTargetChannels);
                {
                    if(Image::NUM_CHANNELS == 1) {
                        for(size_t c = 0; c < this->rawImage.size(); c += (size_t)Image::NUM_CHANNELS) {
                            newRawImage.push_back(this->rawImage[c + 0]);  // R
                            newRawImage.push_back(this->rawImage[c + 0]);  // G
                            newRawImage.push_back(this->rawImage[c + 0]);  // B
                            newRawImage.push_back(0xff);                   // A
                        }
                    } else if(Image::NUM_CHANNELS == 3) {
                        for(size_t c = 0; c < this->rawImage.size(); c += (size_t)Image::NUM_CHANNELS) {
                            newRawImage.push_back(this->rawImage[c + 0]);  // R
                            newRawImage.push_back(this->rawImage[c + 1]);  // G
                            newRawImage.push_back(this->rawImage[c + 2]);  // B
                            newRawImage.push_back(0xff);                   // A
                        }
                    } else {
                        for(size_t c = 0; c < this->rawImage.size(); c += (size_t)Image::NUM_CHANNELS) {
                            // add original data
                            for(int p = 0; p < Image::NUM_CHANNELS; p++) {
                                newRawImage.push_back(this->rawImage[c + p]);
                            }

                            // add padded data
                            for(int m = 0; m < numMissingChannels; m++) {
                                newRawImage.push_back(0xff);
                            }
                        }
                    }
                }
                this->rawImage = std::move(newRawImage);
                // Image::NUM_CHANNELS = numTargetChannels;
            }
        }
    } else {
        // created image is always async ready
        this->bAsyncReady = true;
    }
}

void DirectX11Image::destroy() {
    // FIXME: avoid needing to reupload everything from scratch for dynamic font atlas image reloads
    // like opengl does it
    this->deleteDX();
    if(!this->bKeepInSystemMemory) {
        this->rawImage.clear();
    }
}

void DirectX11Image::deleteDX() {
    if(m_shaderResourceView != nullptr) {
        m_shaderResourceView->Release();
        m_shaderResourceView = nullptr;
    }

    if(m_texture != nullptr) {
        m_texture->Release();
        m_texture = nullptr;
    }
}

void DirectX11Image::bind(unsigned int textureUnit) const {
    if(!this->bReady) return;

    m_iTextureUnitBackup = textureUnit;

    // backup
    // HACKHACK: slow af
    {
        static_cast<DirectX11Interface*>(g.get())->getDeviceContext()->PSGetShaderResources(textureUnit, 1,
                                                                                            &m_prevShaderResourceView);
    }

    static_cast<DirectX11Interface*>(g.get())->getDeviceContext()->PSSetShaderResources(textureUnit, 1,
                                                                                        &m_shaderResourceView);
    static_cast<DirectX11Interface*>(g.get())->getDeviceContext()->PSSetSamplers(textureUnit, 1, &m_samplerState);

    // HACKHACK: TEMP:
    static_cast<DirectX11Interface*>(g.get())->getShaderGeneric()->setUniform1f("misc", 1.0f);  // enable texturing
}

void DirectX11Image::unbind() const {
    if(!this->bReady) return;

    // restore
    // HACKHACK: slow af
    {
        static_cast<DirectX11Interface*>(g.get())->getDeviceContext()->PSSetShaderResources(m_iTextureUnitBackup, 1,
                                                                                            &m_prevShaderResourceView);

        // refcount
        {
            if(m_prevShaderResourceView != nullptr) {
                m_prevShaderResourceView->Release();
                m_prevShaderResourceView = nullptr;
            }
        }
    }
}

void DirectX11Image::setFilterMode(Graphics::FILTER_MODE filterMode) {
    Image::setFilterMode(filterMode);

    switch(filterMode) {
        case Graphics::FILTER_MODE::FILTER_MODE_NONE:
            m_samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;
        case Graphics::FILTER_MODE::FILTER_MODE_LINEAR:
            m_samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        case Graphics::FILTER_MODE::FILTER_MODE_MIPMAP:
            m_samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
    }

    // TODO: anisotropic filtering support (m_samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC), needs new FILTER_MODE_ANISOTROPIC and support in other renderers (implies mipmapping)

    if(!this->bReady) return;

    createOrUpdateSampler();
}

void DirectX11Image::setWrapMode(Graphics::WRAP_MODE wrapMode) {
    Image::setWrapMode(wrapMode);

    switch(wrapMode) {
        case Graphics::WRAP_MODE::WRAP_MODE_CLAMP:
            m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
            m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
            m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        case Graphics::WRAP_MODE::WRAP_MODE_REPEAT:
            m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
            m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
            m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MODE::D3D11_TEXTURE_ADDRESS_WRAP;
            break;
    }

    if(!this->bReady) return;

    createOrUpdateSampler();
}

void DirectX11Image::createOrUpdateSampler() {
    if(m_samplerState != nullptr) {
        m_samplerState->Release();
        m_samplerState = nullptr;
    }

    static_cast<DirectX11Interface*>(g.get())->getDevice()->CreateSamplerState(&m_samplerDesc, &m_samplerState);
}

#endif
