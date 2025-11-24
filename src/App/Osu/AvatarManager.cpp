// Copyright (c) 2025, WH, All rights reserved.

#include "AvatarManager.h"
#include "AsyncIOHandler.h"

#include "Downloader.h"
#include "Bancho.h"
#include "ResourceManager.h"
#include "Engine.h"
#include "ConVar.h"
#include "File.h"
#include "Logging.h"
#include "Timing.h"

#include <sys/stat.h>

class Osu;
extern Osu* osu;

Image* AvatarManager::get_avatar(const AvatarIdentifier& id_folder) {
    auto it = this->avatars.find(id_folder);
    if(it == this->avatars.end()) {
        return nullptr;
    }

    AvatarEntry& entry = it->second;
    entry.last_access_time = engine->getTime();

    // lazy load if not in memory (won't block)
    if(!entry.image) {
        this->load_avatar_image(entry);
    }

    // return only if ready (async loading complete)
    return (entry.image && entry.image->isReady()) ? entry.image : nullptr;
}

// this is run during Osu::update(), while not in unpaused gameplay
void AvatarManager::update() {
    const uSz cur_load_queue_size = this->load_queue.size();

    // nothing to do
    if(cur_load_queue_size == 0) {
        return;
    }

    if(!BanchoState::is_online()) {
        // TODO: offline/local avatars?
        // don't clear what we already have in memory, in case we go back online on the same server
        // but also don't update (downloading online avatars while logged out is just not something we need currently)
        return;
    }

    // remove oldest avatars if we have too many loaded
    this->prune_oldest_avatars();

    // process 4 elements at a time from the download queue
    // we might not drain it fully due to only checking download progress once,
    // but we'll check again next update
    static constexpr const uSz ELEMS_TO_CHECK{4};

    this->last_checked_queue_element %= cur_load_queue_size;  // wrap at ends

    for(uSz i = this->last_checked_queue_element, num_checked = 0;
        num_checked < ELEMS_TO_CHECK && i < this->load_queue.size();
        ++num_checked, ++i, ++this->last_checked_queue_element) {
        auto id_folder = this->load_queue[i];

        bool exists_on_disk = false;
        struct stat attr;
        if(stat(id_folder.second.c_str(), &attr) == 0) {
            time_t now = time(nullptr);
            struct tm expiration_date;
            localtime_x(&attr.st_mtime, &expiration_date);
            expiration_date.tm_mday += 7;
            if(now <= mktime(&expiration_date)) {
                exists_on_disk = true;
            }
        }

        // if we have the file or the download just finished, create the entry
        // but only actually load it when it's needed (in get_avatar)
        bool newly_downloaded = false;
        if(exists_on_disk) {
            this->avatars[id_folder] = {.file_path = id_folder.second, .image = nullptr, .last_access_time = 0.0};
        } else if((newly_downloaded = this->download_avatar(id_folder))) {
            // write async
            io->write(id_folder.second, std::move(this->temp_img_download_data),
                      [&avatars = this->avatars, pair = id_folder](bool success) -> void {
                          if(!osu) return;  // do not run callback if osu has shut down
                          if(success) {
                              avatars[pair] = {.file_path = pair.second, .image = nullptr, .last_access_time = 0.0};
                          }
                      });
        }

        if(exists_on_disk || newly_downloaded) {
            this->load_queue.erase(this->load_queue.begin() + (sSz)i);  // remove it from the queue
        }
    }
}

void AvatarManager::add_avatar(const AvatarIdentifier& id_folder) {
    const bool debug = cv::debug_avatars.getBool();

    // increment refcount even if we didn't add to load queue
    const u32 current_refcount = this->avatar_refcount[id_folder].fetch_add(1, std::memory_order_relaxed) + 1;
    logIf(debug, "trying to add {} to load queue, current refcount: {}", id_folder.first, current_refcount);

    {
        bool already_added = false;
        if(current_refcount > 1 || this->id_blacklist.contains(id_folder) ||
           (already_added = this->avatars.contains(id_folder))) {
            logIf(
                debug, "not adding {} to load queue, {}", id_folder.first,
                current_refcount > 1 ? "refcount > 1" : (already_added ? "already have it in avatars" : "blacklisted"));
            return;
        }
    }

    if(resourceManager->getImage(id_folder.second)) {
        // shouldn't happen...
        logIf(debug, "{} already tracked by ResourceManager, not adding", id_folder.second);
        return;
    }

    // avoid duplicates in queue
    if(!std::ranges::contains(this->load_queue, id_folder)) {
        logIf(debug, "added {} to load queue", id_folder.first);
        this->load_queue.push_back(id_folder);
    }
}

void AvatarManager::remove_avatar(const AvatarIdentifier& id_folder) {
    const u32 current_refcount = this->avatar_refcount[id_folder].fetch_sub(1, std::memory_order_acq_rel) - 1;
    logIfCV(debug_avatars, "current refcount for {} is {}", current_refcount, id_folder.first);

    if(current_refcount == 0) {
        // dequeue if it's waiting to be loaded, that's all
        if(std::erase_if(this->load_queue, [&id_folder](const auto& pair) { return pair == id_folder; }) > 0) {
            logIfCV(debug_avatars, "removed {} from load queue", id_folder.first);
        }
    }
}

void AvatarManager::load_avatar_image(AvatarEntry& entry) {
    if(entry.image || entry.file_path.empty()) {
        return;
    }

    resourceManager->requestNextLoadAsync();
    // the path *is* the resource name
    entry.image = resourceManager->loadImageAbs(entry.file_path, entry.file_path);
}

void AvatarManager::prune_oldest_avatars() {
    // don't even do anything if we're not close to the limit (incl. unloaded)
    if(this->avatars.size() <= (uSz)(MAX_LOADED_AVATARS * (7.f / 8.f))) return;

    // collect all loaded entries
    std::vector<std::unordered_map<AvatarIdentifier, AvatarEntry>::iterator> loaded_entries;

    for(auto it = this->avatars.begin(); it != this->avatars.end(); ++it) {
        if(it->second.image && it->second.image->isReady()) {
            loaded_entries.push_back(it);
        }
    }

    if(loaded_entries.size() <= MAX_LOADED_AVATARS) {
        return;
    }

    std::ranges::sort(loaded_entries, [](const auto& a, const auto& b) {
        return a->second.last_access_time < b->second.last_access_time;
    });

    // unload oldest images (a bit more, to not constantly be unloading images for each new image added after we hit the limit once)
    uSz to_unload = std::clamp<uSz>((uSz)(MAX_LOADED_AVATARS / 4.f), 0, loaded_entries.size() / 2);
    for(uSz i = 0; i < to_unload; ++i) {
        logIfCV(debug_avatars, "unloading {} from memory due to age", loaded_entries[i]->second.file_path);
        resourceManager->destroyResource(loaded_entries[i]->second.image);
        loaded_entries[i]->second.image = nullptr;
    }
}

bool AvatarManager::download_avatar(const AvatarIdentifier& id_folder) {
    float progress = -1.f;
    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto img_url = fmt::format("{:s}a.{}/{:d}", scheme, BanchoState::endpoint, id_folder.first);
    int response_code;
    // TODO: constantly requesting the full download is a bad API, should be a way to just check if it's already downloading
    Downloader::download(img_url.c_str(), &progress, this->temp_img_download_data, &response_code);
    if(progress == -1.f) this->id_blacklist.insert(id_folder);
    if(this->temp_img_download_data.empty()) return false;

    // NOTE: We return true even if progress is -1. Because we still get avatars from a 404!
    // TODO: only download a single 404 (blacklisted) avatar and share it
    return true;
}

void AvatarManager::clear() {
    for(auto& [id_folder, entry] : this->avatars) {
        if(entry.image) {
            resourceManager->destroyResource(entry.image);
        }
    }

    this->avatars.clear();
    this->load_queue.clear();
    this->id_blacklist.clear();
}
