// Copyright (c) 2017, PG, All rights reserved.
#include "TextureAtlas.h"

#include <algorithm>

#include "Engine.h"
#include "ResourceManager.h"
#include "Logging.h"
#include "Image.h"

TextureAtlas::TextureAtlas(int width, int height, bool filtering) : Resource() {
    this->iWidth = width;
    this->iHeight = height;

    resourceManager->requestNextLoadUnmanaged();
    this->atlasImage.reset(resourceManager->createImage(this->iWidth, this->iHeight, false,
                                                        true /* keep in system memory, for faster reloads */));
    if(!filtering) {
        this->atlasImage->setFilterMode(TextureFilterMode::FILTER_MODE_NONE);
    }  // else linear filtering is default
}

TextureAtlas::~TextureAtlas() { this->destroy(); }

void TextureAtlas::init() {
    resourceManager->loadResource(this->atlasImage.get());

    this->setReady(true);
}

void TextureAtlas::initAsync() { this->setAsyncReady(true); }

void TextureAtlas::destroy() { this->atlasImage.reset(); }

void TextureAtlas::putAt(int x, int y, int width, int height, bool flipHorizontal, bool flipVertical, Color *pixels) {
    if(width < 1 || height < 1 || this->atlasImage == nullptr) return;

    if(x + width > this->iWidth || y + height > this->iHeight || x < 0 || y < 0) {
        debugLog("TextureAtlas::putAt( {}, {}, {}, {} ) WARNING: Out of bounds! Atlas size: {}x{}", x, y, width, height,
                 this->iWidth, this->iHeight);
        return;
    }

// if input pixels was null, we're making that region black
#define PIXEL_COLOR_(x__, y__, width__) (pixels ? pixels[/*sourceIdx*/ (y__) * (width__) + (x__)] : (Color)0)

    // insert pixels at specified coordinates
    for(int j = 0; j < height; j++) {
        for(int i = 0; i < width; i++) {
            const int atlasX = x + i;
            const int atlasY = y + j;

            // bounds checking with debug info
            if(atlasX >= this->iWidth || atlasY >= this->iHeight) {
                debugLog("WARNING: Pixel placement out of bounds: atlas=({},{}) in {}x{}", atlasX, atlasY, this->iWidth,
                         this->iHeight);
                continue;
            }
            int actualX = (flipHorizontal ? width - i - 1 : i);
            int actualY = (flipVertical ? height - j - 1 : j);

            this->atlasImage->setPixel(atlasX, atlasY, PIXEL_COLOR_(actualX, actualY, width));
        }
    }

    // mirror border pixels for padding > 1
    if(unlikely(ATLAS_PADDING > 1)) {
        // left border
        for(int j = -1; j < height + 1; j++) {
            const int i = 0;
            int actualX = (flipHorizontal ? width - i - 1 : i);
            int actualY = std::clamp<int>((flipVertical ? height - j - 1 : j), 0, height - 1);

            if(x + i - 1 >= 0 && y + j >= 0 && y + j < this->iHeight)
                this->atlasImage->setPixel(x + i - 1, y + j, PIXEL_COLOR_(actualX, actualY, width));
        }
        // right border
        for(int j = -1; j < height + 1; j++) {
            const int i = width - 1;
            int actualX = (flipHorizontal ? width - i - 1 : i);
            int actualY = std::clamp<int>((flipVertical ? height - j - 1 : j), 0, height - 1);

            if(x + i + 1 < this->iWidth && y + j >= 0 && y + j < this->iHeight)
                this->atlasImage->setPixel(x + i + 1, y + j, PIXEL_COLOR_(actualX, actualY, width));
        }
        // top border
        for(int i = -1; i < width + 1; i++) {
            const int j = 0;
            int actualX = std::clamp<int>((flipHorizontal ? width - i - 1 : i), 0, width - 1);
            int actualY = (flipVertical ? height - j - 1 : j);

            if(x + i >= 0 && x + i < this->iWidth && y + j - 1 >= 0)
                this->atlasImage->setPixel(x + i, y + j - 1, PIXEL_COLOR_(actualX, actualY, width));
        }
        // bottom border
        for(int i = -1; i < width + 1; i++) {
            const int j = height - 1;
            int actualX = std::clamp<int>((flipHorizontal ? width - i - 1 : i), 0, width - 1);
            int actualY = (flipVertical ? height - j - 1 : j);

            if(x + i >= 0 && x + i < this->iWidth && y + j + 1 < this->iHeight)
                this->atlasImage->setPixel(x + i, y + j + 1, PIXEL_COLOR_(actualX, actualY, width));
        }
    }
#undef PIXEL_COLOR_
}

bool TextureAtlas::packRects(std::vector<PackRect> &rects) {
    if(rects.empty()) return true;

    // sort rectangles by height (tallest first) for better packing efficiency
    std::ranges::sort(rects, [](const PackRect &a, const PackRect &b) { return a.height > b.height; });

    // initialize skyline - start with single segment covering entire width
    std::vector<Skyline> skylines = {{.x = 0, .y = ATLAS_PADDING, .width = this->iWidth}};

    for(auto &rect : rects) {
        const int rectWidth = rect.width + ATLAS_PADDING;
        const int rectHeight = rect.height + ATLAS_PADDING;

        int bestHeight = this->iHeight;
        int bestIndex = -1;
        int bestX = this->iWidth;  // initialize to rightmost position for leftmost preference

        // find best position along skyline
        for(size_t i = 0; i < skylines.size(); ++i) {
            // check if rectangle fits horizontally at this skyline segment
            if(skylines[i].x + rectWidth > this->iWidth) continue;

            // find maximum height across all skyline segments this rect would span
            int maxY = skylines[i].y;
            int currentX = skylines[i].x;

            for(size_t j = i; j < skylines.size() && currentX < skylines[i].x + rectWidth; ++j) {
                maxY = std::max(maxY, skylines[j].y);
                currentX += skylines[j].width;
            }

            // select this position if it gives us the minimum height
            if(maxY + rectHeight < bestHeight) {
                bestHeight = maxY + rectHeight;
                bestIndex = static_cast<int>(i);
                bestX = skylines[i].x;
            }
        }

        if(bestIndex == -1 || bestHeight > this->iHeight) {
            debugLog("ERROR: Packing failed for rect id={}: bestIndex={}, bestHeight={}, atlasHeight={}", rect.id,
                     bestIndex, bestHeight, this->iHeight);
            return false;
        }

        // place the rectangle
        rect.x = bestX + ATLAS_PADDING;
        rect.y = bestHeight - rectHeight;

        // update skyline - remove segments covered by this rectangle and add new segment
        std::vector<Skyline> newSkylines;

        // copy segments before the placed rectangle
        for(auto &skyline : skylines) {
            if(skyline.x + skyline.width <= bestX)
                newSkylines.push_back(skyline);
            else if(skyline.x < bestX)  // partial segment before rectangle
                newSkylines.push_back({skyline.x, skyline.y, bestX - skyline.x});
            else
                break;
        }

        // add new segment for placed rectangle
        newSkylines.push_back({bestX, bestHeight, rectWidth});

        // add remaining segments after the placed rectangle
        for(auto &skyline : skylines) {
            if(skyline.x >= bestX + rectWidth) {
                newSkylines.push_back(skyline);
            } else if(skyline.x + skyline.width > bestX + rectWidth) {
                // partial segment after rectangle
                int newX = bestX + rectWidth;
                int newWidth = skyline.x + skyline.width - newX;
                newSkylines.push_back({newX, skyline.y, newWidth});
            }
        }

        skylines = std::move(newSkylines);

        // merge adjacent skylines with same height
        for(size_t i = 0; i < skylines.size() - 1; ++i) {
            if(skylines[i].y == skylines[i + 1].y && skylines[i].x + skylines[i].width == skylines[i + 1].x) {
                skylines[i].width += skylines[i + 1].width;
                skylines.erase(skylines.begin() + i + 1);
                --i;
            }
        }
    }

    return true;
}

size_t TextureAtlas::calculateOptimalSize(const std::vector<PackRect> &rects, float targetOccupancy, size_t minSize,
                                          size_t maxSize) {
    if(rects.empty()) return minSize;

    // calculate total area including padding
    size_t totalArea = 0;
    for(const auto &rect : rects) {
        totalArea += static_cast<size_t>((rect.width + ATLAS_PADDING) * (rect.height + ATLAS_PADDING));
    }

    // add 20% for packing inefficiency
    totalArea = static_cast<size_t>(totalArea * 1.2f);

    // find smallest power of 2 that can fit the rectangles with desired occupancy
    size_t size = minSize;
    while(size * size * targetOccupancy < totalArea && size < maxSize) {
        size *= 2;
    }

    return size;
}
