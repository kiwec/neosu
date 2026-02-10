//================ Copyright (c) 2026, WH, All rights reserved. =================//
//
// Purpose:		SDL_gpu implementation of Image
//
// $NoKeywords: $sdlgpuimg
//===============================================================================//
#include "config.h"

#ifdef MCENGINE_FEATURE_SDLGPU

#include <SDL3/SDL_gpu.h>

#include "SDLGPUImage.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include "SDLGPUInterface.h"

#include <cstring>

SDLGPUImage::SDLGPUImage(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : Image(std::move(filepath), mipmapped, keepInSystemMemory) {}

SDLGPUImage::SDLGPUImage(int width, int height, bool mipmapped, bool keepInSystemMemory)
    : Image(width, height, mipmapped, keepInSystemMemory) {}

SDLGPUImage::~SDLGPUImage() {
    this->destroy();

    auto *device = static_cast<SDLGPUInterface *>(g.get())->getDevice();

    if(m_sampler) SDL_ReleaseGPUSampler(device, m_sampler);
    if(m_texture) SDL_ReleaseGPUTexture(device, m_texture);

    this->rawImage.clear();
}

void SDLGPUImage::init() {
    if((m_texture != nullptr && !this->bKeepInSystemMemory) || !this->isAsyncReady()) {
        if(cv::debug_image.getBool()) {
            debugLog("SDLGPUImage: already loaded, bReady: {} texture: {:p} bKeepInSystemMemory: {} bAsyncReady: {}",
                     this->isReady(), fmt::ptr(m_texture), this->bKeepInSystemMemory, this->isAsyncReady());
        }
        return;
    }

    auto *device = static_cast<SDLGPUInterface *>(g.get())->getDevice();

    // calculate mip levels: cap to 32px smallest mipmap (same as OpenGL/DX11)
    const u32 maxDim = (u32)std::max(this->iWidth, this->iHeight);
    const u32 mipLevels = this->bMipmapped ? (u32)std::max(1, (int)std::floor(std::log2(maxDim)) - 4) : 1;

    // create texture (or re-upload to existing)
    if(m_texture == nullptr) {
        SDL_GPUTextureCreateInfo texInfo{};
        texInfo.type = SDL_GPU_TEXTURETYPE_2D;
        texInfo.format = (SDL_GPUTextureFormat)SDLGPUInterface::DEFAULT_TEXTURE_FORMAT;
        texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if(this->bMipmapped) texInfo.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;  // needed for GenerateMipmaps
        texInfo.width = (u32)this->iWidth;
        texInfo.height = (u32)this->iHeight;
        texInfo.layer_count_or_depth = 1;
        texInfo.num_levels = mipLevels;
        texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        m_texture = SDL_CreateGPUTexture(device, &texInfo);
        if(!m_texture) {
            debugLog("SDLGPUImage Error: Couldn't CreateGPUTexture() on file {:s}: {}", this->sFilePath,
                     SDL_GetError());
            return;
        }
    }

    // upload pixel data
    if(this->totalBytes() >= (u64)this->iWidth * this->iHeight * Image::NUM_CHANNELS) {
        this->uploadPixelData();
    }

    // free raw image
    if(!this->bKeepInSystemMemory) this->rawImage.clear();

    // create sampler (applies current filter/wrap mode)
    if(m_sampler == nullptr) {
        // set up defaults then apply any non-default modes
        if(this->filterMode != TextureFilterMode::LINEAR) setFilterMode(this->filterMode);
        if(this->wrapMode != TextureWrapMode::CLAMP) setWrapMode(this->wrapMode);
        createOrUpdateSampler();
    }

    if(!m_sampler) {
        debugLog("SDLGPUImage Error: Couldn't CreateGPUSampler() on file {:s}!", this->sFilePath);
        return;
    }

    // generate mipmaps
    if(this->bMipmapped) {
        auto *cmdBuf = SDL_AcquireGPUCommandBuffer(device);
        if(cmdBuf) {
            SDL_GenerateMipmapsForGPUTexture(cmdBuf, m_texture);
            SDL_SubmitGPUCommandBuffer(cmdBuf);
        }
    }

    this->setReady(true);
}

void SDLGPUImage::uploadPixelData() {
    auto *device = static_cast<SDLGPUInterface *>(g.get())->getDevice();
    const u32 dataSize = (u32)this->totalBytes();

    // create a temporary transfer buffer
    SDL_GPUTransferBufferCreateInfo tbInfo{};
    tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size = dataSize;

    auto *transferBuf = SDL_CreateGPUTransferBuffer(device, &tbInfo);
    if(!transferBuf) return;

    // map and copy pixel data
    void *mapped = SDL_MapGPUTransferBuffer(device, transferBuf, false);
    if(mapped) {
        std::memcpy(mapped, this->rawImage.get(), dataSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuf);
    }

    // upload via copy pass
    auto *cmdBuf = SDL_AcquireGPUCommandBuffer(device);
    if(cmdBuf) {
        auto *copyPass = SDL_BeginGPUCopyPass(cmdBuf);
        if(copyPass) {
            SDL_GPUTextureTransferInfo src{};
            src.transfer_buffer = transferBuf;

            SDL_GPUTextureRegion dst{};
            dst.texture = m_texture;
            dst.w = (u32)this->iWidth;
            dst.h = (u32)this->iHeight;
            dst.d = 1;

            SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
            SDL_EndGPUCopyPass(copyPass);
        }
        SDL_SubmitGPUCommandBuffer(cmdBuf);
    }

    SDL_ReleaseGPUTransferBuffer(device, transferBuf);
}

void SDLGPUImage::initAsync() {
    if(m_texture != nullptr) {
        this->setAsyncReady(true);
        return;
    }

    if(!this->bCreatedImage) {
        logIfCV(debug_rm, "Resource Manager: Loading {:s}", this->sFilePath);
        this->setAsyncReady(loadRawImage());
    } else {
        this->setAsyncReady(true);
    }
}

void SDLGPUImage::destroy() {
    if(!this->bKeepInSystemMemory) {
        auto *device = static_cast<SDLGPUInterface *>(g.get())->getDevice();

        if(m_texture) {
            SDL_ReleaseGPUTexture(device, m_texture);
            m_texture = nullptr;
        }

        this->rawImage.clear();
    }
}

void SDLGPUImage::bind(unsigned int /*textureUnit*/) const {
    if(!this->isReady()) return;

    auto *gpu = static_cast<SDLGPUInterface *>(g.get());

    // backup current
    m_prevTexture = gpu->getBoundTexture();
    m_prevSampler = gpu->getBoundSampler();

    // bind
    gpu->setBoundTexture(m_texture);
    gpu->setBoundSampler(m_sampler);
    gpu->setTexturing(true);
}

void SDLGPUImage::unbind() const {
    if(!this->isReady()) return;

    auto *gpu = static_cast<SDLGPUInterface *>(g.get());
    gpu->setBoundTexture(m_prevTexture);
    gpu->setBoundSampler(m_prevSampler);
}

void SDLGPUImage::setFilterMode(TextureFilterMode newFilterMode) {
    Image::setFilterMode(newFilterMode);
    if(!this->isReady()) return;
    createOrUpdateSampler();
}

void SDLGPUImage::setWrapMode(TextureWrapMode newWrapMode) {
    Image::setWrapMode(newWrapMode);
    if(!this->isReady()) return;
    createOrUpdateSampler();
}

void SDLGPUImage::createOrUpdateSampler() {
    auto *device = static_cast<SDLGPUInterface *>(g.get())->getDevice();

    if(m_sampler) {
        SDL_ReleaseGPUSampler(device, m_sampler);
        m_sampler = nullptr;
    }

    SDL_GPUSamplerCreateInfo samplerInfo{};

    switch(this->filterMode) {
        case TextureFilterMode::NONE:
            samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
            samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
            samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            break;
        case TextureFilterMode::LINEAR:
        case TextureFilterMode::MIPMAP:
            samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
            samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
            samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            break;
    }

    switch(this->wrapMode) {
        case TextureWrapMode::CLAMP:
            samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            break;
        case TextureWrapMode::REPEAT:
            samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            break;
    }

    samplerInfo.max_lod = this->bMipmapped ? 1000.0f : 0.0f;

    m_sampler = SDL_CreateGPUSampler(device, &samplerInfo);
}

#endif
