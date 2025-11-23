// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include <map>
#include <set>
#include <utility>

#include "Image.h"

using AvatarIdentifier = std::pair<i32, std::string>;
namespace std {
template <>
struct hash<AvatarIdentifier> {
    size_t operator()(const AvatarIdentifier& id_folder) const noexcept {
        return hash<std::string>()(id_folder.second);
    }
};
}  // namespace std

class AvatarManager final {
    NOCOPY_NOMOVE(AvatarManager)

   public:
    AvatarManager() = default;
    ~AvatarManager() { this->clear(); }

    // this is run during Osu::update(), while not in unpaused gameplay
    void update();

    // called by UIAvatar ctor to add new user id/folder avatar pairs to the loading queue (and tracking)
    void add_avatar(const AvatarIdentifier& id_folder);

    // called by ~UIAvatar dtor (removes it from pending queue, to not load/download images we don't need)
    void remove_avatar(const AvatarIdentifier& id_folder);

    // may return null if avatar is still loading
    [[nodiscard]] Image* get_avatar(const AvatarIdentifier& id_folder);

   private:
    // only keep this many avatar Image resources loaded in VRAM at once
    static constexpr size_t MAX_LOADED_AVATARS{192};

    struct AvatarEntry {
        std::string file_path;
        Image* image{nullptr};  // null if not loaded in memory
        double last_access_time{0.0};
    };

    void prune_oldest_avatars();
    bool download_avatar(const AvatarIdentifier& id_folder);
    void load_avatar_image(AvatarEntry& entry);
    void clear();

    // all AvatarEntries added through add_avatar remain alive forever, but the actual Image resource
    // it references will be unloaded (by priority of access time) to keep VRAM/RAM usage sustainable
    std::map<AvatarIdentifier, AvatarEntry> avatars;
    std::deque<AvatarIdentifier> load_queue;
    std::unordered_map<AvatarIdentifier, std::atomic<u32>> avatar_refcount;
    std::set<AvatarIdentifier> id_blacklist;
    std::vector<u8> temp_img_download_data;  // if it has something in it, we just downloaded something
};
