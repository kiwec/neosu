// Copyright (c) 2020, PG, All rights reserved.
#ifndef OSUBACKGROUNDIMAGEHANDLER_H
#define OSUBACKGROUNDIMAGEHANDLER_H

#include "noinclude.h"
#include "types.h"
#include "templates.h"

#include <cmath>
#include <algorithm>
#include <string>

class Image;

class DatabaseBeatmap;

class BGImageHandler final {
    NOCOPY_NOMOVE(BGImageHandler)
   public:
    BGImageHandler();
    ~BGImageHandler();

    void update(bool allowEviction);

    void scheduleFreezeCache() { this->frozen = true; }

    const Image *getLoadBackgroundImage(const DatabaseBeatmap *beatmap, bool load_immediately = false);

   private:
    class MapBGImagePathLoader;

    struct ENTRY {
        std::string folder;
        std::string bg_image_filename;

        MapBGImagePathLoader *bg_image_path_ldr;
        Image *image;

        u32 evict_framecnt;

        f32 loading_time;

        bool load_scheduled;
        bool used_last_frame;
    };

    [[nodiscard]] const Image *getImageOrSkinFallback(const Image *candidate_loaded) const;

    void handleLoadPathForEntry(const std::string &path, ENTRY &entry);
    void handleLoadImageForEntry(ENTRY &entry);

    // store convars as callbacks to avoid convar overhead
    inline void cacheSizeCB(f32 new_value) {
        u32 new_u32 = std::clamp<u32>(static_cast<u32>(std::round(new_value)), 0, 128);
        this->max_cache_size = new_u32;
    }
    inline void evictionDelayCB(f32 new_value) {
        u32 new_u32 = std::clamp<u32>(static_cast<u32>(std::round(new_value)), 0, 1024);
        this->eviction_delay_frames = new_u32;
    }
    inline void loadingDelayCB(f32 new_value) {
        f32 new_delay = std::clamp<f32>(new_value, 0.f, 2.f);
        this->image_loading_delay = new_delay;
    }

    sv_unordered_map<ENTRY> cache;

    u32 max_cache_size;
    u32 eviction_delay_frames;
    f32 image_loading_delay;

    bool frozen{false};
};

#endif
