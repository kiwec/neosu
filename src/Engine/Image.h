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

    virtual inline void setFilterMode(TextureFilterMode filterMode) { this->filterMode = filterMode; };
    virtual inline void setWrapMode(TextureWrapMode wrapMode) { this->wrapMode = wrapMode; };

    void setPixel(i32 x, i32 y, Color color);
    void setPixels(const std::vector<u8> &pixels);

    [[nodiscard]] inline bool failedLoad() const { return this->bLoadError.load(std::memory_order_acquire); }
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

    // holding actual pointer width/height separately, just in case
    struct SizedRGBABytes final {
        struct CFree {
            // stb_image_free is just a macro to free, anyways
            forceinline void operator()(void *p) const noexcept { free(p); }
        };
        SizedRGBABytes() = default;
        ~SizedRGBABytes() noexcept = default;

        // for taking ownership of some raw pointer (stb)
        explicit SizedRGBABytes(u8 *to_own, i32 width, i32 height) noexcept : size(width, height), bytes(to_own) {}

        explicit SizedRGBABytes(i32 width, i32 height) noexcept
            : size(width, height),
              bytes(static_cast<u8 *>(malloc(static_cast<u64>(width) * height * Image::NUM_CHANNELS))) {}
        explicit SizedRGBABytes(i32 width, i32 height, bool /*zero*/) noexcept
            : size(width, height),
              bytes(static_cast<u8 *>(calloc(static_cast<u64>(width) * height * Image::NUM_CHANNELS, sizeof(u8)))) {}

        SizedRGBABytes(const SizedRGBABytes &other) noexcept
            : size(other.size), bytes(other.bytes ? static_cast<u8 *>(malloc(other.getNumBytes())) : nullptr) {
            if(this->bytes) {
                memcpy(this->bytes.get(), other.bytes.get(), other.getNumBytes());
            }
        }
        SizedRGBABytes &operator=(const SizedRGBABytes &other) noexcept {
            if(this != &other) {
                this->size = other.size;
                this->bytes.reset(other.bytes ? static_cast<u8 *>(malloc(other.getNumBytes())) : nullptr);
                if(this->bytes) {
                    memcpy(this->bytes.get(), other.bytes.get(), other.getNumBytes());
                }
            }
            return *this;
        }
        SizedRGBABytes(SizedRGBABytes &&other) noexcept : size(other.size), bytes(std::move(other.bytes)) {}
        SizedRGBABytes &operator=(SizedRGBABytes &&other) noexcept {
            if(this != &other) {
                this->size = other.size;
                this->bytes = std::move(other.bytes);
            }
            return *this;
        }

        [[nodiscard]] constexpr forceinline u64 getNumBytes() const {
            return static_cast<u64>(this->size.x) * this->size.y * Image::NUM_CHANNELS;
        }
        [[nodiscard]] constexpr forceinline u64 getArea() const {
            return static_cast<u64>(this->size.x) * this->size.y;
        }
        [[nodiscard]] constexpr forceinline i32 getX() const { return this->size.x; }
        [[nodiscard]] constexpr forceinline i32 getY() const { return this->size.y; }
        [[nodiscard]] constexpr forceinline u8 *data() { return this->bytes.get(); }
        [[nodiscard]] constexpr forceinline const u8 *data() const { return this->bytes.get(); }
        [[nodiscard]] constexpr forceinline u8 &operator[](uSz i) {
            assert(this->bytes && i < this->getNumBytes());
            return *(this->bytes.get() + i);
        }
        [[nodiscard]] constexpr forceinline const u8 &operator[](uSz i) const {
            assert(this->bytes && i < this->getNumBytes());
            return this->bytes.get()[i];
        }

        void clear() {
            this->size = {};
            this->bytes = {};
        }

       private:
        ivec2 size{0, 0};
        std::unique_ptr<u8, CFree> bytes{nullptr};
    };

    SizedRGBABytes rawImage;

    [[nodiscard]] constexpr forceinline u64 totalBytes() const { return this->rawImage.getNumBytes(); }

    i32 iWidth;
    i32 iHeight;

    TextureWrapMode wrapMode;
    Image::TYPE type;
    TextureFilterMode filterMode;

    bool bMipmapped;
    bool bCreatedImage;
    bool bKeepInSystemMemory;
    std::atomic<bool> bLoadError{false};
    bool bLoadedImageEntirelyTransparent{false};

   private:
    [[nodiscard]] bool isRawImageCompletelyTransparent() const;
    static bool canHaveTransparency(const std::unique_ptr<u8[]> &data, u64 size);

    enum class DECODE_RESULT : u8 {
        SUCCESS,
        FAIL,
        INTERRUPTED,
    };

    DECODE_RESULT decodeJPEGFromMemory(const std::unique_ptr<u8[]> &inData, u64 size);
    DECODE_RESULT decodePNGFromMemory(const std::unique_ptr<u8[]> &inData, u64 size);
    DECODE_RESULT decodeSTBFromMemory(const std::unique_ptr<u8[]> &inData, u64 size);
};

#endif
