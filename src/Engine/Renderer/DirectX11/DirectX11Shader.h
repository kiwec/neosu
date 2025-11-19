//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		DirectX HLSL implementation of Shader
//
// $NoKeywords: $dxshader
//===============================================================================//

#pragma once

#ifndef DIRECTX11SHADER_H
#define DIRECTX11SHADER_H
#include "config.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "Shader.h"

#include "templates.h"

#include <vector>

#if defined(__GNUC__) || defined(__clang__)
MC_DO_PRAGMA(GCC diagnostic ignored "-Wpragmas")
MC_DO_PRAGMA(GCC diagnostic ignored "-Wextern-c-compat")
MC_DO_PRAGMA(GCC diagnostic push)
#endif

#include "d3d11.h"

#if defined(__GNUC__) || defined(__clang__)
MC_DO_PRAGMA(GCC diagnostic pop)
#endif

namespace dynutils {
using lib_obj = struct lib_obj;
}

// calling convention handling
#ifdef _MSC_VER
#define D3D_CALL WINAPI
#else
#ifdef __x86_64__
#define D3D_CALL __attribute__((ms_abi))
#else
#define D3D_CALL __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#endif
#endif

// d3d blob interface
typedef struct D3D_Blob D3D_Blob;
typedef struct D3D_BlobVtbl {
    HRESULT(D3D_CALL *QueryInterface)(D3D_Blob *This, REFIID riid, void **ppvObject);
    ULONG(D3D_CALL *AddRef)(D3D_Blob *This);
    ULONG(D3D_CALL *Release)(D3D_Blob *This);
    void *(D3D_CALL *GetBufferPointer)(D3D_Blob *This);
    SIZE_T(D3D_CALL *GetBufferSize)(D3D_Blob *This);
} D3D_BlobVtbl;

struct D3D_Blob {
    const D3D_BlobVtbl *lpVtbl;
};

// function pointer type for D3DCompile with proper calling convention
typedef HRESULT(D3D_CALL D3DCompile_t)(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
                                       const D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude, LPCSTR pEntrypoint,
                                       LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob **ppCode,
                                       ID3DBlob **ppErrorMsgs);

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
    bool bStateBackedUp{false};

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

    // dynloading
    static dynutils::lib_obj *s_d3dCompilerHandle;
    static D3DCompile_t *s_d3dCompileFunc;

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

#endif

#endif
