//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX implementation of RenderTarget / render to texture
//
// $NoKeywords: $drt
//===============================================================================//

#include "DirectX11RenderTarget.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "Engine.h"
#include "ConVar.h"
#include "VertexArrayObject.h"
#include "Logging.h"

#include "DirectX11Interface.h"

DirectX11RenderTarget::DirectX11RenderTarget(int x, int y, int width, int height,
                                             Graphics::MULTISAMPLE_TYPE multiSampleType)
    : RenderTarget(x, y, width, height, multiSampleType) {
    this->renderTexture = nullptr;
    this->depthStencilTexture = nullptr;
    this->renderTargetView = nullptr;
    this->depthStencilView = nullptr;
    this->shaderResourceView = nullptr;

    this->prevRenderTargetView = nullptr;
    this->prevDepthStencilView = nullptr;

    this->iTextureUnitBackup = 0;
    this->prevShaderResourceView = nullptr;
}

void DirectX11RenderTarget::init() {
    debugLog("Building RenderTarget ({}x{}) ...", (int)this->vSize.x, (int)this->vSize.y);

    HRESULT hr;

    auto* device = static_cast<DirectX11Interface*>(g.get())->getDevice();

    // create color texture
    D3D11_TEXTURE2D_DESC colorTextureDesc;
    {
        colorTextureDesc.ArraySize = 1;
        colorTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        colorTextureDesc.CPUAccessFlags = 0;
        colorTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        colorTextureDesc.MipLevels = 1;
        colorTextureDesc.MiscFlags = 0;
        colorTextureDesc.SampleDesc.Count = 1;
        colorTextureDesc.SampleDesc.Quality = 0;
        colorTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        colorTextureDesc.Width = (UINT)this->vSize.x;
        colorTextureDesc.Height = (UINT)this->vSize.y;
    }
    hr = device->CreateTexture2D(&colorTextureDesc, nullptr, &this->renderTexture);
    if(FAILED(hr)) {
        engine->showMessageErrorFatal(
            "RenderTarget Error",
            UString::format("Couldn't color CreateTexture2D(%ld, %x, %x)!", hr, hr, MAKE_DXGI_HRESULT(hr)));
        return;
    }

    // create depthstencil texture
    D3D11_TEXTURE2D_DESC depthStencilTextureDesc;
    {
        depthStencilTextureDesc.ArraySize = 1;
        depthStencilTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthStencilTextureDesc.CPUAccessFlags = 0;
        depthStencilTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilTextureDesc.MipLevels = 1;
        depthStencilTextureDesc.MiscFlags = 0;
        depthStencilTextureDesc.SampleDesc.Count = 1;
        depthStencilTextureDesc.SampleDesc.Quality = 0;
        depthStencilTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        depthStencilTextureDesc.Width = (UINT)this->vSize.x;
        depthStencilTextureDesc.Height = (UINT)this->vSize.y;
    }
    hr = device->CreateTexture2D(&depthStencilTextureDesc, nullptr, &this->depthStencilTexture);
    if(FAILED(hr)) {
        engine->showMessageErrorFatal(
            "RenderTarget Error",
            UString::format("Couldn't depthStencil CreateTexture2D(%ld, %x, %x)!", hr, hr, MAKE_DXGI_HRESULT(hr)));
        return;
    }

    // create rendertarget view
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    {
        renderTargetViewDesc.Format = colorTextureDesc.Format;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;
    }
    hr = device->CreateRenderTargetView(this->renderTexture, &renderTargetViewDesc, &this->renderTargetView);
    if(FAILED(hr)) {
        engine->showMessageErrorFatal(
            "RenderTarget Error",
            UString::format("Couldn't CreateRenderTargetView(%ld, %x, %x)!", hr, hr, MAKE_DXGI_HRESULT(hr)));
        return;
    }

    // create depthstencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    {
        depthStencilViewDesc.Format = depthStencilTextureDesc.Format;
        depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Flags = 0;
        depthStencilViewDesc.Texture2D.MipSlice = 0;
    }
    hr = device->CreateDepthStencilView(this->depthStencilTexture, &depthStencilViewDesc, &this->depthStencilView);
    if(FAILED(hr)) {
        engine->showMessageErrorFatal(
            "RenderTarget Error",
            UString::format("Couldn't CreateDepthStencilView(%ld, %x, %x)!", hr, hr, MAKE_DXGI_HRESULT(hr)));
        return;
    }

    // create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    {
        shaderResourceViewDesc.Format = colorTextureDesc.Format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;
    }
    hr = device->CreateShaderResourceView(this->renderTexture, &shaderResourceViewDesc, &this->shaderResourceView);
    if(FAILED(hr)) {
        engine->showMessageErrorFatal(
            "RenderTarget Error",
            UString::format("Couldn't CreateShaderResourceView(%ld, %x, %x)!", hr, hr, MAKE_DXGI_HRESULT(hr)));
        return;
    }

    this->setReady(true);
}

void DirectX11RenderTarget::initAsync() { this->setAsyncReady(true); }

void DirectX11RenderTarget::destroy() {
    if(this->shaderResourceView != nullptr) this->shaderResourceView->Release();

    if(this->depthStencilView != nullptr) this->depthStencilView->Release();

    if(this->renderTargetView != nullptr) this->renderTargetView->Release();

    if(this->depthStencilTexture != nullptr) this->depthStencilTexture->Release();

    if(this->renderTexture != nullptr) this->renderTexture->Release();

    this->shaderResourceView = nullptr;
    this->depthStencilView = nullptr;
    this->renderTargetView = nullptr;
    this->depthStencilTexture = nullptr;
    this->renderTexture = nullptr;
}

void DirectX11RenderTarget::draw(int x, int y) {
    if(!this->isReady()) {
        debugLog("WARNING: RenderTarget is not ready!");
        return;
    }

    bind();
    {
        g->setColor(this->color);
        g->drawQuad(x, y, this->vSize.x, this->vSize.y);
    }
    unbind();
}

void DirectX11RenderTarget::draw(int x, int y, int width, int height) {
    if(!this->isReady()) {
        debugLog("WARNING: RenderTarget is not ready!");
        return;
    }

    bind();
    {
        g->setColor(this->color);
        g->drawQuad(x, y, width, height);
    }
    unbind();
}

void DirectX11RenderTarget::drawRect(int x, int y, int width, int height) {
    if(!this->isReady()) {
        debugLog("WARNING: RenderTarget is not ready!");
        return;
    }

    const float texCoordWidth0 = x / this->vSize.x;
    const float texCoordWidth1 = (x + width) / this->vSize.x;
    const float texCoordHeight1 = y / this->vSize.y;
    const float texCoordHeight0 = (y + height) / this->vSize.y;

    bind();
    {
        g->setColor(this->color);

        static VertexArrayObject vao;

        vao.clear();

        vao.addTexcoord(texCoordWidth0, texCoordHeight1);
        vao.addVertex(x, y);

        vao.addTexcoord(texCoordWidth0, texCoordHeight0);
        vao.addVertex(x, y + height);

        vao.addTexcoord(texCoordWidth1, texCoordHeight0);
        vao.addVertex(x + width, y + height);

        vao.addTexcoord(texCoordWidth1, texCoordHeight0);
        vao.addVertex(x + width, y + height);

        vao.addTexcoord(texCoordWidth1, texCoordHeight1);
        vao.addVertex(x + width, y);

        vao.addTexcoord(texCoordWidth0, texCoordHeight1);
        vao.addVertex(x, y);

        g->drawVAO(&vao);
    }
    unbind();
}

void DirectX11RenderTarget::enable() {
    if(!this->isReady()) return;

    auto* context = static_cast<DirectX11Interface*>(g.get())->getDeviceContext();

    // backup
    // HACKHACK: slow af
    {
        context->OMGetRenderTargets(1, &this->prevRenderTargetView, &this->prevDepthStencilView);
    }

    context->OMSetRenderTargets(1, &this->renderTargetView, this->depthStencilView);

    // clear
    Color clearColor = this->clearColor;
    if(cv::debug_rt.getBool()) clearColor = argb(0.5f, 0.0f, 0.5f, 0.0f);

    float fClearColor[4] = {clearColor.Rf(), clearColor.Gf(), clearColor.Bf(), clearColor.Af()};

    if(this->bClearColorOnDraw) context->ClearRenderTargetView(this->renderTargetView, fClearColor);

    if(this->bClearDepthOnDraw)
        context->ClearDepthStencilView(this->depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,
                                       0);  // yes, the 1.0f is correct
}

void DirectX11RenderTarget::disable() {
    if(!this->isReady()) return;

    // restore
    // HACKHACK: slow af
    {
        static_cast<DirectX11Interface*>(g.get())->getDeviceContext()->OMSetRenderTargets(
            1, &this->prevRenderTargetView, this->prevDepthStencilView);

        // refcount
        {
            if(this->prevRenderTargetView != nullptr) {
                this->prevRenderTargetView->Release();
                this->prevRenderTargetView = nullptr;
            }

            if(this->prevDepthStencilView != nullptr) {
                this->prevDepthStencilView->Release();
                this->prevDepthStencilView = nullptr;
            }
        }
    }
}

void DirectX11RenderTarget::bind(unsigned int textureUnit) {
    if(!this->isReady()) return;

    auto* dx11 = static_cast<DirectX11Interface*>(g.get());
    auto* context = dx11->getDeviceContext();

    this->iTextureUnitBackup = textureUnit;

    // backup
    // HACKHACK: slow af
    {
        context->PSGetShaderResources(textureUnit, 1, &this->prevShaderResourceView);
    }

    context->PSSetShaderResources(textureUnit, 1, &this->shaderResourceView);

    dx11->setTexturing(true);  // enable texturing
}

void DirectX11RenderTarget::unbind() {
    if(!this->isReady()) return;

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

#endif
