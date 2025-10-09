// Copyright (c) 2020, PG, All rights reserved.
#ifndef OSUBACKGROUNDIMAGEHANDLER_H
#define OSUBACKGROUNDIMAGEHANDLER_H

#include "noinclude.h"
#include "types.h"

#include <vector>
#include <string>

class Image;

class DatabaseBeatmap;
class DatabaseBeatmapBackgroundImagePathLoader;

class BackgroundImageHandler final {
    NOCOPY_NOMOVE(BackgroundImageHandler)
   public:
    BackgroundImageHandler();
    ~BackgroundImageHandler();

    void update(bool allowEviction);

    void scheduleFreezeCache() { this->bFrozen = true; }

    const Image *getLoadBackgroundImage(const DatabaseBeatmap *beatmap, bool loadImmediately = false);

   private:
    struct ENTRY {
        std::string osuFilePath;
        std::string folder;
        std::string backgroundImageFileName;

        DatabaseBeatmapBackgroundImagePathLoader *backgroundImagePathLoader;
        Image *image;

        u32 evictionTimeFrameCount;

        float loadingTime;
        float evictionTime;

        bool isLoadScheduled;
        bool wasUsedLastFrame;
    };

    [[nodiscard]] const Image *getImageOrSkinFallback(const Image *candidateLoaded) const;

    void handleLoadPathForEntry(ENTRY &entry);
    void handleLoadImageForEntry(ENTRY &entry);

    std::vector<ENTRY> cache;
    bool bFrozen;
};

#endif
