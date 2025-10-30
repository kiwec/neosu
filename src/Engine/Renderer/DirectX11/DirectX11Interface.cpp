//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		raw DirectX 11 graphics interface
//
// $NoKeywords: $dx11i
//===============================================================================//

#include "DirectX11Interface.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "dxgi1_3.h"

#include "Camera.h"
#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include "ResourceManager.h"
#include "VisualProfiler.h"

#include "DirectX11Image.h"
#include "DirectX11RenderTarget.h"
#include "DirectX11Shader.h"
#include "DirectX11VertexArrayObject.h"

#include "shaders.h"

#include <string_view>

#if 1  // defined(_WINVER) && _WINVER < 0x0A00
#define NO_FLIP \
    true  // FIXME: for some reason, perf is lower with FLIP_DISCARD than DISCARD (def. doing something wrong)
#else
#define NO_FLIP false
#endif

// #define D3D11_DEBUG

DirectX11Interface::DirectX11Interface(HWND hwnd) : Graphics(), hwnd(hwnd) {}

void DirectX11Interface::init() {
    if(!DirectX11Shader::loadLibs()) return;

    static constexpr std::array<D3D_FEATURE_LEVEL, 4> FEATURE_LEVELS11_1{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    // flags
    UINT createDeviceFlags = 0;
    createDeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef D3D11_DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_DEBUGGABLE;
#endif

    // create device + context

    std::string error = "D3D11CreateDevice";
    D3D_FEATURE_LEVEL featureLevelOut;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                                   FEATURE_LEVELS11_1.data(), FEATURE_LEVELS11_1.size(), D3D11_SDK_VERSION,
                                   &this->device, &featureLevelOut, &this->deviceContext);

    if(hr == E_INVALIDARG) {  // try without 11_1
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                               FEATURE_LEVELS11_1.data() + 1, FEATURE_LEVELS11_1.size() - 1, D3D11_SDK_VERSION,
                               &this->device, &featureLevelOut, &this->deviceContext);
    }

    if(SUCCEEDED(hr)) {
        error = "device->QueryInterface(IDXGIDevice)";
        hr = this->device->QueryInterface(__uuidof(IDXGIDevice), (void **)&this->dxgiDevice);
    }
    if(SUCCEEDED(hr)) {
        error = "dxgiDevice->GetAdapter()";
        hr = this->dxgiDevice->GetAdapter(&this->dxgiAdapter);
    }

    if(SUCCEEDED(hr)) {
        error = "dxgiAdapter->GetParent(IDXGIFactory2)";
        hr = this->dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&this->dxgiFactory);
    }
    if(FAILED(hr)) {
        UString errorTitle = "DirectX Error";
        UString errorMessage =
            fmt::format("{} failed! HR: {:#x} DXGI_HR: {:#x})", error, (u32)hr, (u32)MAKE_DXGI_HRESULT(hr));

        errorMessage.append("\nThe engine will quit now.");
        engine->showMessageErrorFatal("DirectX Error", errorMessage);
        engine->shutdown();

        return;
    }

    hr = this->device->QueryInterface(__uuidof(IDXGIDevice1), (void **)&this->dxgiDevice1);

    if(FAILED(hr)) {
        debugLog(
            "Disabling support for frame pacing (couldn't device->QueryInterface(IDXGIDevice): HR: {:#x} DXGI_HR: "
            "{:#x})",
            (u32)hr, (u32)MAKE_DXGI_HRESULT(hr));
    } else {
        this->dxgiDevice1->SetMaximumFrameLatency(this->iMaxFrameLatency);

        cv::r_sync_max_frames.setDefaultDouble(this->iMaxFrameLatency);
        cv::r_sync_enabled.setDefaultDouble((double)!this->bFrameLatencyDisabled);

        cv::r_sync_max_frames.setCallback(SA::MakeDelegate<&DirectX11Interface::onFramecountNumChanged>(this));
        cv::r_sync_enabled.setCallback(SA::MakeDelegate<&DirectX11Interface::onSyncBehaviorChanged>(this));
    }

    this->device->CreateRasterizerState(&this->rasterizerDesc, &this->rasterizerState);
    this->deviceContext->RSSetState(this->rasterizerState);

    this->device->CreateDepthStencilState(&this->depthStencilDesc, &this->depthStencilState);
    this->deviceContext->OMSetDepthStencilState(this->depthStencilState,
                                                0);  // for 0 see StencilReadMask, StencilWriteMask

    this->device->CreateBlendState(&this->blendDesc, &this->blendState);
    this->deviceContext->OMSetBlendState(this->blendState, nullptr, D3D11_DEFAULT_SAMPLE_MASK);

    // create default shader
    const auto vertexShader = std::string{reinterpret_cast<const char *>(DX11_default_vsh), DX11_default_vsh_size()};
    const auto pixelShader = std::string{reinterpret_cast<const char *>(DX11_default_fsh), DX11_default_fsh_size()};

    this->shaderTexturedGeneric = static_cast<DirectX11Shader *>(createShaderFromSource(vertexShader, pixelShader));
    this->shaderTexturedGeneric->load();

    if(!this->shaderTexturedGeneric->isReady()) {
        engine->showMessageErrorFatal("DirectX Error", "Failed to create default shader!\nThe engine will quit now.");
        engine->shutdown();
        return;
    }

    if(FAILED(this->device->CreateBuffer(&this->vertexBufferDesc, nullptr, &this->vertexBuffer))) {
        engine->showMessageErrorFatal("DirectX Error",
                                      "Failed to create default vertex buffer!\nThe engine will quit now.");
        engine->shutdown();
        return;
    }
    // defer swapchain creation until drawing actually begins
}

bool DirectX11Interface::createSwapchain() {
    UINT startingWidth = 0;  // 0x0 to create with window size
    UINT startingHeight = 0;

    // dxvk-native throws an exception on wayland that we can't catch...
    // i shouldn't be wasting time working around external bugs, but this is one of them
    BOOL startWindowed = env->isWayland() || !env->winFullscreened();
    if(!startWindowed) {
        vec2 desktopRect = env->getNativeScreenSize();
        startingWidth = (UINT)desktopRect.x;
        startingHeight = (UINT)desktopRect.y;
    }

    DXGI_SWAP_CHAIN_DESC1 swapchainCreateDesc{
        .Width = startingWidth,
        .Height = startingHeight,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = 0,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = NO_FLIP ? 1 : 2,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = NO_FLIP ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags =
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | (Env::cfg(OS::WINDOWS) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0),
    };

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc{
        .RefreshRate = {.Numerator = 0, .Denominator = 1},
        .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE,
        .Scaling = DXGI_MODE_SCALING_CENTERED,
        .Windowed = startWindowed,
    };

    auto hr = this->dxgiFactory->CreateSwapChainForHwnd(this->device, this->hwnd, &swapchainCreateDesc, &fsDesc,
                                                        nullptr, &this->swapChain);

    if(FAILED(hr)) {
        engine->showMessageErrorFatal(
            "DirectX Error",
            fmt::format("Failed to create a swapchain: HR: {:#x} DXGI_HR: {:#x})\nThe engine will shut down now.",
                        (u32)hr, (u32)MAKE_DXGI_HRESULT(hr)));
        engine->shutdown();
        return false;
    }

    // disable hardcoded DirectX ALT + ENTER fullscreen toggle functionality (this is instead handled by the engine internally)
    // disable dxgi interfering with mode changes and WndProc (again, handled by the engine internally)
    this->dxgiFactory->MakeWindowAssociation(this->hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

    // NOTE: force build swapchain rendertarget for first time
    onResolutionChange(this->vResolution);
    return true;
}

DirectX11Interface::~DirectX11Interface() {
    if(this->swapChain) this->swapChain->SetFullscreenState(FALSE, nullptr);

    SAFE_DELETE(this->shaderTexturedGeneric);

    if(this->vertexBuffer) this->vertexBuffer->Release();
    if(this->rasterizerState) this->rasterizerState->Release();
    if(this->swapChain) this->swapChain->Release();
    if(this->frameBuffer) this->frameBuffer->Release();
    if(this->frameBufferDepthStencilView) this->frameBufferDepthStencilView->Release();
    if(this->frameBufferDepthStencilTexture) this->frameBufferDepthStencilTexture->Release();
    if(this->dxgiDevice) this->dxgiDevice->Release();
    if(this->dxgiAdapter) this->dxgiAdapter->Release();
    if(this->dxgiFactory) this->dxgiFactory->Release();
    if(this->device) this->device->Release();
    if(this->deviceContext) this->deviceContext->Release();

    DirectX11Shader::cleanupLibs();
}

void DirectX11Interface::beginScene() {
    // create initial swapchain if we haven't already
    if(!this->swapChain && !this->createSwapchain()) return;

#ifndef NO_FLIP
    // ensure render targets are bound (needed because onResolutionChange might skip setup during init)
    if(this->frameBuffer)
        this->deviceContext->OMSetRenderTargets(1, &this->frameBuffer, this->frameBufferDepthStencilView);
#endif

    Matrix4 defaultProjectionMatrix =
        Camera::buildMatrixOrtho2DDXLH(0, this->vResolution.x, this->vResolution.y, 0, -1.0f, 1.0f);

    // push main transforms
    pushTransform();
    setProjectionMatrix(defaultProjectionMatrix);
    translate(cv::r_globaloffset_x.getFloat(), cv::r_globaloffset_y.getFloat());

    // and apply them
    this->updateTransform();

    // clear
    static constexpr std::array<float, 4> clearColor{};
    if(this->frameBuffer) this->deviceContext->ClearRenderTargetView(this->frameBuffer, clearColor.data());
    if(this->frameBufferDepthStencilView)
        this->deviceContext->ClearDepthStencilView(this->frameBufferDepthStencilView,
                                                   D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,
                                                   0);  // yes, the 1.0f is correct

    // enable default shader
    this->shaderTexturedGeneric->enable();

    // prev frame render stats
    const int numDrawCallsPrevFrame = this->iStatsNumDrawCalls;
    this->iStatsNumDrawCalls = 0;
    if(vprof && vprof->isEnabled()) {
        int numActiveShaders = 1;
        for(const Resource *shader : resourceManager->getShaders()) {
            const auto *dx11Shader = static_cast<const DirectX11Shader *>(shader);
            if(dx11Shader->getStatsNumConstantBufferUploadsPerFrameEngineFrameCount() == (engine->getFrameCount() - 1))
                numActiveShaders++;
        }

        int shaderCounter = 0;
        vprof->addInfoBladeEngineTextLine(UString::format("Draw Calls: %i", numDrawCallsPrevFrame));
        vprof->addInfoBladeEngineTextLine(UString::format("Active Shaders: %i", numActiveShaders));
        vprof->addInfoBladeEngineTextLine(
            UString::format("shader[%i]: shaderTexturedGeneric: %ic", shaderCounter++,
                            (int)this->shaderTexturedGeneric->getStatsNumConstantBufferUploadsPerFrame()));
        for(const Resource *shader : resourceManager->getShaders()) {
            const auto *dx11Shader = static_cast<const DirectX11Shader *>(shader);
            if(dx11Shader->getStatsNumConstantBufferUploadsPerFrameEngineFrameCount() == (engine->getFrameCount() - 1))
                vprof->addInfoBladeEngineTextLine(
                    UString::format("shader[%i]: %s: %ic", shaderCounter++, shader->getName().c_str(),
                                    (int)dx11Shader->getStatsNumConstantBufferUploadsPerFrame()));
        }
    }
}

void DirectX11Interface::endScene() {
    this->popTransform();

    UINT presentFlags = DXGI_PRESENT_DO_NOT_WAIT;
    if(!this->bVSync && (this->bIsFullscreen && !this->bIsFullscreenBorderlessWindowed)) {
        presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    [[maybe_unused]] auto swapHR = this->swapChain->Present(this->bVSync, presentFlags);
#if defined(_DEBUG) || defined(D3D11_DEBUG)
    if(FAILED(swapHR)) {
        debugLog("WARNING: Present( {}, {:#x} ) gave HRESULT: {:#x}", this->bVSync, presentFlags,
                 static_cast<uint32_t>(MAKE_DXGI_HRESULT(swapHR)));
    }
    this->checkStackLeaks();

    if(this->clipRectStack.size() > 0) {
        engine->showMessageErrorFatal("ClipRect Stack Leak", "Make sure all push*() have a pop*()!");
        engine->shutdown();
    }
#endif

    // aka checkErrors()
#ifdef D3D11_DEBUG
    constexpr auto maxFails = 5;  // spam prevention
    static auto failCount = 0;

    ID3D11Debug *d3dDebug = nullptr;
    auto hr = this->device->QueryInterface(__uuidof(ID3D11Debug), (void **)&d3dDebug);
    if(FAILED(hr)) {
        if(failCount++ < maxFails)
            debugLog("DirectX Error: Couldn't device->QueryInterface( ID3D11Debug ) {:#x}", static_cast<uint32_t>(hr));
        return;
    }

    ID3D11InfoQueue *debugInfoQueue = nullptr;
    hr = d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&debugInfoQueue);
    if(FAILED(hr)) {
        d3dDebug->Release();
        if(failCount++ < maxFails)
            debugLog("DirectX Error: Couldn't d3dDebug->QueryInterface( ID3D11InfoQueue ) {:#x}",
                     static_cast<uint32_t>(hr));
        return;
    }

    UINT64 message_count = debugInfoQueue->GetNumStoredMessages();

    for(UINT64 i = 0; i < message_count; i++) {
        SIZE_T message_size = 0;
        debugInfoQueue->GetMessage(i, nullptr, &message_size);

        D3D11_MESSAGE *message = (D3D11_MESSAGE *)calloc(message_size + 1, 1);
        hr = debugInfoQueue->GetMessage(i, message, &message_size);
        if(SUCCEEDED(hr))
            debugLog("DirectX11Debug: {:s}", message->pDescription);
        else
            debugLog("DirectX Error: Couldn't debugInfoQueue->GetMessage() {:#x}", static_cast<uint32_t>(hr));

        free(message);
    }

    debugInfoQueue->ClearStoredMessages();
    debugInfoQueue->Release();
    d3dDebug->Release();
#endif
}

void DirectX11Interface::clearDepthBuffer() {
    if(this->frameBufferDepthStencilView)
        this->deviceContext->ClearDepthStencilView(this->frameBufferDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f,
                                                   0);  // yes, the 1.0f is correct
}

void DirectX11Interface::setColor(Color color) {
    if(this->color == color) return;

    this->color = color;
    this->shaderTexturedGeneric->setUniform4f("col", this->color.Af(), this->color.Rf(), this->color.Gf(),
                                              this->color.Bf());
}

void DirectX11Interface::setAlpha(float alpha) {
    this->color &= 0x00ffffff;
    this->color |= ((int)(255.0f * alpha)) << 24;

    this->setColor(this->color);
}

void DirectX11Interface::drawPixel(int x, int y) {
    this->updateTransform();

    this->setTexturing(false);  // disable texturing

    // build directx vertices
    this->vertices.clear();

    this->vertices.push_back(SimpleVertex{
        .pos = {static_cast<float>(x), static_cast<float>(y), 0.f},
        .col = {this->color.Rf(), this->color.Gf(), this->color.Bf(), this->color.Af()},
        .tex = {0.0f, 0.0f},
    });

    // upload everything to gpu
    size_t numVertexOffset = 0;
    bool uploadedSuccessfully = true;
    if(this->vertexBufferDesc.Usage == D3D11_USAGE_DEFAULT) {
        D3D11_BOX box;
        {
            box.left = sizeof(DirectX11Interface::SimpleVertex) * 0;
            box.right = box.left + (sizeof(DirectX11Interface::SimpleVertex) * this->vertices.size());
            box.top = 0;
            box.bottom = 1;
            box.front = 0;
            box.back = 1;
        }
        this->deviceContext->UpdateSubresource(this->vertexBuffer, 0, &box, &this->vertices[0], 0, 0);
    } else {
        const bool needsDiscardEntireBuffer =
            (this->iVertexBufferNumVertexOffsetCounter + this->vertices.size() > MAX_VERTEX_BUFFER_VERTS);
        const size_t writeOffsetNumVertices =
            (needsDiscardEntireBuffer ? 0 : this->iVertexBufferNumVertexOffsetCounter);
        numVertexOffset = writeOffsetNumVertices;
        {
            D3D11_MAPPED_SUBRESOURCE mappedResource{};
            if(SUCCEEDED(this->deviceContext->Map(
                   this->vertexBuffer, 0,
                   (needsDiscardEntireBuffer ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE), 0,
                   &mappedResource))) {
                memcpy((void *)(((SimpleVertex *)mappedResource.pData) + writeOffsetNumVertices), &this->vertices[0],
                       sizeof(DirectX11Interface::SimpleVertex) * this->vertices.size());
                this->deviceContext->Unmap(this->vertexBuffer, 0);
            } else
                uploadedSuccessfully = false;
        }
        this->iVertexBufferNumVertexOffsetCounter = writeOffsetNumVertices + this->vertices.size();
    }

    // shader update
    if(uploadedSuccessfully) {
        if(this->activeShader) this->activeShader->onJustBeforeDraw();
    }

    // draw it
    if(uploadedSuccessfully) {
        const UINT stride = sizeof(SimpleVertex);
        const UINT offset = 0;

        this->deviceContext->IASetVertexBuffers(0, 1, &this->vertexBuffer, &stride, &offset);
        this->deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        this->deviceContext->Draw(this->vertices.size(), numVertexOffset);
        this->iStatsNumDrawCalls++;
    }
}

void DirectX11Interface::drawPixels(int /*x*/, int /*y*/, int /*width*/, int /*height*/,
                                    Graphics::DRAWPIXELS_TYPE /*type*/, const void * /*pixels*/) {
    // TODO: implement
}

void DirectX11Interface::drawLinef(float x1, float y1, float x2, float y2) {
    this->updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_LINES);
    {
        vao.clear();

        vao.addVertex(x1, y1);
        vao.addVertex(x2, y2);
    }
    this->drawVAO(&vao);
}

void DirectX11Interface::drawRectf(const RectOptions &opts) {
    this->updateTransform();

    // for line thickness > 1, draw as filled rectangles since D3D11 doesn't support variable line widths
    if(opts.lineThickness > 1.0f) {
        this->setTexturing(false);

        const float halfThickness = opts.lineThickness * 0.5f;

        if(opts.withColor) {
            // top edge
            this->setColor(opts.top);
            this->fillRectf(opts.x - halfThickness, opts.y - halfThickness, opts.width + opts.lineThickness,
                            opts.lineThickness);

            // bottom edge
            this->setColor(opts.bottom);
            this->fillRectf(opts.x - halfThickness, opts.y + opts.height - halfThickness,
                            opts.width + opts.lineThickness, opts.lineThickness);

            // left edge
            this->setColor(opts.left);
            this->fillRectf(opts.x - halfThickness, opts.y + halfThickness, opts.lineThickness,
                            opts.height - opts.lineThickness);

            // right edge
            this->setColor(opts.right);
            this->fillRectf(opts.x + opts.width - halfThickness, opts.y + halfThickness, opts.lineThickness,
                            opts.height - opts.lineThickness);
        } else {
            // all edges same color
            // top edge
            this->fillRectf(opts.x - halfThickness, opts.y - halfThickness, opts.width + opts.lineThickness,
                            opts.lineThickness);

            // bottom edge
            this->fillRectf(opts.x - halfThickness, opts.y + opts.height - halfThickness,
                            opts.width + opts.lineThickness, opts.lineThickness);

            // left edge
            this->fillRectf(opts.x - halfThickness, opts.y + halfThickness, opts.lineThickness,
                            opts.height - opts.lineThickness);

            // right edge
            this->fillRectf(opts.x + opts.width - halfThickness, opts.y + halfThickness, opts.lineThickness,
                            opts.height - opts.lineThickness);
        }
    } else {
        // fallback to line drawing for thickness == 1
        if(opts.withColor) {
            this->setColor(opts.top);
            this->drawLinef(opts.x, opts.y, opts.x + opts.width, opts.y);
            this->setColor(opts.left);
            this->drawLinef(opts.x, opts.y, opts.x, opts.y + opts.height);
            this->setColor(opts.bottom);
            this->drawLinef(opts.x, opts.y + opts.height, opts.x + opts.width, opts.y + opts.height + 0.5f);
            this->setColor(opts.right);
            this->drawLinef(opts.x + opts.width, opts.y, opts.x + opts.width, opts.y + opts.height + 0.5f);
        } else {
            this->drawLinef(opts.x, opts.y, opts.x + opts.width, opts.y);
            this->drawLinef(opts.x, opts.y, opts.x, opts.y + opts.height);
            this->drawLinef(opts.x, opts.y + opts.height, opts.x + opts.width, opts.y + opts.height + 0.5f);
            this->drawLinef(opts.x + opts.width, opts.y, opts.x + opts.width, opts.y + opts.height + 0.5f);
        }
    }
}

void DirectX11Interface::fillRectf(float x, float y, float width, float height) {
    this->updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.clear();

        vao.addVertex(x, y);
        vao.addVertex(x, y + height);
        vao.addVertex(x + width, y + height);
        vao.addVertex(x + width, y);
    }
    this->drawVAO(&vao);
}

void DirectX11Interface::fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                                      Color bottomLeftColor, Color bottomRightColor) {
    this->updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.clear();

        vao.addVertex(x, y);
        vao.addColor(topLeftColor);
        vao.addVertex(x + width, y);
        vao.addColor(topRightColor);
        vao.addVertex(x + width, y + height);
        vao.addColor(bottomRightColor);
        vao.addVertex(x, y + height);
        vao.addColor(bottomLeftColor);
    }
    this->drawVAO(&vao);
}

void DirectX11Interface::drawQuad(int x, int y, int width, int height) {
    this->updateTransform();

    this->setTexturing(true);  // enable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.clear();

        vao.addVertex(x, y);
        vao.addTexcoord(0, 0);
        vao.addVertex(x, y + height);
        vao.addTexcoord(0, 1);
        vao.addVertex(x + width, y + height);
        vao.addTexcoord(1, 1);
        vao.addVertex(x + width, y);
        vao.addTexcoord(1, 0);
    }
    this->drawVAO(&vao);
}

void DirectX11Interface::drawQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft, Color topLeftColor,
                                  Color topRightColor, Color bottomRightColor, Color bottomLeftColor) {
    this->updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.clear();

        vao.addVertex(topLeft.x, topLeft.y);
        vao.addColor(topLeftColor);
        // vao.addTexcoord(0, 0);
        vao.addVertex(bottomLeft.x, bottomLeft.y);
        vao.addColor(bottomLeftColor);
        // vao.addTexcoord(0, 1);
        vao.addVertex(bottomRight.x, bottomRight.y);
        vao.addColor(bottomRightColor);
        // vao.addTexcoord(1, 1);
        vao.addVertex(topRight.x, topRight.y);
        vao.addColor(topRightColor);
        // vao.addTexcoord(1, 0);
    }
    this->drawVAO(&vao);
}

void DirectX11Interface::drawImage(const Image *image, AnchorPoint anchor, float edgeSoftness, McRect clipRect) {
    if(image == nullptr) {
        debugLog("WARNING: Tried to draw image with NULL texture!");
        return;
    }
    if(!image->isReady()) return;

    const bool clipRectSpecified = vec::length(clipRect.getSize()) != 0;
    bool smoothedEdges = edgeSoftness > 0.0f;

    // initialize shader on first use
    if(smoothedEdges) {
        if(!this->smoothClipShader) {
            this->initSmoothClipShader();
        }
        smoothedEdges = this->smoothClipShader->isReady();
    }

    const bool fallbackClip = clipRectSpecified && !smoothedEdges;

    if(fallbackClip) {
        this->pushClipRect(clipRect);
    }

    this->updateTransform();

    this->setTexturing(true);  // enable texturing

    const float width = image->getWidth();
    const float height = image->getHeight();

    f32 x{}, y{};
    switch(anchor) {
        case AnchorPoint::CENTER:
            x = -width / 2;
            y = -height / 2;
            break;
        case AnchorPoint::TOP_LEFT:
            x = 0;
            y = 0;
            break;
        case AnchorPoint::TOP_RIGHT:
            x = -width;
            y = 0;
            break;
        case AnchorPoint::BOTTOM_LEFT:
            x = 0;
            y = -height;
            break;
        case AnchorPoint::LEFT:
            x = 0;
            y = -height / 2;
            break;
        default:
            abort();
    }

    if(smoothedEdges && !clipRectSpecified) {
        // set a default clip rect as the exact image size if one wasn't explicitly passed, but we still want smoothing
        clipRect = McRect{x, y, width, height};
    }

    if(smoothedEdges) {
        // DirectX uses top-left origin, so no Y-flipping needed
        D3D11_VIEWPORT viewport;
        UINT numViewports = 1;
        // maybe inefficient? could be cached like opengl
        this->deviceContext->RSGetViewports(&numViewports, &viewport);

        float clipMinX = (clipRect.getX() + viewport.TopLeftX) - .5f;  // i don't know... weird rounding
        float clipMinY = (clipRect.getY() + viewport.TopLeftY) - .5f;
        float clipMaxX = (clipMinX + clipRect.getWidth()) + .5f;
        float clipMaxY = (clipMinY + clipRect.getHeight()) + .5f;

        this->smoothClipShader->enable();
        this->smoothClipShader->setUniform2f("rect_min", clipMinX, clipMinY);
        this->smoothClipShader->setUniform2f("rect_max", clipMaxX, clipMaxY);
        this->smoothClipShader->setUniform1f("edge_softness", edgeSoftness);

        // set mvp for the shader
        this->smoothClipShader->setUniformMatrix4fv("mvp", this->MP);
    }

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.clear();

        vao.addVertex(x, y);
        vao.addTexcoord(0, 0);
        vao.addVertex(x, y + height);
        vao.addTexcoord(0, 1);
        vao.addVertex(x + width, y + height);
        vao.addTexcoord(1, 1);
        vao.addVertex(x + width, y);
        vao.addTexcoord(1, 0);
    }

    image->bind();
    {
        this->drawVAO(&vao);
    }
    image->unbind();

    if(smoothedEdges) {
        this->smoothClipShader->disable();
    } else if(fallbackClip) {
        this->popClipRect();
    }

    if(cv::r_debug_drawimage.getBool()) {
        this->setColor(0xbbff00ff);
        Graphics::drawRectf(x, y, width, height);
    }
}

void DirectX11Interface::drawString(McFont *font, const UString &text) {
    if(font == nullptr || text.length() < 1 || !font->isReady()) return;

    this->updateTransform();

    this->setTexturing(true);  // enable texturing

    font->drawString(text);
}

void DirectX11Interface::drawVAO(VertexArrayObject *vao) {
    if(vao == nullptr) return;

    this->updateTransform();

    // if baked, then we can directly draw the buffer
    if(vao->isReady()) {
        // shader update
        if(this->activeShader) this->activeShader->onJustBeforeDraw();

        vao->draw();
        return;
    }

    const std::vector<vec3> &vertices = vao->getVertices();
    /// const std::vector<vec3> &normals = vao->getNormals();
    const std::vector<vec2> &texcoords = vao->getTexcoords();
    const std::vector<Color> &vcolors = vao->getColors();

    if(vertices.size() < 2) return;

    // TODO: optimize this piece of shit

    // no support for quads, because fuck you
    // no support for triangle fans, because fuck youuu
    // rewrite all quads into triangles
    // rewrite all triangle fans into triangles
    static std::vector<vec3> finalVertices;
    finalVertices = vertices;
    static std::vector<vec2> finalTexcoords;
    finalTexcoords = texcoords;
    static std::vector<vec4> colors;
    colors.clear();
    static std::vector<vec4> finalColors;
    finalColors.clear();

    for(auto vcolor : vcolors) {
        const vec4 color = vec4(vcolor.Rf(), vcolor.Gf(), vcolor.Bf(), vcolor.Af());
        colors.push_back(color);
        finalColors.push_back(color);
    }
    const size_t maxColorIndex = (colors.size() > 0 ? colors.size() - 1 : 0);

    Graphics::PRIMITIVE primitive = vao->getPrimitive();
    if(primitive == Graphics::PRIMITIVE::PRIMITIVE_QUADS) {
        finalVertices.clear();
        finalTexcoords.clear();
        finalColors.clear();
        primitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES;

        if(vertices.size() > 3) {
            for(size_t i = 0; i < vertices.size(); i += 4) {
                finalVertices.push_back(vertices[i + 0]);
                finalVertices.push_back(vertices[i + 1]);
                finalVertices.push_back(vertices[i + 2]);

                if(!texcoords.empty()) {
                    finalTexcoords.push_back(texcoords[i + 0]);
                    finalTexcoords.push_back(texcoords[i + 1]);
                    finalTexcoords.push_back(texcoords[i + 2]);
                }

                if(colors.size() > 0) {
                    finalColors.push_back(colors[std::clamp<size_t>(i + 0, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i + 1, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i + 2, 0, maxColorIndex)]);
                }

                finalVertices.push_back(vertices[i + 0]);
                finalVertices.push_back(vertices[i + 2]);
                finalVertices.push_back(vertices[i + 3]);

                if(!texcoords.empty()) {
                    finalTexcoords.push_back(texcoords[i + 0]);
                    finalTexcoords.push_back(texcoords[i + 2]);
                    finalTexcoords.push_back(texcoords[i + 3]);
                }

                if(colors.size() > 0) {
                    finalColors.push_back(colors[std::clamp<size_t>(i + 0, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i + 2, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i + 3, 0, maxColorIndex)]);
                }
            }
        }
    } else if(primitive == Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN) {
        finalVertices.clear();
        finalTexcoords.clear();
        finalColors.clear();
        primitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES;

        if(vertices.size() > 2) {
            for(size_t i = 2; i < vertices.size(); i++) {
                finalVertices.push_back(vertices[0]);

                finalVertices.push_back(vertices[i]);
                finalVertices.push_back(vertices[i - 1]);

                if(!texcoords.empty()) {
                    finalTexcoords.push_back(texcoords[0]);
                    finalTexcoords.push_back(texcoords[i]);
                    finalTexcoords.push_back(texcoords[i - 1]);
                }

                if(colors.size() > 0) {
                    finalColors.push_back(colors[std::clamp<size_t>(0, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i - 1, 0, maxColorIndex)]);
                }
            }
        }
    }

    // build directx vertices
    const bool hasTexcoords0 = (finalTexcoords.size() > 0);
    this->vertices.resize(finalVertices.size());
    {
        const bool hasColors = (finalColors.size() > 0);

        const size_t maxColorIndex = (hasColors ? finalColors.size() - 1 : 0);
        const size_t maxTexcoords0Index = (hasTexcoords0 ? finalTexcoords.size() - 1 : 0);

        const vec4 color = vec4(this->color.Rf(), this->color.Gf(), this->color.Bf(), this->color.Af());

        for(size_t i = 0; i < finalVertices.size(); i++) {
            this->vertices[i].pos = finalVertices[i];

            if(hasColors)
                this->vertices[i].col = finalColors[std::clamp<size_t>(i, 0, maxColorIndex)];
            else
                this->vertices[i].col = color;

            // TODO: multitexturing
            if(hasTexcoords0) this->vertices[i].tex = finalTexcoords[std::clamp<size_t>(i, 0, maxTexcoords0Index)];
        }
    }

    // upload everything to gpu
    size_t numVertexOffset = 0;
    bool uploadedSuccessfully = true;
    {
        if(this->vertexBufferDesc.Usage == D3D11_USAGE_DEFAULT) {
            D3D11_BOX box;
            {
                box.left = sizeof(DirectX11Interface::SimpleVertex) * 0;
                box.right = box.left + (sizeof(DirectX11Interface::SimpleVertex) * this->vertices.size());
                box.top = 0;
                box.bottom = 1;
                box.front = 0;
                box.back = 1;
            }
            this->deviceContext->UpdateSubresource(this->vertexBuffer, 0, &box, &this->vertices[0], 0, 0);
        } else {
            const bool needsDiscardEntireBuffer =
                (this->iVertexBufferNumVertexOffsetCounter + this->vertices.size() > MAX_VERTEX_BUFFER_VERTS);
            const size_t writeOffsetNumVertices =
                (needsDiscardEntireBuffer ? 0 : this->iVertexBufferNumVertexOffsetCounter);
            numVertexOffset = writeOffsetNumVertices;
            {
                D3D11_MAPPED_SUBRESOURCE mappedResource{};
                if(SUCCEEDED(this->deviceContext->Map(
                       this->vertexBuffer, 0,
                       (needsDiscardEntireBuffer ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE), 0,
                       &mappedResource))) {
                    memcpy((void *)(((SimpleVertex *)mappedResource.pData) + writeOffsetNumVertices),
                           &this->vertices[0], sizeof(DirectX11Interface::SimpleVertex) * this->vertices.size());
                    this->deviceContext->Unmap(this->vertexBuffer, 0);
                } else
                    uploadedSuccessfully = false;
            }
            this->iVertexBufferNumVertexOffsetCounter = writeOffsetNumVertices + this->vertices.size();
        }

        // shader update
        if(uploadedSuccessfully) {
            this->setTexturing(hasTexcoords0);

            if(this->activeShader) this->activeShader->onJustBeforeDraw();
        }
    }

    // draw it
    if(uploadedSuccessfully) {
        const UINT stride = sizeof(SimpleVertex);
        const UINT offset = 0;

        this->deviceContext->IASetVertexBuffers(0, 1, &this->vertexBuffer, &stride, &offset);
        this->deviceContext->IASetPrimitiveTopology((D3D_PRIMITIVE_TOPOLOGY)primitiveToDirectX(primitive));
        this->deviceContext->Draw(this->vertices.size(), numVertexOffset);
        this->iStatsNumDrawCalls++;
    }
}

void DirectX11Interface::setClipRect(McRect clipRect) {
    if(cv::r_debug_disable_cliprect.getBool()) return;
    // if (m_bIs3DScene) return; // HACKHACK: TODO:

    this->setClipping(true);

    D3D11_RECT rect;
    {
        rect.left = clipRect.getMinX();
        rect.top = clipRect.getMinY() - 1;
        rect.right = clipRect.getMaxX();
        rect.bottom = clipRect.getMaxY() - 1;
    }
    this->deviceContext->RSSetScissorRects(1, &rect);
}

void DirectX11Interface::pushClipRect(McRect clipRect) {
    if(this->clipRectStack.size() > 0)
        this->clipRectStack.push(this->clipRectStack.top().intersect(clipRect));
    else
        this->clipRectStack.push(clipRect);

    this->setClipRect(this->clipRectStack.top());
}

void DirectX11Interface::popClipRect() {
    this->clipRectStack.pop();

    if(this->clipRectStack.size() > 0)
        this->setClipRect(this->clipRectStack.top());
    else
        this->setClipping(false);
}

void DirectX11Interface::setClipping(bool enabled) {
    if(enabled) {
        if(this->clipRectStack.size() < 1) enabled = false;
    }

    this->rasterizerState->Release();
    this->rasterizerDesc.ScissorEnable = (enabled ? TRUE : FALSE);
    this->device->CreateRasterizerState(&this->rasterizerDesc, &this->rasterizerState);
    this->deviceContext->RSSetState(this->rasterizerState);
}

void DirectX11Interface::setAlphaTesting(bool /*enabled*/) {
    // TODO: implement in default shader
}

void DirectX11Interface::setAlphaTestFunc(COMPARE_FUNC /*alphaFunc*/, float /*ref*/) {
    // TODO: implement in default shader
}

void DirectX11Interface::setBlending(bool enabled) {
    this->blendState->Release();
    this->blendDesc.RenderTarget[0].BlendEnable = (enabled ? TRUE : FALSE);
    this->device->CreateBlendState(&this->blendDesc, &this->blendState);
    this->deviceContext->OMSetBlendState(this->blendState, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
}

void DirectX11Interface::setBlendMode(BLEND_MODE blendMode) {
    this->blendState->Release();

    auto &blendDescRT0 = this->blendDesc.RenderTarget[0];
    switch(blendMode) {
        case BLEND_MODE::BLEND_MODE_ALPHA: {
            blendDescRT0.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            blendDescRT0.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blendDescRT0.BlendOp = D3D11_BLEND_OP_ADD;

            blendDescRT0.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
            blendDescRT0.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blendDescRT0.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        } break;

        case BLEND_MODE::BLEND_MODE_ADDITIVE: {
            blendDescRT0.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            blendDescRT0.DestBlend = D3D11_BLEND_ONE;
            blendDescRT0.BlendOp = D3D11_BLEND_OP_ADD;

            blendDescRT0.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
            blendDescRT0.DestBlendAlpha = D3D11_BLEND_ONE;
            blendDescRT0.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        } break;

        case BLEND_MODE::BLEND_MODE_PREMUL_ALPHA: {
            blendDescRT0.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            blendDescRT0.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blendDescRT0.BlendOp = D3D11_BLEND_OP_ADD;

            blendDescRT0.SrcBlendAlpha = D3D11_BLEND_ONE;
            blendDescRT0.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blendDescRT0.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        } break;

        case BLEND_MODE::BLEND_MODE_PREMUL_COLOR: {
            blendDescRT0.SrcBlend = D3D11_BLEND_ONE;
            blendDescRT0.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blendDescRT0.BlendOp = D3D11_BLEND_OP_ADD;

            blendDescRT0.SrcBlendAlpha = D3D11_BLEND_ONE;
            blendDescRT0.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blendDescRT0.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        } break;
    }

    this->device->CreateBlendState(&this->blendDesc, &this->blendState);
    this->deviceContext->OMSetBlendState(this->blendState, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
}

void DirectX11Interface::setDepthBuffer(bool enabled) {
    this->depthStencilState->Release();
    this->depthStencilDesc.DepthEnable = (enabled ? TRUE : FALSE);
    this->depthStencilDesc.DepthWriteMask = (enabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO);
    this->device->CreateDepthStencilState(&this->depthStencilDesc, &this->depthStencilState);
    this->deviceContext->OMSetDepthStencilState(this->depthStencilState,
                                                0);  // for 0 see StencilReadMask, StencilWriteMask
}

void DirectX11Interface::setCulling(bool culling) {
    this->rasterizerState->Release();
    this->rasterizerDesc.CullMode = (culling ? D3D11_CULL_BACK : D3D11_CULL_NONE);
    this->device->CreateRasterizerState(&this->rasterizerDesc, &this->rasterizerState);
    this->deviceContext->RSSetState(this->rasterizerState);
}

void DirectX11Interface::setColorWriting(bool /*r*/, bool /*g*/, bool /*b*/, bool /*a*/) { MC_MESSAGE("TODO") }

void DirectX11Interface::setColorInversion(bool enabled) {
    if(this->bColorInversion == enabled) return;

    this->bColorInversion = enabled;
    this->setTexturing(this->bTexturingEnabled);  // re-apply with new inversion state
}

void DirectX11Interface::setDepthWriting(bool /*enabled*/) { MC_MESSAGE("TODO") }

void DirectX11Interface::setAntialiasing(bool aa) {
    this->rasterizerState->Release();
    this->rasterizerDesc.MultisampleEnable = (aa ? TRUE : FALSE);
    this->device->CreateRasterizerState(&this->rasterizerDesc, &this->rasterizerState);
    this->deviceContext->RSSetState(this->rasterizerState);
}

void DirectX11Interface::setWireframe(bool enabled) {
    this->rasterizerState->Release();
    this->rasterizerDesc.FillMode = (enabled ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID);
    this->device->CreateRasterizerState(&this->rasterizerDesc, &this->rasterizerState);
    this->deviceContext->RSSetState(this->rasterizerState);
}

void DirectX11Interface::setLineWidth(float /*width*/) { MC_MESSAGE("TODO"); }

void DirectX11Interface::flush() { this->deviceContext->Flush(); }

std::vector<u8> DirectX11Interface::getScreenshot(bool /*withAlpha*/) {
    ID3D11Texture2D *backBuffer = nullptr;
    if(!this->swapChain || FAILED(this->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&backBuffer)) ||
       !backBuffer) {
        return {};
    }

    bool success = false;
    std::vector<u8> result;

    D3D11_TEXTURE2D_DESC backBufferDesc;
    backBuffer->GetDesc(&backBufferDesc);
    {
        backBufferDesc.Usage = D3D11_USAGE_STAGING;
        backBufferDesc.BindFlags = 0;
        backBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    }

    ID3D11Texture2D *tempTexture2D = nullptr;
    if(SUCCEEDED(this->device->CreateTexture2D(&backBufferDesc, nullptr, &tempTexture2D)) && tempTexture2D) {
        D3D11_TEXTURE2D_DESC tempTexture2DDesc;
        tempTexture2D->GetDesc(&tempTexture2DDesc);
        this->deviceContext->CopyResource(tempTexture2D, backBuffer);

        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        if(SUCCEEDED(this->deviceContext->Map(tempTexture2D, 0, D3D11_MAP_READ, 0, &mappedResource))) {
            success = true;
            result.reserve(tempTexture2DDesc.Width * tempTexture2DDesc.Height * 3);  // RGB
            const UINT numPixelBytes = 4;                                            // RGBA
            const UINT numRowBytes = mappedResource.RowPitch / sizeof(u8);
            for(UINT y = 0; y < tempTexture2DDesc.Height; y++) {
                for(UINT x = 0; x < tempTexture2DDesc.Width; x++) {
                    u8 r = (u8)(((u8 *)mappedResource.pData)[y * numRowBytes + x * numPixelBytes + 0]);  // RGBA
                    u8 g = (u8)(((u8 *)mappedResource.pData)[y * numRowBytes + x * numPixelBytes + 1]);
                    u8 b = (u8)(((u8 *)mappedResource.pData)[y * numRowBytes + x * numPixelBytes + 2]);
                    // u8 a = (u8)(((u8*)mappedResource.pData)[y*numRowBytes + x*numPixelBytes + 3]);

                    result.push_back(r);
                    result.push_back(g);
                    result.push_back(b);
                }
            }
            this->deviceContext->Unmap(tempTexture2D, 0);
        }
        tempTexture2D->Release();
    }
    backBuffer->Release();

    if(!success) {
        const int numExpectedPixels = (int)(this->vResolution.x) * (int)(this->vResolution.y);
        for(int i = 0; i < numExpectedPixels; i++) {
            result.push_back(0);
            result.push_back(0);
            result.push_back(0);
        }
    }
    return result;
}

UString DirectX11Interface::getVendor() {
    DXGI_ADAPTER_DESC desc;
    if(this->dxgiAdapter && SUCCEEDED(this->dxgiAdapter->GetDesc(&desc))) {
        return UString::format("0x%x", desc.VendorId);
    }

    return "<UNKNOWN>";
}

UString DirectX11Interface::getModel() {
    DXGI_ADAPTER_DESC desc;
    if(this->dxgiAdapter && SUCCEEDED(this->dxgiAdapter->GetDesc(&desc))) {
        const std::wstring description = std::wstring(desc.Description, 128);
        return {description.c_str()};
    }

    return "<UNKNOWN>";
}

UString DirectX11Interface::getVersion() {
    DXGI_ADAPTER_DESC desc;
    if(this->dxgiAdapter && SUCCEEDED(this->dxgiAdapter->GetDesc(&desc))) {
        return UString::format("0x%x/%x/%x", desc.DeviceId, desc.SubSysId, desc.Revision);
    }

    return "<UNKNOWN>";
}

int DirectX11Interface::getVRAMTotal() {
    DXGI_ADAPTER_DESC desc;
    if(this->dxgiAdapter && SUCCEEDED(this->dxgiAdapter->GetDesc(&desc))) {
        // NOTE: this value is affected by 32-bit limits, meaning it will cap out at ~3071 MB (or ~3072 MB depending on rounding), which makes sense since we
        // can't address more video memory in a 32-bit process anyway
        return (desc.DedicatedVideoMemory / 1024);  // (from bytes to kb)
    }

    return -1;
}

int DirectX11Interface::getVRAMRemaining() {
    // TODO: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo

    return -1;
}

void DirectX11Interface::setVSync(bool vsync) { this->bVSync = vsync; }

void DirectX11Interface::onResolutionChange(vec2 newResolution) {
    this->vResolution = newResolution;
    if(!this->swapChain) return;  // ignore until swapchain is created

    if(!engine->isDrawing()) {  // HACKHACK: to allow viewport changes for rendertarget rendering OpenGL style
        // rebuild swapchain rendertarget + view

        // unset + release
        if(this->frameBuffer) {
            this->frameBuffer->Release();
            this->frameBuffer = nullptr;
        }

        if(this->frameBufferDepthStencilView) {
            this->frameBufferDepthStencilView->Release();
            this->frameBufferDepthStencilView = nullptr;
        }

        if(this->frameBufferDepthStencilTexture) {
            this->frameBufferDepthStencilTexture->Release();
            this->frameBufferDepthStencilTexture = nullptr;
        }

        UINT newWidth = static_cast<UINT>(this->vResolution.x);
        UINT newHeight = static_cast<UINT>(this->vResolution.y);

        auto oldDesc = this->queryCurrentSwapchainDesc();
        UINT oldDescWidth = oldDesc.Width;
        UINT oldDescHeight = oldDesc.Height;

        if(oldDescWidth != newHeight || oldDescHeight != newWidth) {
            oldDesc.Width = newWidth;
            oldDesc.Height = newHeight;
            this->swapChainModeDesc = oldDesc;

            this->swapChain->ResizeTarget(&this->swapChainModeDesc);
        }

        // resize
        // NOTE: when in fullscreen mode, use 0 as width/height (because they were set internally by SetFullscreenState())
        // NOTE: DXGI_FORMAT_UNKNOWN preserves the existing format
        // debugLog("actual resize fullscreen {} borderless {} {}x{}", m_bIsFullscreen, m_bIsFullscreenBorderlessWindowed, m_bIsFullscreen &&
        // !m_bIsFullscreenBorderlessWindowed ? 0 : (UINT)newResolution.x, m_bIsFullscreen && !m_bIsFullscreenBorderlessWindowed ? 0 : (UINT)newResolution.y);
        const bool isTrueFS = this->bIsFullscreen && !this->bIsFullscreenBorderlessWindowed;
        UINT resizeWidth = isTrueFS ? 0 : newWidth;
        UINT resizeHeight = isTrueFS ? 0 : newHeight;
        HRESULT hr = this->swapChain->ResizeBuffers(0, resizeWidth, resizeHeight, DXGI_FORMAT_UNKNOWN, 0);

        if(FAILED(hr))
            debugLog("FATAL ERROR: couldn't ResizeBuffers({}, {:#x})!!!", (u32)hr, (u32)MAKE_DXGI_HRESULT(hr));

        // get new (automatically generated) backbuffer
        ID3D11Texture2D *backBuffer{nullptr};
        hr = this->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer);
        if(FAILED(hr)) {
            debugLog("FATAL ERROR: couldn't GetBuffer({}, {:#x})!!!", (u32)hr, (u32)MAKE_DXGI_HRESULT(hr));
            return;
        }

        // and create new framebuffer from it
        hr = this->device->CreateRenderTargetView(backBuffer, nullptr, &this->frameBuffer);
        backBuffer->Release();  // (release temp buffer)
        if(FAILED(hr)) {
            debugLog("FATAL ERROR: couldn't CreateRenderTargetView({}, {:#x})!!!", (u32)hr, (u32)MAKE_DXGI_HRESULT(hr));
            this->frameBuffer = nullptr;
            return;
        }

        // add new depth buffer
        D3D11_TEXTURE2D_DESC depthStencilTextureDesc{
            .Width = newWidth,
            .Height = newHeight,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
            .SampleDesc =
                {
                    .Count = 1,
                    .Quality = 0,
                },
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_DEPTH_STENCIL,
            .CPUAccessFlags = 0,
            .MiscFlags = 0,
        };

        hr = this->device->CreateTexture2D(&depthStencilTextureDesc, nullptr, &this->frameBufferDepthStencilTexture);
        if(FAILED(hr)) {
            debugLog("FATAL ERROR: couldn't CreateTexture2D({}, {:x}, {:x})!!!", hr, hr, MAKE_DXGI_HRESULT(hr));
            this->frameBufferDepthStencilTexture = nullptr;
        } else {
            D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{.Format = depthStencilTextureDesc.Format,
                                                               .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
                                                               .Flags = 0,
                                                               .Texture2D = {.MipSlice = 0}};

            hr = this->device->CreateDepthStencilView(this->frameBufferDepthStencilTexture, &depthStencilViewDesc,
                                                      &this->frameBufferDepthStencilView);
            if(FAILED(hr)) {
                debugLog("FATAL ERROR: couldn't CreateDepthStencilView({}, {:x}, {:x})!!!", hr, hr,
                         MAKE_DXGI_HRESULT(hr));
                this->frameBufferDepthStencilView = nullptr;
            }
        }

        // use new framebuffer
        this->deviceContext->OMSetRenderTargets(1, &this->frameBuffer, this->frameBufferDepthStencilView);
        // debugLog("Rebuilt resolution {:g}x{:g}", m_vResolution.x, m_vResolution.y);
    } else {
        // debugLog("Engine was drawing, not rebuilding rendertarget {:g}x{:g}", newResolution.x, newResolution.y);
    }

    // rebuild viewport
    D3D11_VIEWPORT viewport{
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = this->vResolution.x,
        .Height = this->vResolution.y,
        .MinDepth = 0.0f,  // NOTE: between 0 and 1
        .MaxDepth = 1.0f,  // NOTE: between 0 and 1
    };

    this->deviceContext->RSSetViewports(1, &viewport);
    // resizeTarget(m_vResolution);
    // debugLog("Set viewport {:g}x{:g}", viewport.Width, viewport.Height);
}

bool DirectX11Interface::enableFullscreen(bool borderlessWindowedFullscreen) {
    this->bIsFullscreenBorderlessWindowed = borderlessWindowedFullscreen;

    if(!this->bIsFullscreenBorderlessWindowed) {
        HRESULT hr = this->swapChain->SetFullscreenState((BOOL) true, nullptr);
        this->bIsFullscreen = !FAILED(hr);
    } else
        this->bIsFullscreen = true;  // ("fake" fullscreen)

    return this->bIsFullscreen;
}

void DirectX11Interface::disableFullscreen() {
    if(!this->bIsFullscreen) return;

    if(!this->bIsFullscreenBorderlessWindowed) this->swapChain->SetFullscreenState((BOOL) false, nullptr);

    this->bIsFullscreen = false;
    this->bIsFullscreenBorderlessWindowed = false;
}

void DirectX11Interface::setTexturing(bool enabled) {
    this->bTexturingEnabled = enabled;
    this->shaderTexturedGeneric->setUniform4f("misc", enabled ? 1.f : 0.f, this->bColorInversion ? 1.f : 0.f, 0.f, 0.f);
}

Image *DirectX11Interface::createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) {
    return new DirectX11Image(std::move(filePath), mipmapped, keepInSystemMemory);
}

Image *DirectX11Interface::createImage(int width, int height, bool mipmapped, bool keepInSystemMemory) {
    return new DirectX11Image(width, height, mipmapped, keepInSystemMemory);
}

RenderTarget *DirectX11Interface::createRenderTarget(int x, int y, int width, int height,
                                                     Graphics::MULTISAMPLE_TYPE multiSampleType) {
    return new DirectX11RenderTarget(x, y, width, height, multiSampleType);
}

Shader *DirectX11Interface::createShaderFromFile(std::string vertexShaderFilePath, std::string fragmentShaderFilePath) {
    return new DirectX11Shader(vertexShaderFilePath, fragmentShaderFilePath, false);
}

Shader *DirectX11Interface::createShaderFromSource(std::string vertexShader, std::string fragmentShader) {
    return new DirectX11Shader(vertexShader, fragmentShader, true);
}

VertexArrayObject *DirectX11Interface::createVertexArrayObject(Graphics::PRIMITIVE primitive,
                                                               Graphics::USAGE_TYPE usage, bool keepInSystemMemory) {
    return new DirectX11VertexArrayObject(primitive, usage, keepInSystemMemory);
}

void DirectX11Interface::onTransformUpdate(Matrix4 & /*projectionMatrix*/, Matrix4 & /*worldMatrix*/) {
    // NOTE: convert from OpenGL coordinate space
    static Matrix4 zflip = Matrix4().scale(1, 1, -1);

    Matrix4 mvp = this->MP * zflip;
    this->shaderTexturedGeneric->setUniformMatrix4fv("mvp", mvp);
}

int DirectX11Interface::primitiveToDirectX(Graphics::PRIMITIVE primitive) {
    switch(primitive) {
        case Graphics::PRIMITIVE::PRIMITIVE_LINES:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case Graphics::PRIMITIVE::PRIMITIVE_LINE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN:  // NOTE: not available! -------------------
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case Graphics::PRIMITIVE::PRIMITIVE_QUADS:  // NOTE: not available! -------------------
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    }

    return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

int DirectX11Interface::compareFuncToDirectX(Graphics::COMPARE_FUNC /*compareFunc*/) {
    // TODO: implement

    return 0;
}

void DirectX11Interface::initSmoothClipShader() {
    if(this->smoothClipShader) return;

    this->smoothClipShader.reset(this->createShaderFromSource(
        std::string(reinterpret_cast<const char *>(DX11_smoothclip_vsh), DX11_smoothclip_vsh_size()),
        std::string(reinterpret_cast<const char *>(DX11_smoothclip_fsh), DX11_smoothclip_fsh_size())));

    if(this->smoothClipShader) {
        this->smoothClipShader->loadAsync();
        this->smoothClipShader->load();
    }
}

DXGI_MODE_DESC DirectX11Interface::queryCurrentSwapchainDesc() const {
    DXGI_SWAP_CHAIN_DESC swapDesc;
    auto hr = this->swapChain->GetDesc(&swapDesc);

    if(FAILED(hr)) {
        debugLog("WARNING: couldn't get current swapchain description.");
        return this->swapChainModeDesc;
    }

    return swapDesc.BufferDesc;
}

// frame latency
void DirectX11Interface::onSyncBehaviorChanged(const float newValue) {
    if(!this->dxgiDevice1) return;
    const bool disabled = !static_cast<int>(newValue);
    if(disabled != this->bFrameLatencyDisabled) {
        this->bFrameLatencyDisabled = disabled;
        if(!disabled) {
            this->dxgiDevice1->SetMaximumFrameLatency(this->iMaxFrameLatency);
        } else {
            this->dxgiDevice1->SetMaximumFrameLatency(0);
        }
    }
}

void DirectX11Interface::onFramecountNumChanged(const float newValue) {
    if(!this->dxgiDevice1) return;
    auto newLatency = std::clamp<UINT>(static_cast<UINT>(newValue), 1U, 3U);
    if(newLatency != this->iMaxFrameLatency) {
        this->iMaxFrameLatency = newLatency;
        if(!this->bFrameLatencyDisabled) {
            this->dxgiDevice1->SetMaximumFrameLatency(newLatency);
        }
    }
}

#endif
