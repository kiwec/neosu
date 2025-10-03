//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX HLSL implementation of Shader
//
// $NoKeywords: $dxshader
//===============================================================================//

// TODO: prime full cache on load anyway
// TODO: individually remember m_bConstantBuffersUpToDate per constant buffer

#include "DirectX11Shader.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"
#include "SString.h"

#include "DirectX11Interface.h"

#include "d3dcompiler.h"

#include "shaders.h"

#include <sstream>

#define MCENGINE_D3D11_SHADER_MAX_NUM_CONSTANT_BUFFERS 9

DirectX11Shader::CACHE_ENTRY DirectX11Shader::invalidCacheEntry{-1, -1};

DirectX11Shader::DirectX11Shader(const std::string &vertexShader, const std::string &fragmentShader,
                                 [[maybe_unused]] bool source) {
    m_sVsh = vertexShader;
    m_sFsh = fragmentShader;

    m_vs = nullptr;
    m_ps = nullptr;
    m_inputLayout = nullptr;
    m_bConstantBuffersUpToDate = false;

    m_prevShader = nullptr;
    m_prevVS = nullptr;
    m_prevPS = nullptr;
    m_prevInputLayout = nullptr;
    for(int i = 0; i < MCENGINE_D3D11_SHADER_MAX_NUM_CONSTANT_BUFFERS; i++) {
        m_prevConstantBuffers.push_back(nullptr);
    }

    // stats
    m_iStatsNumConstantBufferUploadsPerFrameCounter = 0;
    m_iStatsNumConstantBufferUploadsPerFrameCounterEngineFrameCount = 0;
}

void DirectX11Shader::init() {
    const SHADER_PARSE_RESULT parsedVertexShader = parseShaderFromString("DirectX11Interface::VertexShader", m_sVsh);
    const SHADER_PARSE_RESULT parsedPixelShader = parseShaderFromString("DirectX11Interface::PixelShader", m_sFsh);

    bool valid = (parsedVertexShader.descs.size() > 0);
    {
        // parse lines
        std::vector<INPUT_DESC_LINE> inputDescLines;
        std::vector<BIND_DESC_LINE> bindDescLines;
        for(size_t i = 0; i < parsedVertexShader.descs.size(); i++) {
            const std::string &desc = parsedVertexShader.descs[i];
            const std::vector<std::string> tokens = SString::split(desc, "::");

            if(cv::debug_shaders.getBool()) debugLog("descs[{}] = {:s}", (int)i, desc);

            if(tokens.size() > 4) {
                const std::string &descType = tokens[0];

                if(cv::debug_shaders.getBool()) {
                    for(size_t t = 0; t < tokens.size(); t++) {
                        debugLog("descs[{}][{}] = {:s}", (int)i, (int)t, tokens[t]);
                    }
                }

                if(descType == "D3D11_INPUT_ELEMENT_DESC") {
                    const std::string &inputType = tokens[1];      // e.g. VS_INPUT
                    const std::string &inputDataType = tokens[2];  // e.g. POSITION or COLOR0 or TEXCOORD0 etc.
                    const std::string &inputFormat = tokens
                        [3];  // e.g. DXGI_FORMAT_R32G32B32_FLOAT or DXGI_FORMAT_R32G32B32A32_FLOAT or DXGI_FORMAT_R32G32_FLOAT etc.
                    const std::string &inputClassification = tokens[4];  // e.g. D3D11_INPUT_PER_VERTEX_DATA

                    INPUT_DESC_LINE inputDescLine;
                    {
                        inputDescLine.type = inputType;
                        {
                            inputDescLine.dataType = inputDataType;

                            // NOTE: remove integer from end of datatype string (e.g. "COLOR0", "TEXCOORD0" etc., since this is implied by the order and only
                            // necessary in actual shader code as CreateInputLayout() would fail otherwise)
                            if(inputDescLine.dataType.find('0') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('1') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('2') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('3') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('4') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('5') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('6') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('7') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('8') == inputDescLine.dataType.length() - 1 ||
                               inputDescLine.dataType.find('9') == inputDescLine.dataType.length() - 1) {
                                inputDescLine.dataType.erase(inputDescLine.dataType.length() - 1, 1);
                            }
                        }
                        {
                            if(inputFormat == "DXGI_FORMAT_R32_FLOAT") {
                                inputDescLine.dxgiFormat = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
                                inputDescLine.dxgiFormatBytes = 1 * 4;
                            } else if(inputFormat == "DXGI_FORMAT_R32G32_FLOAT") {
                                inputDescLine.dxgiFormat = DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;
                                inputDescLine.dxgiFormatBytes = 2 * 4;
                            } else if(inputFormat == "DXGI_FORMAT_R32G32B32_FLOAT") {
                                inputDescLine.dxgiFormat = DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT;
                                inputDescLine.dxgiFormatBytes = 3 * 4;
                            } else if(inputFormat == "DXGI_FORMAT_R32G32B32A32_FLOAT") {
                                inputDescLine.dxgiFormat = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
                                inputDescLine.dxgiFormatBytes = 4 * 4;
                            } else {
                                valid = false;
                                engine->showMessageError(
                                    "DirectX11Shader Error",
                                    UString::format("Invalid/Unsupported inputFormat \"%s\"", inputFormat.c_str()));
                                break;
                            }
                        }
                        {
                            if(inputClassification == "D3D11_INPUT_PER_VERTEX_DATA")
                                inputDescLine.classification = D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA;
                            else {
                                valid = false;
                                engine->showMessageError(
                                    "DirectX11Shader Error",
                                    UString::format("Invalid/Unsupported inputClassification \"%s\"",
                                                    inputClassification.c_str()));
                                break;
                            }
                        }
                    }
                    inputDescLines.push_back(inputDescLine);
                } else if(descType == "D3D11_BUFFER_DESC") {
                    const std::string &bufferBindType = tokens[1];      // e.g. D3D11_BIND_CONSTANT_BUFFER
                    const std::string &bufferName = tokens[2];          // e.g. ModelViewProjectionConstantBuffer
                    const std::string &bufferVariableName = tokens[3];  // e.g. mvp or col or misc etc.
                    const std::string &bufferVariableType = tokens[4];  // e.g. float4x4 or float4 etc.

                    BIND_DESC_LINE bindDescLine;
                    {
                        bindDescLine.type =
                            descType;  // NOTE: not bufferBindType! since we want to be able to support more than just D3D11_BUFFER_DESC
                        {
                            if(bufferBindType == "D3D11_BIND_CONSTANT_BUFFER")
                                bindDescLine.bindFlag = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
                            else {
                                valid = false;
                                engine->showMessageError("DirectX11Shader Error",
                                                         UString::format("Invalid/Unsupported bufferBindType \"%s\"",
                                                                         bufferBindType.c_str()));
                                break;
                            }
                        }
                        bindDescLine.name = bufferName;
                        bindDescLine.variableName = bufferVariableName.c_str();
                        {
                            bindDescLine.variableType = bufferVariableType;
                            if(bufferVariableType == "float")
                                bindDescLine.variableBytes = 1 * 4;
                            else if(bufferVariableType == "int")
                                bindDescLine.variableBytes = 1 * 4;
                            else if(bufferVariableType == "float2")
                                bindDescLine.variableBytes = 2 * 4;
                            else if(bufferVariableType == "float3")
                                bindDescLine.variableBytes = 3 * 4;
                            else if(bufferVariableType == "float4")
                                bindDescLine.variableBytes = 4 * 4;
                            else if(bufferVariableType == "float4x4")
                                bindDescLine.variableBytes = 4 * 4 * 4;
                            else {
                                valid = false;
                                engine->showMessageError(
                                    "DirectX11Shader Error",
                                    UString::format("Invalid/Unsupported bufferVariableType \"%s\"",
                                                    bufferVariableType.c_str()));
                                break;
                            }
                        }
                    }
                    bindDescLines.push_back(bindDescLine);
                } else {
                    valid = false;
                    engine->showMessageError("DirectX11Shader Error",
                                             UString::format("Invalid/Unsupported descType \"%s\"", descType.c_str()));
                    break;
                }
            } else {
                valid = false;
                engine->showMessageError("DirectX11Shader Error", UString::format("Invalid desc \"%s\"", desc.c_str()));
                break;
            }
        }

        // build m_inputDescs + m_bindDescs
        {
            // compound by INPUT_DESC_LINE::type
            for(size_t i = 0; i < inputDescLines.size(); i++) {
                const INPUT_DESC_LINE &inputDescLine = inputDescLines[i];

                bool alreadyExists = false;
                for(INPUT_DESC &inputDesc : m_inputDescs) {
                    if(inputDesc.type == inputDescLine.type) {
                        alreadyExists = true;
                        {
                            inputDesc.lines.push_back(inputDescLine);
                        }
                        break;
                    }
                }
                if(!alreadyExists) {
                    INPUT_DESC inputDesc;
                    {
                        inputDesc.type = inputDescLine.type;
                    }
                    m_inputDescs.push_back(inputDesc);

                    // (repeat to avoid code duplication)
                    i--;
                    continue;
                }
            }

            // compound by BIND_DESC_LINE::name
            for(size_t i = 0; i < bindDescLines.size(); i++) {
                const BIND_DESC_LINE &bindDescLine = bindDescLines[i];

                bool alreadyExists = false;
                for(BIND_DESC &bindDesc : m_bindDescs) {
                    if(bindDesc.name == bindDescLine.name) {
                        alreadyExists = true;
                        {
                            bindDesc.lines.push_back(bindDescLine);
                            bindDesc.floats.resize(bindDesc.floats.size() +
                                                   (bindDescLine.variableBytes / sizeof(float)));

                            if(cv::debug_shaders.getBool())
                                debugLog("bindDesc[{:s}].floats.size() = {}", bindDescLine.name.c_str(),
                                         (int)bindDesc.floats.size());
                        }
                        break;
                    }
                }

                if(!alreadyExists) {
                    BIND_DESC bindDesc;
                    {
                        bindDesc.name = bindDescLine.name;
                    }
                    m_bindDescs.push_back(bindDesc);

                    // (repeat to avoid code duplication)
                    i--;
                    continue;
                }
            }

            // error checking
            if(m_inputDescs.size() < 1) {
                valid = false;
                engine->showMessageError("DirectX11Shader Error",
                                         "Missing at least one D3D11_INPUT_ELEMENT_DESC instance");
            } else if(m_bindDescs.size() < 1) {
                // (there could theoretically be a shader without any buffers bound, so this is not an error)
            }

            if(cv::debug_shaders.getBool()) {
                for(size_t i = 0; i < m_inputDescs.size(); i++) {
                    debugLog("m_inputDescs[{}] = \"{:s}\", has {} line(s)", (int)i, m_inputDescs[i].type.c_str(),
                             (int)m_inputDescs[i].lines.size());
                }

                for(size_t i = 0; i < m_bindDescs.size(); i++) {
                    debugLog("m_bindDescs[{}] = \"{:s}\", has {} lines(s)", (int)i, m_bindDescs[i].name.c_str(),
                             (int)m_bindDescs[i].lines.size());
                }
            }
        }
    }

    this->bReady = compile((valid ? parsedVertexShader.source : ""), (valid ? parsedPixelShader.source : ""));
}

void DirectX11Shader::initAsync() { this->bAsyncReady = true; }

void DirectX11Shader::destroy() {
    for(ID3D11Buffer *buffer : m_constantBuffers) {
        buffer->Release();
    }
    m_constantBuffers.clear();

    if(m_inputLayout != nullptr) {
        m_inputLayout->Release();
        m_inputLayout = nullptr;
    }

    if(m_vs != nullptr) {
        m_vs->Release();
        m_vs = nullptr;
    }

    if(m_ps != nullptr) {
        m_ps->Release();
        m_ps = nullptr;
    }

    m_inputDescs.clear();
    m_bindDescs.clear();

    m_uniformLocationCache.clear();
}

void DirectX11Shader::enable() {
    auto *dx11 = static_cast<DirectX11Interface *>(g.get());
    if(!this->bReady || dx11->getActiveShader() == this) return;

    // backup
    // HACKHACK: slow af
    {
        m_prevShader = dx11->getActiveShader();

        dx11->getDeviceContext()->IAGetInputLayout(&m_prevInputLayout);
        dx11->getDeviceContext()->VSGetShader(&m_prevVS, nullptr, nullptr);
        dx11->getDeviceContext()->PSGetShader(&m_prevPS, nullptr, nullptr);
        dx11->getDeviceContext()->VSGetConstantBuffers(0, (UINT)m_prevConstantBuffers.size(),
                                                       &m_prevConstantBuffers[0]);
    }

    dx11->getDeviceContext()->IASetInputLayout(m_inputLayout);
    dx11->getDeviceContext()->VSSetShader(m_vs, nullptr, 0);
    dx11->getDeviceContext()->PSSetShader(m_ps, nullptr, 0);

    dx11->getDeviceContext()->VSSetConstantBuffers(0, (UINT)m_constantBuffers.size(), &m_constantBuffers[0]);

    dx11->setActiveShader(this);
}

void DirectX11Shader::disable() {
    auto *dx11 = static_cast<DirectX11Interface *>(g.get());
    if(!this->bReady || dx11->getActiveShader() != this) return;

    // restore
    // HACKHACK: slow af
    {
        UINT numPrevConstantBuffers = 0;
        for(size_t i = 0; i < m_prevConstantBuffers.size(); i++) {
            if(m_prevConstantBuffers[i] != nullptr)
                numPrevConstantBuffers++;
            else
                break;
        }

        dx11->getDeviceContext()->IASetInputLayout(m_prevInputLayout);
        dx11->getDeviceContext()->VSSetShader(m_prevVS, nullptr, 0);
        dx11->getDeviceContext()->PSSetShader(m_prevPS, nullptr, 0);
        dx11->getDeviceContext()->VSSetConstantBuffers(0, numPrevConstantBuffers, &m_prevConstantBuffers[0]);

        // refcount
        {
            if(m_prevInputLayout != nullptr) {
                m_prevInputLayout->Release();
                m_prevInputLayout = nullptr;
            }

            if(m_prevVS != nullptr) {
                m_prevVS->Release();
                m_prevVS = nullptr;
            }

            if(m_prevPS != nullptr) {
                m_prevPS->Release();
                m_prevPS = nullptr;
            }

            for(size_t i = 0; i < m_prevConstantBuffers.size(); i++) {
                if(m_prevConstantBuffers[i] != nullptr) {
                    m_prevConstantBuffers[i]->Release();
                    m_prevConstantBuffers[i] = nullptr;
                }
            }
        }

        dx11->setActiveShader(m_prevShader);
    }
}

void DirectX11Shader::setUniform1f(std::string_view name, float value) { setUniform(name, &value, sizeof(float) * 1); }

void DirectX11Shader::setUniform1fv(std::string_view name, int count, float *values) {
    setUniform(name, values, sizeof(float) * 1 * count);
}

void DirectX11Shader::setUniform1i(std::string_view name, int value) { setUniform(name, &value, sizeof(int32_t) * 1); }

void DirectX11Shader::setUniform2f(std::string_view name, float x, float y) {
    vec2 f(x, y);
    setUniform(name, &f[0], sizeof(float) * 2);
}

void DirectX11Shader::setUniform2fv(std::string_view name, int count, float *vectors) {
    setUniform(name, vectors, sizeof(float) * 2 * count);
}

void DirectX11Shader::setUniform3f(std::string_view name, float x, float y, float z) {
    vec3 f(x, y, z);
    setUniform(name, &f[0], sizeof(float) * 3);
}

void DirectX11Shader::setUniform3fv(std::string_view name, int count, float *vectors) {
    setUniform(name, vectors, sizeof(float) * 3 * count);
}

void DirectX11Shader::setUniform4f(std::string_view name, float x, float y, float z, float w) {
    vec4 f(x, y, z, w);
    setUniform(name, &f[0], sizeof(float) * 4);
}

void DirectX11Shader::setUniformMatrix4fv(std::string_view name, Matrix4 &matrix) {
    setUniformMatrix4fv(name, (float *)matrix.getTranspose());
}

void DirectX11Shader::setUniformMatrix4fv(std::string_view name, float *v) {
    setUniform(name, v, sizeof(float) * 4 * 4);
}

void DirectX11Shader::setUniform(std::string_view name, void *src, size_t numBytes) {
    if(!this->bReady) return;

    const CACHE_ENTRY cacheEntry = getAndCacheUniformLocation(name);
    if(cacheEntry.bindIndex > -1) {
        BIND_DESC &bindDesc = m_bindDescs[cacheEntry.bindIndex];

        if(memcmp(src, &bindDesc.floats[cacheEntry.offsetBytes / sizeof(float)], numBytes) !=
           0)  // NOTE: ignore redundant updates
        {
            memcpy(&bindDesc.floats[cacheEntry.offsetBytes / sizeof(float)], src, numBytes);

            // NOTE: uniforms will be lazy updated later in onJustBeforeDraw() below
            // NOTE: this way we concatenate multiple uniform updates into one single gpu memory transfer
            m_bConstantBuffersUpToDate = false;
        }
    }
}

void DirectX11Shader::onJustBeforeDraw() {
    if(!this->bReady) return;

    // lazy update uniforms
    if(!m_bConstantBuffersUpToDate) {
        auto *dx11 = static_cast<DirectX11Interface *>(g.get());

        for(size_t i = 0; i < m_constantBuffers.size(); i++) {
            ID3D11Buffer *constantBuffer = m_constantBuffers[i];
            BIND_DESC &bindDesc = m_bindDescs[i];

            // lock
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            if(FAILED(dx11->getDeviceContext()->Map(constantBuffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0,
                                                    &mappedResource))) {
                debugLog("ERROR: failed to Map()!");
                continue;
            }

            // write
            memcpy(mappedResource.pData, &bindDesc.floats[0], bindDesc.floats.size() * sizeof(float));

            // unlock
            dx11->getDeviceContext()->Unmap(constantBuffer, 0);

            // stats
            {
                if(engine->getFrameCount() == m_iStatsNumConstantBufferUploadsPerFrameCounterEngineFrameCount)
                    m_iStatsNumConstantBufferUploadsPerFrameCounter++;
                else {
                    m_iStatsNumConstantBufferUploadsPerFrameCounterEngineFrameCount = engine->getFrameCount();
                    m_iStatsNumConstantBufferUploadsPerFrameCounter = 1;
                }
            }
        }

        m_bConstantBuffersUpToDate = true;
    }
}

const DirectX11Shader::CACHE_ENTRY DirectX11Shader::getAndCacheUniformLocation(std::string_view name) {
    if(!this->bReady) return invalidCacheEntry;

    const auto cachedValue = m_uniformLocationCache.find(name);
    const bool isCached = (cachedValue != m_uniformLocationCache.end());

    if(isCached)
        return cachedValue->second;
    else {
        CACHE_ENTRY newCacheEntry;
        {
            newCacheEntry.bindIndex = -1;
            newCacheEntry.offsetBytes = -1;

            for(size_t i = 0; i < m_bindDescs.size(); i++) {
                const BIND_DESC &bindDesc = m_bindDescs[i];
                int offsetBytesCounter = 0;
                for(size_t j = 0; j < bindDesc.lines.size(); j++) {
                    const BIND_DESC_LINE &bindDescLine = bindDesc.lines[j];
                    if(bindDescLine.variableName == name) {
                        newCacheEntry.bindIndex = (int)i;
                        newCacheEntry.offsetBytes = offsetBytesCounter;
                        break;
                    } else
                        offsetBytesCounter += bindDescLine.variableBytes;
                }
            }

            if(newCacheEntry.bindIndex > -1 && newCacheEntry.offsetBytes > -1) {
                m_uniformLocationCache[name] = newCacheEntry;
                return newCacheEntry;
            } else if(cv::debug_shaders.getBool())
                debugLog("DirectX11Shader Warning: Can't find uniform {:s}", name);
        }
    }

    return invalidCacheEntry;
}
#ifdef MCENGINE_PLATFORM_LINUX

void *DirectX11Shader::s_vkd3dHandle = nullptr;
PFN_D3DCOMPILE_VKD3D DirectX11Shader::s_d3dCompileFunc = nullptr;

bool DirectX11Shader::loadLibs() {
    if(s_d3dCompileFunc != nullptr) return true;  // already initialized

    setenv("DXVK_WSI_DRIVER", "SDL3", 1);

    // try to load vkd3d-utils
    s_vkd3dHandle = dlopen("libvkd3d-utils.so.1", RTLD_NOW | RTLD_LOCAL);
    if(s_vkd3dHandle == nullptr) {
        debugLog("DirectX11Shader: Failed to load libvkd3d-utils.so.1: {:s}", dlerror());
        return false;
    }

    // get D3DCompile function pointer
    s_d3dCompileFunc = (PFN_D3DCOMPILE_VKD3D)dlsym(s_vkd3dHandle, "D3DCompile");
    if(s_d3dCompileFunc == nullptr) {
        debugLog("DirectX11Shader: Failed to find D3DCompile in vkd3d-utils: {:s}", dlerror());
        dlclose(s_vkd3dHandle);
        s_vkd3dHandle = nullptr;
        return false;
    }

    // check vkd3d version compatibility
    const char *(*vkd3d_shader_get_version)(unsigned int *, unsigned int *);
    vkd3d_shader_get_version =
        (const char *(*)(unsigned int *, unsigned int *))dlsym(s_vkd3dHandle, "vkd3d_shader_get_version");
    if(vkd3d_shader_get_version != nullptr) {
        unsigned int major, minor;
        vkd3d_shader_get_version(&major, &minor);
        if(!((major > 1) || (major == 1 && minor >= 10))) {
            debugLog("DirectX11Shader: vkd3d version {}.{} is too old, need >= 1.10", major, minor);
            return false;
        }
        debugLog("DirectX11Shader: Using vkd3d version {}.{}", major, minor);
    }

    return true;
}

void DirectX11Shader::cleanupLibs() {
    if(s_vkd3dHandle != nullptr) {
        dlclose(s_vkd3dHandle);
        s_vkd3dHandle = nullptr;
    }
    s_d3dCompileFunc = nullptr;
}

#else

bool DirectX11Shader::loadLibs() { return true; }

void DirectX11Shader::cleanupLibs() { ; }

#endif

void *DirectX11Shader::getBlobBufferPointer(ID3DBlob *blob) {
#ifdef MCENGINE_PLATFORM_LINUX
    auto *vkdBlob = (VKD3DBlob *)blob;
    return vkdBlob->lpVtbl->GetBufferPointer(vkdBlob);
#else
    return blob->GetBufferPointer();
#endif
}

SIZE_T DirectX11Shader::getBlobBufferSize(ID3DBlob *blob) {
#ifdef MCENGINE_PLATFORM_LINUX
    auto *vkdBlob = (VKD3DBlob *)blob;
    return vkdBlob->lpVtbl->GetBufferSize(vkdBlob);
#else
    return blob->GetBufferSize();
#endif
}

void DirectX11Shader::releaseBlob(ID3DBlob *blob) {
    if(blob != nullptr) {
#ifdef MCENGINE_PLATFORM_LINUX
        auto *vkdBlob = (VKD3DBlob *)blob;
        vkdBlob->lpVtbl->Release(vkdBlob);
#else
        blob->Release();
#endif
    }
}

bool DirectX11Shader::compile(const std::string &vertexShader, const std::string &fragmentShader) {
    if(vertexShader.length() < 1 || fragmentShader.length() < 1) return false;

    auto *dx11 = static_cast<DirectX11Interface *>(g.get());

    const char *vsProfile =
        (dx11->getDevice()->GetFeatureLevel() >= D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0 ? "vs_5_0" : "vs_4_0");
    const char *psProfile =
        (dx11->getDevice()->GetFeatureLevel() >= D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0 ? "ps_5_0" : "ps_4_0");
    const char *vsEntryPoint = "vsmain";
    const char *psEntryPoint = "psmain";

    UINT flags = 0;

    if(cv::debug_shaders.getBool()) flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    // const D3D_SHADER_MACRO defines[] = {
    //     {"EXAMPLE_DEFINE", "1" },
    //     {nullptr,             nullptr}  // sentinel
    // };

    ID3DBlob *vs = nullptr;
    ID3DBlob *ps = nullptr;
    ID3DBlob *vsError = nullptr;
    ID3DBlob *psError = nullptr;

    // convert to persistent c strings to avoid lifetime issues
    std::string vsSource = vertexShader.c_str();
    std::string psSource = fragmentShader.c_str();

    HRESULT hr1{S_OK}, hr2{S_OK};

#ifdef MCENGINE_PLATFORM_LINUX
    // use vkd3d function pointer with proper calling convention
    debugLog("DirectX11Shader: (VKD3D) D3DCompile()-ing vertex shader ...");
    hr1 = DirectX11Shader::s_d3dCompileFunc(vsSource.c_str(), vsSource.length(), "VS_SOURCE", nullptr /*defines*/,
                                            nullptr, vsEntryPoint, vsProfile, flags, 0, &vs, &vsError);

    debugLog("DirectX11Shader: (VKD3D) D3DCompile()-ing pixel shader ...");
    hr2 = DirectX11Shader::s_d3dCompileFunc(psSource.c_str(), psSource.length(), "PS_SOURCE", nullptr /*defines*/,
                                            nullptr, psEntryPoint, psProfile, flags, 0, &ps, &psError);
#else
    // use standard D3DCompile on Windows
    debugLog("DirectX11Shader: D3DCompile()-ing vertex shader ...");
    hr1 = D3DCompile(vsSource.c_str(), vsSource.length(), "VS_SOURCE", nullptr /*defines*/, nullptr, vsEntryPoint,
                     vsProfile, flags, 0, &vs, &vsError);

    debugLog("DirectX11Shader: D3DCompile()-ing pixel shader ...");
    hr2 = D3DCompile(psSource.c_str(), psSource.length(), "PS_SOURCE", nullptr /*defines*/, nullptr, psEntryPoint,
                     psProfile, flags, 0, &ps, &psError);
#endif

    // check compilation results
    if(FAILED(hr1) || FAILED(hr2) || vs == nullptr || ps == nullptr) {
        if(vsError != nullptr) {
            debugLog("DirectX11Shader Vertex Shader Error: \n{:s}", (const char *)getBlobBufferPointer(vsError));
            releaseBlob(vsError);
            debugLog("");
        }

        if(psError != nullptr) {
            debugLog("DirectX11Shader Pixel Shader Error: \n{:s}", (const char *)getBlobBufferPointer(psError));
            releaseBlob(psError);
            debugLog("");
        }

        // clean up on failure
        releaseBlob(vs);
        releaseBlob(ps);

        engine->showMessageError("DirectX11Shader Error", "Couldn't D3DCompile()! Check the log for details.");
        return false;
    }

    // create vertex shader
    debugLog("DirectX11Shader: CreateVertexShader({}) ...", getBlobBufferSize(vs));
    hr1 = dx11->getDevice()->CreateVertexShader(getBlobBufferPointer(vs), getBlobBufferSize(vs), nullptr, &m_vs);

    // create pixel shader
    debugLog("DirectX11Shader: CreatePixelShader({}) ...", getBlobBufferSize(ps));
    hr2 = dx11->getDevice()->CreatePixelShader(getBlobBufferPointer(ps), getBlobBufferSize(ps), nullptr, &m_ps);
    releaseBlob(ps);

    if(FAILED(hr1) || FAILED(hr2)) {
        releaseBlob(vs);

        engine->showMessageError("DirectX11Shader Error", "Couldn't CreateVertexShader()/CreatePixelShader()!");
        return false;
    }

    // create the input layout (rest of the function remains the same)
    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    {
        const INPUT_DESC &inputDesc = m_inputDescs[0];
        UINT alignedByteOffsetCounter = 0;
        for(const INPUT_DESC_LINE &inputDescLine : inputDesc.lines) {
            D3D11_INPUT_ELEMENT_DESC element;
            {
                element.SemanticName = inputDescLine.dataType.c_str();
                element.SemanticIndex = 0;
                element.Format = inputDescLine.dxgiFormat;
                element.InputSlot = 0;
                element.AlignedByteOffset = alignedByteOffsetCounter;
                element.InputSlotClass = inputDescLine.classification;
                element.InstanceDataStepRate = 0;

                if(cv::debug_shaders.getBool())
                    debugLog("{:s}, {:d}, {:d}, {:d}, {:d}, {:d}, {:d}", element.SemanticName,
                             (int)element.SemanticIndex, (int)element.Format, (int)element.InputSlot,
                             (int)element.AlignedByteOffset, (int)element.InputSlotClass,
                             (int)element.InstanceDataStepRate);
            }
            elements.push_back(element);

            alignedByteOffsetCounter += inputDescLine.dxgiFormatBytes;
        }
    }

    hr1 = dx11->getDevice()->CreateInputLayout(&elements[0], elements.size(), getBlobBufferPointer(vs),
                                               getBlobBufferSize(vs), &m_inputLayout);
    releaseBlob(vs);

    if(FAILED(hr1)) {
        engine->showMessageError("DirectX11Shader Error", "Couldn't CreateInputLayout()!");
        return false;
    }

    // create binds/buffers (rest remains the same)
    for(const BIND_DESC &bindDesc : m_bindDescs) {
        const std::string &descType = bindDesc.lines[0].type;

        if(descType == "D3D11_BUFFER_DESC") {
            const D3D11_BIND_FLAG bindFlag = bindDesc.lines[0].bindFlag;
            const std::string &name = bindDesc.lines[0].name;

            UINT byteWidth = 0;
            for(const BIND_DESC_LINE &bindDescLine : bindDesc.lines) {
                byteWidth += bindDescLine.variableBytes;
            }

            if(byteWidth % 16 != 0) {
                engine->showMessageError("DirectX11Shader Error",
                                         UString::format("Invalid byteWidth %i for \"%s\" (must be a multiple of 16)",
                                                         (int)byteWidth, name.c_str()));
                return false;
            }

            D3D11_BUFFER_DESC bufferDesc;
            {
                bufferDesc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
                bufferDesc.ByteWidth = byteWidth;
                bufferDesc.BindFlags = bindFlag;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
                bufferDesc.MiscFlags = 0;
                bufferDesc.StructureByteStride = 0;
            }
            ID3D11Buffer *buffer = nullptr;
            hr1 = dx11->getDevice()->CreateBuffer(&bufferDesc, nullptr, &buffer);
            if(FAILED(hr1) || buffer == nullptr) {
                engine->showMessageError("DirectX11Shader Error", UString::format("Couldn't CreateBuffer(%ld, %x, %x)!",
                                                                                  hr1, hr1, MAKE_DXGI_HRESULT(hr1)));
                return false;
            }

            m_constantBuffers.push_back(buffer);
        } else {
            engine->showMessageError("DirectX11Shader Error",
                                     UString::format("Invalid descType \"%s\"", descType.c_str()));
            return false;
        }
    }

    return true;
}

DirectX11Shader::SHADER_PARSE_RESULT DirectX11Shader::parseShaderFromString(
    const std::string &graphicsInterfaceAndShaderTypePrefix, const std::string &shaderSource) {
    DirectX11Shader::SHADER_PARSE_RESULT result;

    const std::string shaderPrefix =
        "###";  // e.g. ###OpenGLLegacyInterface::VertexShader##########################################################################################
    const std::string descPrefix =
        "##";  // e.g. ##D3D11_BUFFER_DESC::D3D11_BIND_CONSTANT_BUFFER::ModelViewProjectionConstantBuffer::mvp::float4x4

    {
        std::istringstream ss(shaderSource);

        bool foundGraphicsInterfaceAndShaderTypePrefixAtLeastOnce = false;
        bool foundGraphicsInterfaceAndShaderTypePrefix = false;
        std::string curLine;

        while(static_cast<bool>(std::getline(ss, curLine))) {
            const bool isShaderPrefixLine = (curLine.contains(shaderPrefix));

            if(isShaderPrefixLine) {
                if(!foundGraphicsInterfaceAndShaderTypePrefix) {
                    if(curLine.find(graphicsInterfaceAndShaderTypePrefix) == shaderPrefix.length()) {
                        foundGraphicsInterfaceAndShaderTypePrefix = true;
                        foundGraphicsInterfaceAndShaderTypePrefixAtLeastOnce = true;
                    }
                } else
                    foundGraphicsInterfaceAndShaderTypePrefix = false;
            } else if(foundGraphicsInterfaceAndShaderTypePrefix) {
                const bool isDescPrefixLine = (curLine.find(descPrefix) == 0);

                if(!isDescPrefixLine) {
                    if(!result.source.empty()) result.source.push_back('\n');
                    result.source.append(curLine);
                } else {
                    result.descs.push_back(curLine.substr(descPrefix.length()));
                }
            }
        }

        if(!foundGraphicsInterfaceAndShaderTypePrefixAtLeastOnce)
            engine->showMessageError(
                "Shader Error", UString::format("Missing \"%s\" in shader %s",
                                                graphicsInterfaceAndShaderTypePrefix.c_str(), shaderSource.c_str()));
    }

    return result;
}

#endif
