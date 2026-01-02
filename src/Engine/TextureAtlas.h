#pragma once
// Copyright (c) 2017, PG, All rights reserved.
#ifndef TEXTUREATLAS_H
#define TEXTUREATLAS_H

#include "Resource.h"
#include <vector>
#include <memory>

struct Color;

class Image;

class TextureAtlas final : public Resource {
    NOCOPY_NOMOVE(TextureAtlas)
   public:
    // should basically never need to be changed
    static constexpr const int ATLAS_PADDING{1};

    struct PackRect {
        int x, y, width, height;
        int id;  // user-defined identifier for tracking
    };

    TextureAtlas(int width = 512, int height = 512, bool filtering = false);
    ~TextureAtlas() override;

    // place pixels at specific coordinates (for use after packing)
    void putAt(int x, int y, int width, int height, bool flipHorizontal, bool flipVertical, Color *pixels);
    // set image region to black
    inline void clearRegion(int x, int y, int width, int height, bool flipHorizontal, bool flipVertical) {
        return putAt(x, y, width, height, flipHorizontal, flipVertical, nullptr);
    }

    // advanced skyline packing for efficient atlas utilization
    bool packRects(std::vector<PackRect> &rects);

    // calculate optimal atlas size for given rectangles
    static size_t calculateOptimalSize(const std::vector<PackRect> &rects, float targetOccupancy = 0.75f,
                                       size_t minSize = 256, size_t maxSize = 4096);

    [[nodiscard]] inline int getWidth() const { return this->iWidth; }
    [[nodiscard]] inline int getHeight() const { return this->iHeight; }
    [[nodiscard]] inline const std::unique_ptr<Image> &getAtlasImage() const { return this->atlasImage; }

    TextureAtlas *asTextureAtlas() final { return this; }
    [[nodiscard]] const TextureAtlas *asTextureAtlas() const final { return this; }

   private:
    struct Skyline {
        int x, y, width;
    };

    void init() override;
    void initAsync() override;
    void destroy() override;

    std::unique_ptr<Image> atlasImage;

    int iWidth;
    int iHeight;

    bool bFiltered;
};

#endif
