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
    const Image *getLoadBackgroundImage(const DatabaseBeatmap *beatmap, bool load_immediately = false);

    inline void scheduleFreezeCache() { this->frozen = true; }

   private:
    class MapBGImagePathLoader;

    struct ENTRY;

    [[nodiscard]] u32 getMaxEvictions() const;
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
    inline void enableToggleCB(f32 new_value) {
        bool enabled = !!static_cast<int>(new_value);
        this->disabled = !enabled;
    }

    sv_unordered_map<ENTRY> cache;
    std::string last_requested_entry;

    u32 max_cache_size;
    u32 eviction_delay_frames;
    f32 image_loading_delay;

    bool frozen{false};
    bool disabled{false};
};

#endif
