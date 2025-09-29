//========== Copyright (c) 2012, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		image wrapper
//
// $NoKeywords: $img
//===============================================================================//

#pragma once
#ifndef IMAGE_H
#define IMAGE_H

#include "Graphics.h"
#include "Resource.h"
#include "TextureAtlas.h"

class Image : public Resource {
   public:
    static void saveToImage(const u8 *data, i32 width, i32 height, u8 channels, std::string filepath);

    enum class TYPE : uint8_t { TYPE_RGBA, TYPE_PNG, TYPE_JPG };

   public:
    Image(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    Image(i32 width, i32 height, bool mipmapped = false, bool keepInSystemMemory = false);

    virtual void bind(unsigned int textureUnit = 0) const = 0;
    virtual void unbind() const = 0;

    virtual inline void setFilterMode(Graphics::FILTER_MODE filterMode) { this->filterMode = filterMode; };
    virtual inline void setWrapMode(Graphics::WRAP_MODE wrapMode) { this->wrapMode = wrapMode; };

    void setPixel(i32 x, i32 y, Color color);
    void setPixels(const u8 *data, u64 size, TYPE type);
    void setPixels(const std::vector<u8> &pixels);

    [[nodiscard]] inline bool failedLoad() const { return this->bLoadError; }
    [[nodiscard]] Color getPixel(i32 x, i32 y) const;

    [[nodiscard]] inline Image::TYPE getType() const { return this->type; }
    [[nodiscard]] inline i32 getWidth() const { return this->iWidth; }
    [[nodiscard]] inline i32 getHeight() const { return this->iHeight; }
    [[nodiscard]] inline vec2 getSize() const {
        return vec2{static_cast<float>(this->iWidth), static_cast<float>(this->iHeight)};
    }

    [[nodiscard]] constexpr bool hasAlphaChannel() const { return true; }

    // type inspection
    [[nodiscard]] Type getResType() const final { return IMAGE; }

    Image *asImage() final { return this; }
    [[nodiscard]] const Image *asImage() const final { return this; }

    // all images are converted to RGBA
    static constexpr const u8 NUM_CHANNELS{4};

   protected:
    void init() override = 0;
    void initAsync() override = 0;
    void destroy() override = 0;

    bool loadRawImage();

    std::vector<u8> rawImage;

    i32 iWidth;
    i32 iHeight;

    Graphics::WRAP_MODE wrapMode;
    Image::TYPE type;
    Graphics::FILTER_MODE filterMode;

    bool bMipmapped;
    bool bCreatedImage;
    bool bKeepInSystemMemory;
    bool bLoadError{false};

   private:
    [[nodiscard]] bool isCompletelyTransparent() const;
    static bool canHaveTransparency(const u8 *data, u64 size);

    static bool decodePNGFromMemory(const u8 *data, u64 size, std::vector<u8> &outData, i32 &outWidth, i32 &outHeight);
};

#endif
