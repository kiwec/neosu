//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX HLSL implementation of Shader
//
// $NoKeywords: $dxshader
//===============================================================================//

#pragma once

#ifndef DIRECTX11SHADER_H
#define DIRECTX11SHADER_H

#include "Shader.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "templates.h"

#include <vector>

#include "d3d11.h"

#ifdef MCENGINE_PLATFORM_LINUX
namespace dynutils {
using lib_obj = struct lib_obj;
}

// calling convention handling for vkd3d compatibility
#ifdef __x86_64__
#define VKD3D_CALL __attribute__((ms_abi))
#else
#define VKD3D_CALL __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#endif

// vkd3d blob interface
typedef struct VKD3DBlob VKD3DBlob;
typedef struct VKD3DBlobVtbl {
    HRESULT(VKD3D_CALL *QueryInterface)(VKD3DBlob *This, REFIID riid, void **ppvObject);
    ULONG(VKD3D_CALL *AddRef)(VKD3DBlob *This);
    ULONG(VKD3D_CALL *Release)(VKD3DBlob *This);
    void *(VKD3D_CALL *GetBufferPointer)(VKD3DBlob *This);
    SIZE_T(VKD3D_CALL *GetBufferSize)(VKD3DBlob *This);
} VKD3DBlobVtbl;

struct VKD3DBlob {
    const VKD3DBlobVtbl *lpVtbl;
};

// function pointer type for D3DCompile with proper calling convention
typedef HRESULT(VKD3D_CALL *PFN_D3DCOMPILE_VKD3D)(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
                                                  const D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude,
                                                  LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
                                                  ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs);
#endif

class DirectX11Shader final : public Shader {
    NOCOPY_NOMOVE(DirectX11Shader);

   public:
    DirectX11Shader(std::string vertexShader, std::string fragmentShader, bool source = true);
    ~DirectX11Shader() override { destroy(); }

    void enable() override;
    void disable() override;

    void setUniform1f(std::string_view name, float value) override;
    void setUniform1fv(std::string_view name, int count, const float *const values) override;
    void setUniform1i(std::string_view name, int value) override;
    void setUniform2f(std::string_view name, float x, float y) override;
    void setUniform2fv(std::string_view name, int count, const float *const vectors) override;
    void setUniform3f(std::string_view name, float x, float y, float z) override;
    void setUniform3fv(std::string_view name, int count, const float *const vectors) override;
    void setUniform4f(std::string_view name, float x, float y, float z, float w) override;
    void setUniformMatrix4fv(std::string_view name, const Matrix4 &matrix) override;
    void setUniformMatrix4fv(std::string_view name, const float *const v) override;

    // ILLEGAL:
    void onJustBeforeDraw();
    inline unsigned long getStatsNumConstantBufferUploadsPerFrame() const {
        return this->iStatsNumConstantBufferUploadsPerFrameCounter;
    }
    inline unsigned long getStatsNumConstantBufferUploadsPerFrameEngineFrameCount() const {
        return this->iStatsNumConstantBufferUploadsPerFrameCounterEngineFrameCount;
    }

    static bool loadLibs();
    static void cleanupLibs();

   private:
    struct INPUT_DESC_LINE {
        std::string type;        // e.g. "VS_INPUT"
        std::string dataType;    // e.g. "POSITION", "COLOR0", "TEXCOORD0", etc.
        DXGI_FORMAT dxgiFormat;  // e.g. DXGI_FORMAT_R32G32B32_FLOAT, etc.
        int dxgiFormatBytes;     // e.g. "DXGI_FORMAT_R32G32B32_FLOAT" -> 12, etc.
        D3D11_INPUT_CLASSIFICATION
        classification;  // e.g. D3D11_INPUT_PER_INSTANCE_DATA
    };

    struct BIND_DESC_LINE {
        std::string type;          // e.g. "D3D11_BUFFER_DESC"
        D3D11_BIND_FLAG bindFlag;  // e.g. D3D11_BIND_CONSTANT_BUFFER
        std::string name;          // e.g. "ModelViewProjectionConstantBuffer"
        std::string variableName;  // e.g. "mvp", "col", "misc", etc.
        std::string variableType;  // e.g. "float4x4", "float4", "float3", "float2", "float", etc.
        int variableBytes;  // e.g. 16 -> "float4x4", 4 -> "float4", 3 -> "float3, 2 -> "float2", 1 -> "float", etc.
    };

    struct INPUT_DESC {
        std::string type;  // INPUT_DESC_LINE::type
        std::vector<INPUT_DESC_LINE> lines;
    };

    struct BIND_DESC {
        std::string name;  // BIND_DESC_LINE::name
        std::vector<BIND_DESC_LINE> lines;
        std::vector<float> floats;
    };

   private:
    struct CACHE_ENTRY {
        int bindIndex{-1};  // into m_bindDescs[bindIndex] and m_constantBuffers[bindIndex]
        int offsetBytes{-1};
    };

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

    bool compile(const std::string &vertexShader, const std::string &fragmentShader);

    void setUniform(std::string_view name, const void *const src, size_t numBytes);

    const CACHE_ENTRY getAndCacheUniformLocation(std::string_view name);

   private:
    static constexpr const CACHE_ENTRY invalidCacheEntry{-1, -1};

    std::string sVsh, sFsh;

    ID3D11VertexShader *vs{nullptr};
    ID3D11PixelShader *ps{nullptr};
    ID3D11InputLayout *inputLayout{nullptr};
    std::vector<ID3D11Buffer *> constantBuffers;
    bool bConstantBuffersUpToDate{false};

    DirectX11Shader *prevShader{nullptr};
    ID3D11VertexShader *prevVS{nullptr};
    ID3D11PixelShader *prevPS{nullptr};
    ID3D11InputLayout *prevInputLayout{nullptr};
    std::vector<ID3D11Buffer *> prevConstantBuffers;

    std::vector<INPUT_DESC> inputDescs;
    std::vector<BIND_DESC> bindDescs;

    sv_unordered_map<CACHE_ENTRY> uniformLocationCache;

    // stats
    unsigned long iStatsNumConstantBufferUploadsPerFrameCounter{0};
    unsigned long iStatsNumConstantBufferUploadsPerFrameCounterEngineFrameCount{0};

#ifdef MCENGINE_PLATFORM_LINUX
    // loading (dxvk-native)
    static dynutils::lib_obj *s_vkd3dHandle;
    static PFN_D3DCOMPILE_VKD3D s_d3dCompileFunc;
#endif
    // wrapper functions for dx blob ops
    static void *getBlobBufferPointer(ID3DBlob *blob);
    static SIZE_T getBlobBufferSize(ID3DBlob *blob);
    static void releaseBlob(ID3DBlob *blob);

    struct SHADER_PARSE_RESULT {
        std::string source;
        std::vector<std::string> descs;
    };

    SHADER_PARSE_RESULT parseShaderFromString(const std::string &graphicsInterfaceAndShaderTypePrefix,
                                              const std::string &shaderSource);
};

#else
class DirectX11Shader : public Shader {};
#endif

#endif
