// Copyright (c) 2025-2026, WH, All rights reserved.
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Image.h"

using ThumbIdentifier = std::pair<i32, std::string>;
namespace std {
template <>
struct hash<ThumbIdentifier> {
    size_t operator()(const ThumbIdentifier& id_folder) const noexcept { return hash<std::string>()(id_folder.second); }
};
}  // namespace std

class ThumbnailManager {
    NOCOPY_NOMOVE(ThumbnailManager)
   public:
    ThumbnailManager() { this->load_queue.reserve(128); };
    virtual ~ThumbnailManager() { this->clear(); }

    // this is run during Osu::update(), while not in unpaused gameplay
    void update();

    // call this when you want to have some images ready soon
    // e.g. called by UIAvatar ctor to add new user id/folder avatar pairs to the loading queue (and tracking)
    void request_image(const ThumbIdentifier& id_folder);

    // call this when you no longer care about some image you requested
    // e.g. called ~UIAvatar dtor (removes it from pending queue, to not load/download images we don't need)
    void discard_image(const ThumbIdentifier& id_folder);

    // may return null if image is still loading
    [[nodiscard]] Image* try_get_image(const ThumbIdentifier& id_folder);

   private:
    // only keep this many thumbnail Image resources loaded in VRAM at once
    static constexpr size_t MAX_LOADED_IMAGES{256};

    struct ThumbEntry {
        std::string file_path;
        Image* image{nullptr};  // null if not loaded in memory
        double last_access_time{0.0};
    };

    void prune_oldest_entries();
    bool download_image(const ThumbIdentifier& id_folder);
    void load_image(ThumbEntry& entry);
    void clear();

    // all ThumbEntries added through request_image remain alive forever, but the actual Image resource
    // it references will be unloaded (by priority of access time) to keep VRAM/RAM usage sustainable
    std::unordered_map<ThumbIdentifier, ThumbEntry> images;
    std::vector<ThumbIdentifier> load_queue;
    std::unordered_map<ThumbIdentifier, std::atomic<u32>> image_refcount;
    std::unordered_set<ThumbIdentifier> id_blacklist;
    std::vector<u8> temp_img_download_data;  // if it has something in it, we just downloaded something

    size_t last_checked_queue_element{0};

   protected:
    std::string_view url_format{"{:s}a.{}/{:d}"};
};
