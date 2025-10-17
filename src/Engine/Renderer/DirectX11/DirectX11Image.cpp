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

DirectX11Image::DirectX11Image(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : Image(std::move(filepath), mipmapped, keepInSystemMemory) {
    this->texture = nullptr;
    this->shaderResourceView = nullptr;
    this->samplerState = nullptr;

    this->iTextureUnitBackup = 0;
    this->prevShaderResourceView = nullptr;

    this->bShared = false;
}

DirectX11Image::DirectX11Image(int width, int height, bool mipmapped, bool keepInSystemMemory)
    : Image(width, height, mipmapped, keepInSystemMemory) {
    this->texture = nullptr;
    this->shaderResourceView = nullptr;
    this->samplerState = nullptr;

    this->iTextureUnitBackup = 0;
    this->prevShaderResourceView = nullptr;

    this->bShared = false;
}

DirectX11Image::~DirectX11Image() {
    this->destroy();
    this->deleteDX();
    this->rawImage.reset();
}

void DirectX11Image::init() {
    if((this->texture != nullptr && !this->bKeepInSystemMemory) || !this->bAsyncReady)
        return;  // only load if we are not already loaded

    HRESULT hr;

    auto* device = static_cast<DirectX11Interface*>(g.get())->getDevice();
    auto* context = static_cast<DirectX11Interface*>(g.get())->getDeviceContext();

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
                    ? DXGI_FORMAT_R8G8B8A8_UNORM
                    : (Image::NUM_CHANNELS == 3
                           ? DXGI_FORMAT_R8_UNORM
                           : (Image::NUM_CHANNELS == 1 ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM));
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Usage = (this->bKeepInSystemMemory ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT);
            textureDesc.BindFlags = (this->bMipmapped ? D3D11_BIND_RENDER_TARGET : 0) | D3D11_BIND_SHADER_RESOURCE;
            textureDesc.CPUAccessFlags = (this->bKeepInSystemMemory ? D3D11_CPU_ACCESS_WRITE : 0);
            textureDesc.MiscFlags = (this->bMipmapped ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0) |
                                    (this->bShared ? D3D11_RESOURCE_MISC_SHARED : 0);
        }

        // upload new/overwrite data (not mipmapped) (1/2)
        if(this->texture == nullptr) {
            // initData
            {
                initData.pSysMem = (void*)this->rawImage->data();
                initData.SysMemPitch = static_cast<UINT>(this->iWidth * Image::NUM_CHANNELS * sizeof(unsigned char));
                initData.SysMemSlicePitch = 0;
            }
            hr = device->CreateTexture2D(
                &textureDesc,
                (!this->bMipmapped && this->totalBytes() >= this->iWidth * this->iHeight * Image::NUM_CHANNELS
                     ? &initData
                     : nullptr),
                &this->texture);
            if(FAILED(hr) || this->texture == nullptr) {
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
    if(!this->bKeepInSystemMemory && !this->bMipmapped) this->rawImage.reset();

    // create shader resource view
    if(this->shaderResourceView == nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
        {
            shaderResourceViewDesc.Format = textureDesc.Format;
            shaderResourceViewDesc.ViewDimension = D3D_SRV_DIMENSION::D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MipLevels =
                (this->bMipmapped ? (UINT)(std::log2((double)std::max(this->iWidth, this->iHeight))) : 1);
            shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        }
        hr = device->CreateShaderResourceView(this->texture, &shaderResourceViewDesc, &this->shaderResourceView);
        if(FAILED(hr) || this->shaderResourceView == nullptr) {
            this->texture->Release();
            this->texture = nullptr;

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
            context->UpdateSubresource(this->texture, 0, nullptr, initData.pSysMem, initData.SysMemPitch,
                                       initData.SysMemPitch * (UINT)this->iHeight);
    }

    // free memory (mipmapped) (2/2)
    if(!this->bKeepInSystemMemory && this->bMipmapped) this->rawImage.reset();

    // create mipmaps
    if(this->bMipmapped) context->GenerateMips(this->shaderResourceView);

    // create sampler
    {
        // default sampler
        if(this->samplerState == nullptr) {
            this->samplerDesc = {};

            this->samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            this->samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            this->samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            this->samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            this->samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
            this->samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
            this->samplerDesc.MipLODBias =
                0.0f;  // TODO: make this configurable somehow (per texture, but also some kind of global override convar?)
            this->samplerDesc.MaxAnisotropy = 1;  // TODO: anisotropic filtering support (valid range 1 to 16)
            this->samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
            this->samplerDesc.BorderColor[0] = 1.0f;
            this->samplerDesc.BorderColor[1] = 1.0f;
            this->samplerDesc.BorderColor[2] = 1.0f;
            this->samplerDesc.BorderColor[3] = 1.0f;
        }

        // customize sampler
        // NOTE: this concatenates into one single actual createOrUpdateSampler() call below because we are not this->bReady yet here on purpose
        {
            if(this->filterMode != Graphics::FILTER_MODE::FILTER_MODE_LINEAR) setFilterMode(this->filterMode);

            if(this->wrapMode != Graphics::WRAP_MODE::WRAP_MODE_CLAMP) setWrapMode(this->wrapMode);
        }

        // actually create the (customized) sampler now
        createOrUpdateSampler();
        if(this->samplerState == nullptr) {
            debugLog("DirectX Image Error: Couldn't CreateSamplerState() on file {:s}!", this->sFilePath);
            engine->showMessageError("Image Error",
                                     UString::format("Couldn't CreateSamplerState() on file %s!", this->sFilePath));
            return;
        }
    }

    this->bReady = true;
}

void DirectX11Image::initAsync() {
    if(this->texture != nullptr) {
        this->bAsyncReady = true;
        return;  // only load if we are not already loaded
    }

    if(!this->bCreatedImage) {
        logIfCV(debug_rm, "Resource Manager: Loading {:s}", this->sFilePath);

        this->bAsyncReady = loadRawImage();
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
        this->rawImage.reset();
    }
}

void DirectX11Image::deleteDX() {
    if(this->shaderResourceView != nullptr) {
        this->shaderResourceView->Release();
        this->shaderResourceView = nullptr;
    }

    if(this->texture != nullptr) {
        this->texture->Release();
        this->texture = nullptr;
    }
}

void DirectX11Image::bind(unsigned int textureUnit) const {
    if(!this->bReady) return;

    this->iTextureUnitBackup = textureUnit;

    auto* dx11 = static_cast<DirectX11Interface*>(g.get());
    auto* context = dx11->getDeviceContext();
    // backup
    // HACKHACK: slow af
    {
        context->PSGetShaderResources(textureUnit, 1, &this->prevShaderResourceView);
    }

    context->PSSetShaderResources(textureUnit, 1, &this->shaderResourceView);
    context->PSSetSamplers(textureUnit, 1, &this->samplerState);

    // HACKHACK: TEMP:
    dx11->setTexturing(true);  // enable texturing
}

void DirectX11Image::unbind() const {
    if(!this->bReady) return;

    // restore
    // HACKHACK: slow af
    {
        static_cast<DirectX11Interface*>(g.get())->getDeviceContext()->PSSetShaderResources(
            this->iTextureUnitBackup, 1, &this->prevShaderResourceView);

        // refcount
        {
            if(this->prevShaderResourceView != nullptr) {
                this->prevShaderResourceView->Release();
                this->prevShaderResourceView = nullptr;
            }
        }
    }
}

void DirectX11Image::setFilterMode(Graphics::FILTER_MODE filterMode) {
    Image::setFilterMode(filterMode);

    switch(filterMode) {
        case Graphics::FILTER_MODE::FILTER_MODE_NONE:
            this->samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;
        case Graphics::FILTER_MODE::FILTER_MODE_LINEAR:
            this->samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        case Graphics::FILTER_MODE::FILTER_MODE_MIPMAP:
            this->samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
    }

    // TODO: anisotropic filtering support (this->samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC), needs new FILTER_MODE_ANISOTROPIC and support in other renderers (implies mipmapping)

    if(!this->bReady) return;

    createOrUpdateSampler();
}

void DirectX11Image::setWrapMode(Graphics::WRAP_MODE wrapMode) {
    Image::setWrapMode(wrapMode);

    switch(wrapMode) {
        case Graphics::WRAP_MODE::WRAP_MODE_CLAMP:
            this->samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            this->samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            this->samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;
        case Graphics::WRAP_MODE::WRAP_MODE_REPEAT:
            this->samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            this->samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            this->samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            break;
    }

    if(!this->bReady) return;

    createOrUpdateSampler();
}

void DirectX11Image::createOrUpdateSampler() {
    if(this->samplerState != nullptr) {
        this->samplerState->Release();
        this->samplerState = nullptr;
    }

    static_cast<DirectX11Interface*>(g.get())->getDevice()->CreateSamplerState(&this->samplerDesc, &this->samplerState);
}

#endif
