// Copyright (c) 2020, PG, All rights reserved.
#ifndef OSUBACKGROUNDIMAGEHANDLER_H
#define OSUBACKGROUNDIMAGEHANDLER_H

#include "cbase.h"

class Image;

class DatabaseBeatmap;
class DatabaseBeatmapBackgroundImagePathLoader;

class BackgroundImageHandler {
   public:
    BackgroundImageHandler();
    ~BackgroundImageHandler();

    void update(bool allowEviction);

    void scheduleFreezeCache() { this->bFrozen = true; }

    Image *getLoadBackgroundImage(const DatabaseBeatmap *beatmap, bool loadImmediately = false);

   private:
    struct ENTRY {
        std::string osuFilePath;
        std::string folder;
        std::string backgroundImageFileName;

        DatabaseBeatmapBackgroundImagePathLoader *backgroundImagePathLoader;
        Image *image;

        unsigned long evictionTimeFrameCount;

        float loadingTime;
        float evictionTime;

        bool isLoadScheduled;
        bool wasUsedLastFrame;
    };

    void handleLoadPathForEntry(ENTRY &entry);
    void handleLoadImageForEntry(ENTRY &entry);

    std::vector<ENTRY> cache;
    bool bFrozen;
};

#endif
