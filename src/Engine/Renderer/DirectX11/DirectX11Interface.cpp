//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		raw DirectX 11 graphics interface
//
// $NoKeywords: $dx11i
//===============================================================================//

#include "DirectX11Interface.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

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

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#if 1              // defined(_WINVER) && _WINVER < 0x0A00
#define NO_FLIP 1  // FIXME: for some reason, perf is lower with FLIP_DISCARD than DISCARD
#endif

// #define MCENGINE_D3D11_CREATE_DEVICE_DEBUG

DirectX11Interface::DirectX11Interface(HWND hwnd, bool minimalistContext)
    : Graphics(), m_swapChainModeDesc{}, m_rasterizerDesc{}, m_depthStencilDesc{}, m_blendDesc{}, m_vertexBufferDesc{} {
    m_bReady = false;

    // device context
    m_hwnd = hwnd;
    m_bMinimalistContext = minimalistContext;

    // d3d
    m_device = nullptr;
    m_deviceContext = nullptr;
    m_swapChain = nullptr;
    m_frameBuffer = nullptr;
    m_frameBufferDepthStencilTexture = nullptr;
    m_frameBufferDepthStencilView = nullptr;

    // renderer
    m_bIsFullscreen = false;
    m_bIsFullscreenBorderlessWindowed = false;
    m_vResolution = engine->getScreenSize();  // initial viewport size = window size
    m_rasterizerState = nullptr;
    m_depthStencilState = nullptr;
    m_blendState = nullptr;
    m_shaderTexturedGeneric = nullptr;
    m_vertexBuffer = nullptr;
    m_iVertexBufferMaxNumVertices = 16384;
    m_iVertexBufferNumVertexOffsetCounter = 0;

    // persistent vars
    m_color = 0xffffffff;
    m_bVSync = false;
    m_activeShader = nullptr;

    // stats
    m_iStatsNumDrawCalls = 0;
}

void DirectX11Interface::init() {
    if(!DirectX11Shader::loadLibs()) return;

    // flags
    UINT createDeviceFlags = 0;
    createDeviceFlags = D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef MCENGINE_D3D11_CREATE_DEVICE_DEBUG

    createDeviceFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;

#endif

    // feature levels
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const UINT numFeatureLevels = ARRAY_SIZE(featureLevels);

    // backbuffer descriptor
    ZeroMemory(&m_swapChainModeDesc, sizeof(DXGI_MODE_DESC));
    {
        m_swapChainModeDesc.Width = (UINT)m_vResolution.x;
        m_swapChainModeDesc.Height = (UINT)m_vResolution.y;
        m_swapChainModeDesc.RefreshRate.Numerator = 0;
        m_swapChainModeDesc.RefreshRate.Denominator = 1;
        m_swapChainModeDesc.Format = DXGI_FORMAT::
            DXGI_FORMAT_R8G8B8A8_UNORM;  // NOTE: DXGI_FORMAT_R8G8B8A8_UNORM has the broadest compatibility range
        m_swapChainModeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER::DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        m_swapChainModeDesc.Scaling = DXGI_MODE_SCALING::DXGI_MODE_SCALING_UNSPECIFIED;
    }

    // swapchain descriptor
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    {
        swapChainDesc.BufferDesc = m_swapChainModeDesc;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = m_hwnd;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect =
#ifdef NO_FLIP
            DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_DISCARD;
#else
            DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif
        // flip discard requires at least 2 buffers
        swapChainDesc.BufferCount =
#ifdef NO_FLIP
            1;
#else
            2;
#endif
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG::DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    }

    // create device + context + swapchain
    HRESULT hr;
    D3D_FEATURE_LEVEL featureLevelOut;
    if(m_bMinimalistContext)
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                               featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &m_device, &featureLevelOut,
                               &m_deviceContext);
    else
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                           createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION,
                                           &swapChainDesc, &m_swapChain, &m_device, &featureLevelOut, &m_deviceContext);

    if(FAILED(hr)) {
        UString errorTitle = "DirectX Error";
        UString errorMessage =
            UString::format("Couldn't D3D11CreateDevice[AndSwapChain](%ld, %x, %x)!", hr, hr, MAKE_DXGI_HRESULT(hr));

        if(!m_bMinimalistContext) {
            errorMessage.append("\nThe engine will quit now.");
            engine->showMessageErrorFatal("DirectX Error", errorMessage);
            engine->shutdown();
        } else
            engine->showMessageWarning("DirectX Error", errorMessage);

        return;
    }

    if(m_bMinimalistContext) {
        m_bReady = true;
        return;
    }

    // disable hardcoded DirectX ALT + ENTER fullscreen toggle functionality (this is instead handled by the engine internally)
    // disable dxgi interfering with mode changes and WndProc (again, handled by the engine internally)
    {
        IDXGIFactory1 *pFactory = nullptr;
        if(SUCCEEDED(m_swapChain->GetParent(__uuidof(IDXGIFactory1), (void **)&pFactory))) {
            pFactory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
            pFactory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
            pFactory->Release();
        } else
            engine->showMessageWarning("DirectX Error",
                                       "Couldn't m_swapChain->GetParent()!\nThe window may behave weirdly.");
    }

    // default rasterizer settings
    {
        m_rasterizerDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
        m_rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
        m_rasterizerDesc.FrontCounterClockwise = TRUE;
        m_rasterizerDesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
        m_rasterizerDesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        m_rasterizerDesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        m_rasterizerDesc.DepthClipEnable = TRUE;  // (clipping, not depth buffer!)
        m_rasterizerDesc.ScissorEnable = FALSE;
        m_rasterizerDesc.MultisampleEnable = FALSE;
        m_rasterizerDesc.AntialiasedLineEnable = FALSE;
    }
    m_device->CreateRasterizerState(&m_rasterizerDesc, &m_rasterizerState);
    m_deviceContext->RSSetState(m_rasterizerState);

    // default depthStencil settings
    {
        m_depthStencilDesc.DepthEnable = FALSE;
        m_depthStencilDesc.StencilEnable = FALSE;
        m_depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ZERO;
        m_depthStencilDesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS;
        m_depthStencilDesc.StencilReadMask = 0;   // see OMSetDepthStencilState()
        m_depthStencilDesc.StencilWriteMask = 0;  // see OMSetDepthStencilState()

        // stencil front
        D3D11_DEPTH_STENCILOP_DESC depthStencilFrontFaceOpDesc;
        depthStencilFrontFaceOpDesc.StencilFailOp = D3D11_STENCIL_OP::D3D11_STENCIL_OP_ZERO;
        depthStencilFrontFaceOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP::D3D11_STENCIL_OP_ZERO;
        depthStencilFrontFaceOpDesc.StencilPassOp = D3D11_STENCIL_OP::D3D11_STENCIL_OP_ZERO;
        depthStencilFrontFaceOpDesc.StencilFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_ALWAYS;
        m_depthStencilDesc.FrontFace = depthStencilFrontFaceOpDesc;

        // stencil back
        D3D11_DEPTH_STENCILOP_DESC depthStencilBackFaceOpDesc;
        depthStencilBackFaceOpDesc.StencilFailOp = D3D11_STENCIL_OP::D3D11_STENCIL_OP_ZERO;
        depthStencilBackFaceOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP::D3D11_STENCIL_OP_ZERO;
        depthStencilBackFaceOpDesc.StencilPassOp = D3D11_STENCIL_OP::D3D11_STENCIL_OP_ZERO;
        depthStencilBackFaceOpDesc.StencilFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_ALWAYS;
        m_depthStencilDesc.BackFace = depthStencilBackFaceOpDesc;
    }
    m_device->CreateDepthStencilState(&m_depthStencilDesc, &m_depthStencilState);
    m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);  // for 0 see StencilReadMask, StencilWriteMask

    // default blending settings
    {
        ZeroMemory(&m_blendDesc, sizeof(D3D11_BLEND_DESC));
        m_blendDesc.AlphaToCoverageEnable = FALSE;
        m_blendDesc.IndependentBlendEnable = FALSE;

        m_blendDesc.RenderTarget[0].BlendEnable = true;
        m_blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE::D3D11_COLOR_WRITE_ENABLE_ALL;

        m_blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
        m_blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
        m_blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;

        m_blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
        m_blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
        m_blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
    }
    m_device->CreateBlendState(&m_blendDesc, &m_blendState);
    m_deviceContext->OMSetBlendState(m_blendState, nullptr, D3D11_DEFAULT_SAMPLE_MASK);

    // create default shader
    const auto vertexShader = std::string{reinterpret_cast<const char *>(default_vsh), default_vsh_size()};
    const auto pixelShader = std::string{reinterpret_cast<const char *>(default_fsh), default_fsh_size()};

    m_shaderTexturedGeneric = static_cast<DirectX11Shader *>(createShaderFromSource(vertexShader, pixelShader));
    m_shaderTexturedGeneric->load();

    if(!m_shaderTexturedGeneric->isReady()) {
        engine->showMessageErrorFatal("DirectX Error", "Failed to create default shader!\nThe engine will quit now.");
        engine->shutdown();
        return;
    }

    // default vertexbuffer
    {
        m_vertexBufferDesc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
        m_vertexBufferDesc.ByteWidth = sizeof(SimpleVertex) * m_iVertexBufferMaxNumVertices;
        m_vertexBufferDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;
        m_vertexBufferDesc.CPUAccessFlags = (m_vertexBufferDesc.Usage == D3D11_USAGE::D3D11_USAGE_DYNAMIC
                                                 ? D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE
                                                 : 0);
        m_vertexBufferDesc.MiscFlags = 0;
        m_vertexBufferDesc.StructureByteStride = 0;
    }
    if(FAILED(m_device->CreateBuffer(&m_vertexBufferDesc, nullptr, &m_vertexBuffer))) {
        engine->showMessageErrorFatal("DirectX Error",
                                      "Failed to create default vertex buffer!\nThe engine will quit now.");
        engine->shutdown();
        return;
    }

    onResolutionChange(m_vResolution);  // NOTE: force build swapchain rendertarget view

    m_bReady = true;
}

DirectX11Interface::~DirectX11Interface() {
    if(!m_bMinimalistContext) {
        if(m_swapChain != nullptr) m_swapChain->SetFullscreenState(FALSE, nullptr);

        SAFE_DELETE(m_shaderTexturedGeneric);

        if(m_vertexBuffer != nullptr) m_vertexBuffer->Release();

        if(m_rasterizerState != nullptr) m_rasterizerState->Release();

        if(m_swapChain != nullptr) m_swapChain->Release();

        if(m_frameBuffer != nullptr) m_frameBuffer->Release();

        if(m_frameBufferDepthStencilView != nullptr) m_frameBufferDepthStencilView->Release();

        if(m_frameBufferDepthStencilTexture != nullptr) m_frameBufferDepthStencilTexture->Release();
    }

    if(m_device != nullptr) m_device->Release();

    if(m_deviceContext != nullptr) m_deviceContext->Release();

    DirectX11Shader::cleanupLibs();
}

void DirectX11Interface::beginScene() {
#ifndef NO_FLIP
    // ensure render targets are bound (needed because onResolutionChange might skip setup during init)
    if(m_frameBuffer != nullptr) m_deviceContext->OMSetRenderTargets(1, &m_frameBuffer, m_frameBufferDepthStencilView);
#endif

    Matrix4 defaultProjectionMatrix =
        Camera::buildMatrixOrtho2DDXLH(0, m_vResolution.x, m_vResolution.y, 0, -1.0f, 1.0f);

    // push main transforms
    pushTransform();
    setProjectionMatrix(defaultProjectionMatrix);
    translate(cv::r_globaloffset_x.getFloat(), cv::r_globaloffset_y.getFloat());

    // and apply them
    updateTransform();

    // clear
    float clearColor[4]{0.0f, 0.0f, 0.0f, 0.0f};
    if(m_frameBuffer != nullptr) m_deviceContext->ClearRenderTargetView(m_frameBuffer, clearColor);
    if(m_frameBufferDepthStencilView != nullptr)
        m_deviceContext->ClearDepthStencilView(
            m_frameBufferDepthStencilView, D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH | D3D11_CLEAR_FLAG::D3D11_CLEAR_STENCIL,
            1.0f,
            0);  // yes, the 1.0f is correct

    // enable default shader
    m_shaderTexturedGeneric->enable();

    // prev frame render stats
    const int numDrawCallsPrevFrame = m_iStatsNumDrawCalls;
    m_iStatsNumDrawCalls = 0;
    if(vprof != nullptr && vprof->isEnabled()) {
        int numActiveShaders = 1;
        for(const Resource *resource : resourceManager->getResources()) {
            const DirectX11Shader *dx11Shader = static_cast<const DirectX11Shader *>(resource->asShader());
            if(dx11Shader != nullptr) {
                if(dx11Shader->getStatsNumConstantBufferUploadsPerFrameEngineFrameCount() ==
                   (engine->getFrameCount() - 1))
                    numActiveShaders++;
            }
        }

        int shaderCounter = 0;
        vprof->addInfoBladeEngineTextLine(UString::format("Draw Calls: %i", numDrawCallsPrevFrame));
        vprof->addInfoBladeEngineTextLine(UString::format("Active Shaders: %i", numActiveShaders));
        vprof->addInfoBladeEngineTextLine(
            UString::format("shader[%i]: shaderTexturedGeneric: %ic", shaderCounter++,
                            (int)m_shaderTexturedGeneric->getStatsNumConstantBufferUploadsPerFrame()));
        for(const Resource *resource : resourceManager->getResources()) {
            const DirectX11Shader *dx11Shader = static_cast<const DirectX11Shader *>(resource->asShader());
            if(dx11Shader != nullptr) {
                if(dx11Shader->getStatsNumConstantBufferUploadsPerFrameEngineFrameCount() ==
                   (engine->getFrameCount() - 1))
                    vprof->addInfoBladeEngineTextLine(
                        UString::format("shader[%i]: %s: %ic", shaderCounter++, resource->getName().c_str(),
                                        (int)dx11Shader->getStatsNumConstantBufferUploadsPerFrame()));
            }
        }
    }
}

void DirectX11Interface::endScene() {
    popTransform();

    checkStackLeaks();

    if(m_clipRectStack.size() > 0) {
        engine->showMessageErrorFatal("ClipRect Stack Leak", "Make sure all push*() have a pop*()!");
        engine->shutdown();
    }

    m_swapChain->Present(m_bVSync ? 1 : 0, 0);

    // aka checkErrors()
#ifdef MCENGINE_D3D11_CREATE_DEVICE_DEBUG
    {
        ID3D11Debug *d3dDebug = nullptr;
        auto hr = m_device->QueryInterface(__uuidof(ID3D11Debug), (void **)&d3dDebug);
        if(SUCCEEDED(hr)) {
            ID3D11InfoQueue *debugInfoQueue = nullptr;
            hr = d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&debugInfoQueue);
            if(SUCCEEDED(hr)) {
                UINT64 message_count = debugInfoQueue->GetNumStoredMessages();

                for(UINT64 i = 0; i < message_count; i++) {
                    SIZE_T message_size = 0;
                    debugInfoQueue->GetMessage(i, nullptr, &message_size);

                    D3D11_MESSAGE *message = (D3D11_MESSAGE *)malloc(message_size + 1);
                    memset((void *)message, '\0', message_size + 1);
                    hr = debugInfoQueue->GetMessage(i, message, &message_size);
                    if(SUCCEEDED(hr))
                        debugLog("DirectX11Debug: {:s}", message->pDescription);
                    else
                        debugLog("DirectX Error: Couldn't debugInfoQueue->GetMessage() {:#x}",
                                 static_cast<uint32_t>(hr));

                    free(message);
                }

                debugInfoQueue->ClearStoredMessages();
            } else
                debugLog("DirectX Error: Couldn't m_device->QueryInterface( ID3D11InfoQueue ) {:#x}",
                         static_cast<uint32_t>(hr));
        } else
            debugLog("DirectX Error: Couldn't m_device->QueryInterface( ID3D11Debug ) {:#x}",
                     static_cast<uint32_t>(hr));
    }
#endif
}

void DirectX11Interface::clearDepthBuffer() {
    if(m_frameBufferDepthStencilView != nullptr)
        m_deviceContext->ClearDepthStencilView(m_frameBufferDepthStencilView, D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH, 1.0f,
                                               0);  // yes, the 1.0f is correct
}

void DirectX11Interface::setColor(Color color) {
    if(m_color == color) return;

    m_color = color;
    m_shaderTexturedGeneric->setUniform4f("col", m_color.Af(), m_color.Rf(), m_color.Gf(), m_color.Bf());
}

void DirectX11Interface::setAlpha(float alpha) {
    m_color &= 0x00ffffff;
    m_color |= ((int)(255.0f * alpha)) << 24;

    setColor(m_color);
}

void DirectX11Interface::drawPixel(int x, int y) {
    updateTransform();

    this->setTexturing(false);  // disable texturing

    // build directx vertices
    m_vertices.clear();
    {
        SimpleVertex v;
        {
            v.pos.x = x;
            v.pos.y = y;
            v.pos.z = 0.0f;

            v.col = vec4(m_color.Rf(), m_color.Gf(), m_color.Bf(), m_color.Af());

            v.tex.x = 0.0f;
            v.tex.y = 0.0f;
        }
        m_vertices.push_back(v);
    }

    // upload everything to gpu
    size_t numVertexOffset = 0;
    bool uploadedSuccessfully = true;
    {
        if(m_vertexBufferDesc.Usage == D3D11_USAGE::D3D11_USAGE_DEFAULT) {
            D3D11_BOX box;
            {
                box.left = sizeof(DirectX11Interface::SimpleVertex) * 0;
                box.right = box.left + (sizeof(DirectX11Interface::SimpleVertex) * m_vertices.size());
                box.top = 0;
                box.bottom = 1;
                box.front = 0;
                box.back = 1;
            }
            m_deviceContext->UpdateSubresource(m_vertexBuffer, 0, &box, &m_vertices[0], 0, 0);
        } else {
            const bool needsDiscardEntireBuffer =
                (m_iVertexBufferNumVertexOffsetCounter + m_vertices.size() > m_iVertexBufferMaxNumVertices);
            const size_t writeOffsetNumVertices =
                (needsDiscardEntireBuffer ? 0 : m_iVertexBufferNumVertexOffsetCounter);
            numVertexOffset = writeOffsetNumVertices;
            {
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
                if(SUCCEEDED(m_deviceContext->Map(m_vertexBuffer, 0,
                                                  (needsDiscardEntireBuffer ? D3D11_MAP::D3D11_MAP_WRITE_DISCARD
                                                                            : D3D11_MAP::D3D11_MAP_WRITE_NO_OVERWRITE),
                                                  0, &mappedResource))) {
                    memcpy((void *)(((SimpleVertex *)mappedResource.pData) + writeOffsetNumVertices), &m_vertices[0],
                           sizeof(DirectX11Interface::SimpleVertex) * m_vertices.size());
                    m_deviceContext->Unmap(m_vertexBuffer, 0);
                } else
                    uploadedSuccessfully = false;
            }
            m_iVertexBufferNumVertexOffsetCounter = writeOffsetNumVertices + m_vertices.size();
        }

        // shader update
        if(uploadedSuccessfully) {
            if(m_activeShader != nullptr) m_activeShader->onJustBeforeDraw();
        }
    }

    // draw it
    if(uploadedSuccessfully) {
        const UINT stride = sizeof(SimpleVertex);
        const UINT offset = 0;

        m_deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        m_deviceContext->Draw(m_vertices.size(), numVertexOffset);
        m_iStatsNumDrawCalls++;
    }
}

void DirectX11Interface::drawPixels([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int width,
                                    [[maybe_unused]] int height, [[maybe_unused]] Graphics::DRAWPIXELS_TYPE type,
                                    [[maybe_unused]] const void *pixels) {
    // TODO: implement
}

void DirectX11Interface::drawLinef(float x1, float y1, float x2, float y2) {
    updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_LINES);
    {
        vao.empty();

        vao.addVertex(x1, y1);
        vao.addVertex(x2, y2);
    }
    drawVAO(&vao);
}

void DirectX11Interface::drawRectf(float x, float y, float width, float height, bool withColor, Color top, Color right,
                                   Color bottom, Color left) {
    updateTransform();

    if(withColor) {
        setColor(top);
        drawLinef(x, y, x + width, y);
        setColor(left);
        drawLinef(x, y, x, y + height);
        setColor(bottom);
        drawLinef(x, y + height, x + width, y + height);
        setColor(right);
        drawLinef(x + width, y, x + width, y + height);
    } else {
        drawLinef(x, y, x + width, y);
        drawLinef(x, y, x, y + height);
        drawLinef(x, y + height, x + width, y + height);
        drawLinef(x + width, y, x + width, y + height);
    }
}

void DirectX11Interface::fillRectf(float x, float y, float width, float height) {
    updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.empty();

        vao.addVertex(x, y);
        vao.addVertex(x, y + height);
        vao.addVertex(x + width, y + height);
        vao.addVertex(x + width, y);
    }
    drawVAO(&vao);
}

void DirectX11Interface::fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                                      Color bottomLeftColor, Color bottomRightColor) {
    updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.empty();

        vao.addVertex(x, y);
        vao.addColor(topLeftColor);
        vao.addVertex(x + width, y);
        vao.addColor(topRightColor);
        vao.addVertex(x + width, y + height);
        vao.addColor(bottomRightColor);
        vao.addVertex(x, y + height);
        vao.addColor(bottomLeftColor);
    }
    drawVAO(&vao);
}

void DirectX11Interface::drawQuad(int x, int y, int width, int height) {
    updateTransform();

    this->setTexturing(true);  // enable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.empty();

        vao.addVertex(x, y);
        vao.addTexcoord(0, 0);
        vao.addVertex(x, y + height);
        vao.addTexcoord(0, 1);
        vao.addVertex(x + width, y + height);
        vao.addTexcoord(1, 1);
        vao.addVertex(x + width, y);
        vao.addTexcoord(1, 0);
    }
    drawVAO(&vao);
}

void DirectX11Interface::drawQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft, Color topLeftColor,
                                  Color topRightColor, Color bottomRightColor, Color bottomLeftColor) {
    updateTransform();

    this->setTexturing(false);  // disable texturing

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.empty();

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
    drawVAO(&vao);
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
        pushClipRect(clipRect);
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
        m_deviceContext->RSGetViewports(&numViewports, &viewport);

        float clipMinX = (clipRect.getX() + viewport.TopLeftX) - .5f;  // i don't know... weird rounding
        float clipMinY = (clipRect.getY() + viewport.TopLeftY) - .5f;
        float clipMaxX = (clipMinX + clipRect.getWidth()) + .5f;
        float clipMaxY = (clipMinY + clipRect.getHeight()) + .5f;

        this->smoothClipShader->enable();
        this->smoothClipShader->setUniform2f("rect_min", clipMinX, clipMinY);
        this->smoothClipShader->setUniform2f("rect_max", clipMaxX, clipMaxY);
        this->smoothClipShader->setUniform1f("edge_softness", edgeSoftness);

        // set mvp for the shader
        Matrix4 mvp = g->getMVP();
        this->smoothClipShader->setUniformMatrix4fv("mvp", mvp);
    }

    static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
    {
        vao.empty();

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
        drawVAO(&vao);
    }
    image->unbind();

    if(smoothedEdges) {
        this->smoothClipShader->disable();
    } else if(fallbackClip) {
        popClipRect();
    }

    if(cv::r_debug_drawimage.getBool()) {
        setColor(0xbbff00ff);
        drawRect(x, y, width, height);
    }
}

void DirectX11Interface::drawString(McFont *font, const UString &text) {
    if(font == nullptr || text.length() < 1 || !font->isReady()) return;

    updateTransform();

    this->setTexturing(true);  // enable texturing

    font->drawString(text);
}

void DirectX11Interface::drawVAO(VertexArrayObject *vao) {
    if(vao == nullptr) return;

    updateTransform();

    // if baked, then we can directly draw the buffer
    if(vao->isReady()) {
        // shader update
        {
            if(m_activeShader != nullptr) m_activeShader->onJustBeforeDraw();
        }

        vao->draw();
        return;
    }

    const std::vector<vec3> &vertices = vao->getVertices();
    /// const std::vector<vec3> &normals = vao->getNormals();
    const std::vector<std::vector<vec2>> &texcoords = vao->getTexcoords();
    const std::vector<Color> &vcolors = vao->getColors();

    if(vertices.size() < 2) return;

    // TODO: optimize this piece of shit

    // no support for quads, because fuck you
    // no support for triangle fans, because fuck youuu
    // rewrite all quads into triangles
    // rewrite all triangle fans into triangles
    static std::vector<vec3> finalVertices;
    finalVertices = vertices;
    static std::vector<std::vector<vec2>> finalTexcoords;
    finalTexcoords = texcoords;
    static std::vector<vec4> colors;
    colors.clear();
    static std::vector<vec4> finalColors;
    finalColors.clear();

    for(size_t i = 0; i < vcolors.size(); i++) {
        const vec4 color = vec4(vcolors[i].Rf(), vcolors[i].Gf(), vcolors[i].Bf(), vcolors[i].Af());
        colors.push_back(color);
        finalColors.push_back(color);
    }
    const size_t maxColorIndex = (colors.size() > 0 ? colors.size() - 1 : 0);

    Graphics::PRIMITIVE primitive = vao->getPrimitive();
    if(primitive == Graphics::PRIMITIVE::PRIMITIVE_QUADS) {
        finalVertices.clear();
        for(size_t t = 0; t < finalTexcoords.size(); t++) {
            finalTexcoords[t].clear();
        }
        finalColors.clear();
        primitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES;

        if(vertices.size() > 3) {
            for(size_t i = 0; i < vertices.size(); i += 4) {
                finalVertices.push_back(vertices[i + 0]);
                finalVertices.push_back(vertices[i + 1]);
                finalVertices.push_back(vertices[i + 2]);

                for(size_t t = 0; t < texcoords.size(); t++) {
                    finalTexcoords[t].push_back(texcoords[t][i + 0]);
                    finalTexcoords[t].push_back(texcoords[t][i + 1]);
                    finalTexcoords[t].push_back(texcoords[t][i + 2]);
                }

                if(colors.size() > 0) {
                    finalColors.push_back(colors[std::clamp<size_t>(i + 0, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i + 1, 0, maxColorIndex)]);
                    finalColors.push_back(colors[std::clamp<size_t>(i + 2, 0, maxColorIndex)]);
                }

                finalVertices.push_back(vertices[i + 0]);
                finalVertices.push_back(vertices[i + 2]);
                finalVertices.push_back(vertices[i + 3]);

                for(size_t t = 0; t < texcoords.size(); t++) {
                    finalTexcoords[t].push_back(texcoords[t][i + 0]);
                    finalTexcoords[t].push_back(texcoords[t][i + 2]);
                    finalTexcoords[t].push_back(texcoords[t][i + 3]);
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
        for(size_t t = 0; t < finalTexcoords.size(); t++) {
            finalTexcoords[t].clear();
        }
        finalColors.clear();
        primitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES;

        if(vertices.size() > 2) {
            for(size_t i = 2; i < vertices.size(); i++) {
                finalVertices.push_back(vertices[0]);

                finalVertices.push_back(vertices[i]);
                finalVertices.push_back(vertices[i - 1]);

                for(size_t t = 0; t < texcoords.size(); t++) {
                    finalTexcoords[t].push_back(texcoords[t][0]);
                    finalTexcoords[t].push_back(texcoords[t][i]);
                    finalTexcoords[t].push_back(texcoords[t][i - 1]);
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
    const bool hasTexcoords0 = (finalTexcoords.size() > 0 && finalTexcoords[0].size() > 0);
    m_vertices.resize(finalVertices.size());
    {
        const bool hasColors = (finalColors.size() > 0);

        const size_t maxColorIndex = (hasColors ? finalColors.size() - 1 : 0);
        const size_t maxTexcoords0Index = (hasTexcoords0 ? finalTexcoords[0].size() - 1 : 0);

        const vec4 color = vec4(m_color.Rf(), m_color.Gf(), m_color.Bf(), m_color.Af());

        for(size_t i = 0; i < finalVertices.size(); i++) {
            m_vertices[i].pos = finalVertices[i];

            if(hasColors)
                m_vertices[i].col = finalColors[std::clamp<size_t>(i, 0, maxColorIndex)];
            else
                m_vertices[i].col = color;

            // TODO: multitexturing
            if(hasTexcoords0) m_vertices[i].tex = finalTexcoords[0][std::clamp<size_t>(i, 0, maxTexcoords0Index)];
        }
    }

    // upload everything to gpu
    size_t numVertexOffset = 0;
    bool uploadedSuccessfully = true;
    {
        if(m_vertexBufferDesc.Usage == D3D11_USAGE::D3D11_USAGE_DEFAULT) {
            D3D11_BOX box;
            {
                box.left = sizeof(DirectX11Interface::SimpleVertex) * 0;
                box.right = box.left + (sizeof(DirectX11Interface::SimpleVertex) * m_vertices.size());
                box.top = 0;
                box.bottom = 1;
                box.front = 0;
                box.back = 1;
            }
            m_deviceContext->UpdateSubresource(m_vertexBuffer, 0, &box, &m_vertices[0], 0, 0);
        } else {
            const bool needsDiscardEntireBuffer =
                (m_iVertexBufferNumVertexOffsetCounter + m_vertices.size() > m_iVertexBufferMaxNumVertices);
            const size_t writeOffsetNumVertices =
                (needsDiscardEntireBuffer ? 0 : m_iVertexBufferNumVertexOffsetCounter);
            numVertexOffset = writeOffsetNumVertices;
            {
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
                if(SUCCEEDED(m_deviceContext->Map(m_vertexBuffer, 0,
                                                  (needsDiscardEntireBuffer ? D3D11_MAP::D3D11_MAP_WRITE_DISCARD
                                                                            : D3D11_MAP::D3D11_MAP_WRITE_NO_OVERWRITE),
                                                  0, &mappedResource))) {
                    memcpy((void *)(((SimpleVertex *)mappedResource.pData) + writeOffsetNumVertices), &m_vertices[0],
                           sizeof(DirectX11Interface::SimpleVertex) * m_vertices.size());
                    m_deviceContext->Unmap(m_vertexBuffer, 0);
                } else
                    uploadedSuccessfully = false;
            }
            m_iVertexBufferNumVertexOffsetCounter = writeOffsetNumVertices + m_vertices.size();
        }

        // shader update
        if(uploadedSuccessfully) {
            this->setTexturing(hasTexcoords0);

            if(m_activeShader != nullptr) m_activeShader->onJustBeforeDraw();
        }
    }

    // draw it
    if(uploadedSuccessfully) {
        const UINT stride = sizeof(SimpleVertex);
        const UINT offset = 0;

        m_deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
        m_deviceContext->IASetPrimitiveTopology((D3D_PRIMITIVE_TOPOLOGY)primitiveToDirectX(primitive));
        m_deviceContext->Draw(m_vertices.size(), numVertexOffset);
        m_iStatsNumDrawCalls++;
    }
}

void DirectX11Interface::setClipRect(McRect clipRect) {
    if(cv::r_debug_disable_cliprect.getBool()) return;
    // if (m_bIs3DScene) return; // HACKHACK: TODO:

    setClipping(true);

    D3D11_RECT rect;
    {
        rect.left = clipRect.getMinX();
        rect.top = clipRect.getMinY() - 1;
        rect.right = clipRect.getMaxX();
        rect.bottom = clipRect.getMaxY() - 1;
    }
    m_deviceContext->RSSetScissorRects(1, &rect);
}

void DirectX11Interface::pushClipRect(McRect clipRect) {
    if(m_clipRectStack.size() > 0)
        m_clipRectStack.push(m_clipRectStack.top().intersect(clipRect));
    else
        m_clipRectStack.push(clipRect);

    setClipRect(m_clipRectStack.top());
}

void DirectX11Interface::popClipRect() {
    m_clipRectStack.pop();

    if(m_clipRectStack.size() > 0)
        setClipRect(m_clipRectStack.top());
    else
        setClipping(false);
}

void DirectX11Interface::setClipping(bool enabled) {
    if(enabled) {
        if(m_clipRectStack.size() < 1) enabled = false;
    }

    m_rasterizerState->Release();
    m_rasterizerDesc.ScissorEnable = (enabled ? TRUE : FALSE);
    m_device->CreateRasterizerState(&m_rasterizerDesc, &m_rasterizerState);
    m_deviceContext->RSSetState(m_rasterizerState);
}

void DirectX11Interface::setAlphaTesting([[maybe_unused]] bool enabled) {
    // TODO: implement in default shader
}

void DirectX11Interface::setAlphaTestFunc([[maybe_unused]] COMPARE_FUNC alphaFunc, [[maybe_unused]] float ref) {
    // TODO: implement in default shader
}

void DirectX11Interface::setBlending(bool enabled) {
    m_blendState->Release();
    m_blendDesc.RenderTarget[0].BlendEnable = (enabled ? TRUE : FALSE);
    m_device->CreateBlendState(&m_blendDesc, &m_blendState);
    m_deviceContext->OMSetBlendState(m_blendState, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
}

void DirectX11Interface::setBlendMode(BLEND_MODE blendMode) {
    m_blendState->Release();
    {
        switch(blendMode) {
            case BLEND_MODE::BLEND_MODE_ALPHA: {
                m_blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;

                m_blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
            } break;

            case BLEND_MODE::BLEND_MODE_ADDITIVE: {
                m_blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_ONE;
                m_blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;

                m_blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND::D3D11_BLEND_ONE;
                m_blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
            } break;

            case BLEND_MODE::BLEND_MODE_PREMUL_ALPHA: {
                m_blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;

                m_blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND::D3D11_BLEND_ONE;
                m_blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
            } break;

            case BLEND_MODE::BLEND_MODE_PREMUL_COLOR: {
                m_blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_ONE;
                m_blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;

                m_blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND::D3D11_BLEND_ONE;
                m_blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
                m_blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP::D3D11_BLEND_OP_ADD;
            } break;
        }
    }
    m_device->CreateBlendState(&m_blendDesc, &m_blendState);
    m_deviceContext->OMSetBlendState(m_blendState, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
}

void DirectX11Interface::setDepthBuffer(bool enabled) {
    m_depthStencilState->Release();
    m_depthStencilDesc.DepthEnable = (enabled ? TRUE : FALSE);
    m_depthStencilDesc.DepthWriteMask = (enabled ? D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL
                                                 : D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ZERO);
    m_device->CreateDepthStencilState(&m_depthStencilDesc, &m_depthStencilState);
    m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);  // for 0 see StencilReadMask, StencilWriteMask
}

void DirectX11Interface::setCulling(bool culling) {
    m_rasterizerState->Release();
    m_rasterizerDesc.CullMode = (culling ? D3D11_CULL_MODE::D3D11_CULL_BACK : D3D11_CULL_MODE::D3D11_CULL_NONE);
    m_device->CreateRasterizerState(&m_rasterizerDesc, &m_rasterizerState);
    m_deviceContext->RSSetState(m_rasterizerState);
}

void DirectX11Interface::setColorWriting(bool r, bool g, bool b, bool a) { MC_MESSAGE("TODO") }

void DirectX11Interface::setColorInversion(bool enabled) {
    if(m_bColorInversion == enabled) return;

    m_bColorInversion = enabled;
    setTexturing(m_bTexturingEnabled);  // re-apply with new inversion state
}

void DirectX11Interface::setDepthWriting(bool enabled) { MC_MESSAGE("TODO") }

void DirectX11Interface::setAntialiasing(bool aa) {
    m_rasterizerState->Release();
    m_rasterizerDesc.MultisampleEnable = (aa ? TRUE : FALSE);
    m_device->CreateRasterizerState(&m_rasterizerDesc, &m_rasterizerState);
    m_deviceContext->RSSetState(m_rasterizerState);
}

void DirectX11Interface::setWireframe(bool enabled) {
    m_rasterizerState->Release();
    m_rasterizerDesc.FillMode = (enabled ? D3D11_FILL_MODE::D3D11_FILL_WIREFRAME : D3D11_FILL_MODE::D3D11_FILL_SOLID);
    m_device->CreateRasterizerState(&m_rasterizerDesc, &m_rasterizerState);
    m_deviceContext->RSSetState(m_rasterizerState);
}

void DirectX11Interface::setLineWidth(float width) { MC_MESSAGE("TODO"); }

void DirectX11Interface::flush() { m_deviceContext->Flush(); }

std::vector<u8> DirectX11Interface::getScreenshot([[maybe_unused]] bool withAlpha) {
    std::vector<u8> result;
    {
        bool success = false;
        {
            ID3D11Texture2D *backBuffer = nullptr;
            if(SUCCEEDED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&backBuffer)) &&
               backBuffer != nullptr) {
                D3D11_TEXTURE2D_DESC backBufferDesc;
                backBuffer->GetDesc(&backBufferDesc);
                {
                    backBufferDesc.Usage = D3D11_USAGE_STAGING;
                    backBufferDesc.BindFlags = 0;
                    backBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                }
                ID3D11Texture2D *tempTexture2D = nullptr;
                if(SUCCEEDED(m_device->CreateTexture2D(&backBufferDesc, nullptr, &tempTexture2D)) &&
                   tempTexture2D != nullptr) {
                    D3D11_TEXTURE2D_DESC tempTexture2DDesc;
                    tempTexture2D->GetDesc(&tempTexture2DDesc);
                    m_deviceContext->CopyResource(tempTexture2D, backBuffer);
                    {
                        D3D11_MAPPED_SUBRESOURCE mappedResource;
                        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
                        if(SUCCEEDED(m_deviceContext->Map(tempTexture2D, 0, D3D11_MAP_READ, 0, &mappedResource))) {
                            success = true;
                            result.reserve(tempTexture2DDesc.Width * tempTexture2DDesc.Height * 3);  // RGB
                            {
                                const UINT numPixelBytes = 4;  // RGBA
                                const UINT numRowBytes = mappedResource.RowPitch / sizeof(u8);
                                for(UINT y = 0; y < tempTexture2DDesc.Height; y++) {
                                    for(UINT x = 0; x < tempTexture2DDesc.Width; x++) {
                                        u8 r = (u8)(((u8 *)mappedResource
                                                         .pData)[y * numRowBytes + x * numPixelBytes + 0]);  // RGBA
                                        u8 g =
                                            (u8)(((u8 *)mappedResource.pData)[y * numRowBytes + x * numPixelBytes + 1]);
                                        u8 b =
                                            (u8)(((u8 *)mappedResource.pData)[y * numRowBytes + x * numPixelBytes + 2]);
                                        // u8 a = (u8)(((u8*)mappedResource.pData)[y*numRowBytes + x*numPixelBytes + 3]);

                                        result.push_back(r);
                                        result.push_back(g);
                                        result.push_back(b);
                                    }
                                }
                            }
                            m_deviceContext->Unmap(tempTexture2D, 0);
                        }
                    }
                    tempTexture2D->Release();
                }
                backBuffer->Release();
            }
        }

        if(!success) {
            const int numExpectedPixels = (int)(m_vResolution.x) * (int)(m_vResolution.y);
            for(int i = 0; i < numExpectedPixels; i++) {
                result.push_back(0);
                result.push_back(0);
                result.push_back(0);
            }
        }
    }
    return result;
}

UString DirectX11Interface::getVendor() {
    IDXGIFactory1 *pFactory = nullptr;
    if(SUCCEEDED(m_swapChain->GetParent(__uuidof(IDXGIFactory1), (void **)&pFactory)) && pFactory != nullptr) {
        IDXGIAdapter *adapter = nullptr;
        if(SUCCEEDED(pFactory->EnumAdapters(0, &adapter)) && adapter != nullptr) {
            DXGI_ADAPTER_DESC desc;
            if(SUCCEEDED(adapter->GetDesc(&desc))) {
                return UString::format("0x%x", desc.VendorId);
            }
            adapter->Release();
        }
        pFactory->Release();
    }

    return "<UNKNOWN>";
}

UString DirectX11Interface::getModel() {
    IDXGIFactory1 *pFactory = nullptr;
    if(SUCCEEDED(m_swapChain->GetParent(__uuidof(IDXGIFactory1), (void **)&pFactory)) && pFactory != nullptr) {
        IDXGIAdapter *adapter = nullptr;
        if(SUCCEEDED(pFactory->EnumAdapters(0, &adapter)) && adapter != nullptr) {
            DXGI_ADAPTER_DESC desc;
            if(SUCCEEDED(adapter->GetDesc(&desc))) {
                const std::wstring description = std::wstring(desc.Description, 128);
                return UString(description.c_str());
            }
            adapter->Release();
        }
        pFactory->Release();
    }

    return "<UNKNOWN>";
}

UString DirectX11Interface::getVersion() {
    IDXGIFactory1 *pFactory = nullptr;
    if(SUCCEEDED(m_swapChain->GetParent(__uuidof(IDXGIFactory1), (void **)&pFactory)) && pFactory != nullptr) {
        IDXGIAdapter *adapter = nullptr;
        if(SUCCEEDED(pFactory->EnumAdapters(0, &adapter)) && adapter != nullptr) {
            DXGI_ADAPTER_DESC desc;
            if(SUCCEEDED(adapter->GetDesc(&desc))) {
                return UString::format("0x%x/%x/%x", desc.DeviceId, desc.SubSysId, desc.Revision);
            }
            adapter->Release();
        }
        pFactory->Release();
    }

    return "<UNKNOWN>";
}

int DirectX11Interface::getVRAMTotal() {
    IDXGIFactory1 *pFactory = nullptr;
    if(SUCCEEDED(m_swapChain->GetParent(__uuidof(IDXGIFactory1), (void **)&pFactory)) && pFactory != nullptr) {
        IDXGIAdapter *adapter = nullptr;
        if(SUCCEEDED(pFactory->EnumAdapters(0, &adapter)) && adapter != nullptr) {
            DXGI_ADAPTER_DESC desc;
            if(SUCCEEDED(adapter->GetDesc(&desc))) {
                // NOTE: this value is affected by 32-bit limits, meaning it will cap out at ~3071 MB (or ~3072 MB depending on rounding), which makes sense since we
                // can't address more video memory in a 32-bit process anyway
                return (desc.DedicatedVideoMemory / 1024);  // (from bytes to kb)
            }
            adapter->Release();
        }
        pFactory->Release();
    }

    return -1;
}

int DirectX11Interface::getVRAMRemaining() {
    // TODO: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo

    return -1;
}

void DirectX11Interface::setVSync(bool vsync) { m_bVSync = vsync; }

void DirectX11Interface::onResolutionChange(vec2 newResolution) {
    m_vResolution = newResolution;

    if(!engine->isDrawing())  // HACKHACK: to allow viewport changes for rendertarget rendering OpenGL style
    {
        env->syncWindow();
        // rebuild swapchain rendertarget + view
        HRESULT hr;

        // unset + release
        {
            m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);

            if(m_frameBuffer != nullptr) {
                m_frameBuffer->Release();
                m_frameBuffer = nullptr;
            }

            if(m_frameBufferDepthStencilView != nullptr) {
                m_frameBufferDepthStencilView->Release();
                m_frameBufferDepthStencilView = nullptr;
            }

            if(m_frameBufferDepthStencilTexture != nullptr) {
                m_frameBufferDepthStencilTexture->Release();
                m_frameBufferDepthStencilTexture = nullptr;
            }
        }

        // resize
        // NOTE: when in fullscreen mode, use 0 as width/height (because they were set internally by SetFullscreenState())
        // NOTE: DXGI_FORMAT_UNKNOWN preserves the existing format
        // debugLog("actual resize fullscreen {} borderless {} {}x{}", m_bIsFullscreen, m_bIsFullscreenBorderlessWindowed, m_bIsFullscreen &&
        // !m_bIsFullscreenBorderlessWindowed ? 0 : (UINT)newResolution.x, m_bIsFullscreen && !m_bIsFullscreenBorderlessWindowed ? 0 : (UINT)newResolution.y);
        hr = m_swapChain->ResizeBuffers(
            0, (m_bIsFullscreen && !m_bIsFullscreenBorderlessWindowed ? 0 : (UINT)newResolution.x),
            (m_bIsFullscreen && !m_bIsFullscreenBorderlessWindowed ? 0 : (UINT)newResolution.y),
            DXGI_FORMAT::DXGI_FORMAT_UNKNOWN,
            /*DXGI_SWAP_CHAIN_FLAG::DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH*/ 0);

        if(FAILED(hr))
            debugLog("FATAL ERROR: couldn't ResizeBuffers({}, {:x}, {:x})!!!", hr, hr, MAKE_DXGI_HRESULT(hr));

        // get new (automatically generated) backbuffer
        ID3D11Texture2D *backBuffer;
        hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer);
        if(FAILED(hr)) {
            debugLog("FATAL ERROR: couldn't GetBuffer({}, {:x}, {:x})!!!", hr, hr, MAKE_DXGI_HRESULT(hr));
            return;
        }

        // read actual new width/height of backbuffer
        {
            D3D11_TEXTURE2D_DESC backBufferTextureDesc;
            backBuffer->GetDesc(&backBufferTextureDesc);

            // NOTE: force overwrite local resolution (even though it was just set at the start of onResolutionChange() here)
            m_vResolution.x = (float)backBufferTextureDesc.Width;
            m_vResolution.y = (float)backBufferTextureDesc.Height;
        }

        // and create new framebuffer from it
        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_frameBuffer);
        backBuffer->Release();  // (release temp buffer)
        if(FAILED(hr)) {
            debugLog("FATAL ERROR: couldn't CreateRenderTargetView({}, {:x}, {:x})!!!", hr, hr, MAKE_DXGI_HRESULT(hr));
            m_frameBuffer = nullptr;
            return;
        }

        // add new depth buffer
        {
            D3D11_TEXTURE2D_DESC depthStencilTextureDesc;
            {
                depthStencilTextureDesc.ArraySize = 1;
                depthStencilTextureDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_DEPTH_STENCIL;
                depthStencilTextureDesc.CPUAccessFlags = 0;
                depthStencilTextureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT;
                depthStencilTextureDesc.MipLevels = 1;
                depthStencilTextureDesc.MiscFlags = 0;
                depthStencilTextureDesc.SampleDesc.Count = 1;
                depthStencilTextureDesc.SampleDesc.Quality = 0;
                depthStencilTextureDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
                depthStencilTextureDesc.Width = (UINT)m_vResolution.x;
                depthStencilTextureDesc.Height = (UINT)m_vResolution.y;
            }

            hr = m_device->CreateTexture2D(&depthStencilTextureDesc, nullptr, &m_frameBufferDepthStencilTexture);
            if(FAILED(hr)) {
                debugLog("FATAL ERROR: couldn't CreateTexture2D({}, {:x}, {:x})!!!", hr, hr, MAKE_DXGI_HRESULT(hr));
                m_frameBufferDepthStencilTexture = nullptr;
            } else {
                D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
                {
                    depthStencilViewDesc.Format = depthStencilTextureDesc.Format;
                    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION::D3D11_DSV_DIMENSION_TEXTURE2D;
                    depthStencilViewDesc.Flags = 0;
                    depthStencilViewDesc.Texture2D.MipSlice = 0;
                }

                hr = m_device->CreateDepthStencilView(m_frameBufferDepthStencilTexture, &depthStencilViewDesc,
                                                      &m_frameBufferDepthStencilView);
                if(FAILED(hr)) {
                    debugLog("FATAL ERROR: couldn't CreateDepthStencilView({}, {:x}, {:x})!!!", hr, hr,
                             MAKE_DXGI_HRESULT(hr));
                    m_frameBufferDepthStencilView = nullptr;
                }
            }
        }

        // use new framebuffer
        m_deviceContext->OMSetRenderTargets(1, &m_frameBuffer, m_frameBufferDepthStencilView);
        // debugLog("Rebuilt resolution {:g}x{:g}", m_vResolution.x, m_vResolution.y);
    } else {
        // debugLog("Engine was drawing, not rebuilding rendertarget {:g}x{:g}", newResolution.x, newResolution.y);
    }

    // rebuild viewport
    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
    {
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = m_vResolution.x;
        viewport.Height = m_vResolution.y;
        viewport.MinDepth = 0.0f;  // NOTE: between 0 and 1
        viewport.MaxDepth = 1.0f;  // NOTE: between 0 and 1
    }
    m_deviceContext->RSSetViewports(1, &viewport);
    // resizeTarget(m_vResolution);
    // debugLog("Set viewport {:g}x{:g}", viewport.Width, viewport.Height);
    env->syncWindow();
}

void DirectX11Interface::resizeTarget(vec2 newResolution) {
    m_swapChainModeDesc.Width = (UINT)newResolution.x;
    m_swapChainModeDesc.Height = (UINT)newResolution.y;

    m_swapChain->ResizeTarget(
        &m_swapChainModeDesc);  // NOTE: this will resize the actual window and send WM_SIZE which will in turn call onResolutionChange() here
}

bool DirectX11Interface::enableFullscreen(bool borderlessWindowedFullscreen) {
    m_bIsFullscreenBorderlessWindowed = borderlessWindowedFullscreen;

    if(!m_bIsFullscreenBorderlessWindowed) {
        HRESULT hr = m_swapChain->SetFullscreenState((BOOL) true, nullptr);
        m_bIsFullscreen = !FAILED(hr);
    } else
        m_bIsFullscreen = true;  // ("fake" fullscreen)

    return m_bIsFullscreen;
}

void DirectX11Interface::disableFullscreen() {
    if(!m_bIsFullscreen) return;

    if(!m_bIsFullscreenBorderlessWindowed) m_swapChain->SetFullscreenState((BOOL) false, nullptr);

    m_bIsFullscreen = false;
    m_bIsFullscreenBorderlessWindowed = false;
}

void DirectX11Interface::setTexturing(bool enabled) {
    m_bTexturingEnabled = enabled;
    m_shaderTexturedGeneric->setUniform4f("misc", enabled ? 1.f : 0.f, m_bColorInversion ? 1.f : 0.f, 0.f, 0.f);
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

void DirectX11Interface::onTransformUpdate([[maybe_unused]] Matrix4 &projectionMatrix,
                                           [[maybe_unused]] Matrix4 &worldMatrix) {
    // NOTE: convert from OpenGL coordinate space
    static Matrix4 zflip = Matrix4().scale(1, 1, -1);

    Matrix4 mvp = this->MP * zflip;
    m_shaderTexturedGeneric->setUniformMatrix4fv("mvp", mvp);
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

int DirectX11Interface::compareFuncToDirectX([[maybe_unused]] Graphics::COMPARE_FUNC compareFunc) {
    // TODO: implement

    return 0;
}

void DirectX11Interface::initSmoothClipShader() {
    if(this->smoothClipShader != nullptr) return;

    this->smoothClipShader.reset(this->createShaderFromSource(
        std::string(reinterpret_cast<const char *>(smoothclip_vsh), smoothclip_vsh_size()),
        std::string(reinterpret_cast<const char *>(smoothclip_fsh), smoothclip_fsh_size())));

    if(this->smoothClipShader != nullptr) {
        this->smoothClipShader->loadAsync();
        this->smoothClipShader->load();
    }
}

#endif
