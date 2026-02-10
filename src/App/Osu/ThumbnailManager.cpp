// Copyright (c) 2025-26, WH, All rights reserved.

#include "ThumbnailManager.h"
#include "AsyncIOHandler.h"

#include "Downloader.h"
#include "ResourceManager.h"
#include "Engine.h"
#include "OsuConVars.h"
#include "File.h"
#include "Logging.h"
#include "Timing.h"

#include <sys/stat.h>

class Osu;
extern Osu* osu;

Image* ThumbnailManager::try_get_image(const ThumbIdentifier& identifier) {
    auto it = this->images.find(identifier);
    if(it == this->images.end()) {
        return nullptr;
    }

    ThumbEntry& entry = it->second;
    entry.last_access_time = engine->getTime();

    // lazy load if not in memory (won't block)
    if(!entry.image) {
        this->load_image(entry);
    }

    // return only if ready (async loading complete)
    return (entry.image && entry.image->isReady()) ? entry.image : nullptr;
}

// this is run during Osu::update(), while not in unpaused gameplay
void ThumbnailManager::update() {
    const uSz cur_load_queue_size = this->load_queue.size();

    // nothing to do
    if(cur_load_queue_size == 0) {
        return;
    }

    // remove oldest entries if we have too many loaded
    this->prune_oldest_entries();

    // process 4 elements at a time from the download queue
    // we might not drain it fully due to only checking download progress once,
    // but we'll check again next update
    static constexpr const uSz ELEMS_TO_CHECK{4};

    this->last_checked_queue_element %= cur_load_queue_size;  // wrap at ends

    for(uSz i = this->last_checked_queue_element, num_checked = 0;
        num_checked < ELEMS_TO_CHECK && i < this->load_queue.size();
        ++num_checked, ++i, ++this->last_checked_queue_element) {
        auto identifier = this->load_queue[i];

        bool exists_on_disk = false;
        struct stat64 attr;
        if(File::stat_c(identifier.save_path.c_str(), &attr) == 0) {
            time_t now = time(nullptr);
            struct tm expiration_date;
            localtime_x(&attr.st_mtime, &expiration_date);
            expiration_date.tm_mday += 7;
            if(now <= mktime(&expiration_date)) {
                exists_on_disk = true;
            }
        }

        // if we have the file or the download just finished, create the entry
        // but only actually load it when it's needed (in get_image)
        bool newly_downloaded = false;
        if(exists_on_disk) {
            this->images[identifier] = {.file_path = identifier.save_path, .image = nullptr, .last_access_time = 0.0};
        } else if((newly_downloaded = this->download_image(identifier))) {
            // write async
            io->write(identifier.save_path, std::move(this->temp_img_download_data),
                      [&images = this->images, key = identifier](bool success) -> void {
                          if(!osu) return;  // do not run callback if osu has shut down
                          if(success) {
                              images[key] = {.file_path = key.save_path, .image = nullptr, .last_access_time = 0.0};
                          }
                      });
        }

        if(exists_on_disk || newly_downloaded) {
            this->load_queue.erase(this->load_queue.begin() + (sSz)i);  // remove it from the queue
        }
    }
}

void ThumbnailManager::request_image(const ThumbIdentifier& identifier) {
    const bool debug = cv::debug_thumbs.getBool();

    // increment refcount even if we didn't add to load queue
    const u32 current_refcount = this->image_refcount[identifier].fetch_add(1, std::memory_order_relaxed) + 1;
    logIf(debug, "trying to add {} to load queue, current refcount: {}", identifier.id, current_refcount);

    {
        bool already_added = false;
        if(current_refcount > 1 || this->id_blacklist.contains(identifier) ||
           (already_added = this->images.contains(identifier))) {
            logIf(debug, "not adding {} to load queue, {}", identifier.id,
                  current_refcount > 1 ? "refcount > 1" : (already_added ? "already have it" : "blacklisted"));
            return;
        }
    }

    if(resourceManager->getImage(identifier.save_path)) {
        // shouldn't happen...
        logIf(debug, "{} already tracked by ResourceManager, not adding", identifier.save_path);
        return;
    }

    // avoid duplicates in queue
    if(!std::ranges::contains(this->load_queue, identifier)) {
        logIf(debug, "added {} to load queue", identifier.id);
        this->load_queue.push_back(identifier);
    }
}

void ThumbnailManager::discard_image(const ThumbIdentifier& identifier) {
    const u32 current_refcount = this->image_refcount[identifier].fetch_sub(1, std::memory_order_acq_rel) - 1;
    logIfCV(debug_thumbs, "current refcount for {} is {}", identifier.id, current_refcount);

    if(current_refcount == 0) {
        // dequeue if it's waiting to be loaded, that's all
        if(std::erase(this->load_queue, identifier) > 0) {
            logIfCV(debug_thumbs, "removed {} from load queue", identifier.id);
        }
    }
}

void ThumbnailManager::load_image(ThumbEntry& entry) {
    if(entry.image || entry.file_path.empty()) {
        return;
    }

    resourceManager->requestNextLoadAsync();
    // the path *is* the resource name
    entry.image = resourceManager->loadImageAbs(entry.file_path, entry.file_path);
}

void ThumbnailManager::prune_oldest_entries() {
    // don't even do anything if we're not close to the limit (incl. unloaded)
    if(this->images.size() <= (uSz)(MAX_LOADED_IMAGES * (7.f / 8.f))) return;

    // collect all loaded entries
    std::vector<Hash::flat::map<ThumbIdentifier, ThumbEntry>::iterator> loaded_entries;

    for(auto it = this->images.begin(); it != this->images.end(); ++it) {
        if(it->second.image && it->second.image->isReady()) {
            loaded_entries.push_back(it);
        }
    }

    if(loaded_entries.size() <= MAX_LOADED_IMAGES) {
        return;
    }

    std::ranges::sort(loaded_entries, [](const auto& a, const auto& b) {
        return a->second.last_access_time < b->second.last_access_time;
    });

    // unload oldest images (a bit more, to not constantly be unloading images for each new image added after we hit the limit once)
    uSz to_unload = std::clamp<uSz>((uSz)(MAX_LOADED_IMAGES / 4.f), 0, loaded_entries.size() / 2);
    for(uSz i = 0; i < to_unload; ++i) {
        logIfCV(debug_thumbs, "unloading {} from memory due to age", loaded_entries[i]->second.file_path);
        resourceManager->destroyResource(loaded_entries[i]->second.image);
        loaded_entries[i]->second.image = nullptr;
    }
}

bool ThumbnailManager::download_image(const ThumbIdentifier& identifier) {
    const std::string_view scheme = cv::use_https.getBool() ? "https://"sv : "http://"sv;
    const std::string url = fmt::format("{}{}", scheme, identifier.download_url);
    float progress = -1.f;
    int response_code;
    // TODO: constantly requesting the full download is a bad API, should be a way to just check if it's already downloading
    // TODO: only download a single (response_code == 404) result and share it
    Downloader::download(url, &progress, this->temp_img_download_data, &response_code);
    if(progress == -1.f) this->id_blacklist.insert(identifier);
    if(progress == 1.f && response_code != 200) this->id_blacklist.insert(identifier);

    return (progress == 1.f && !this->temp_img_download_data.empty());
}

void ThumbnailManager::clear() {
    for(auto& [identifier, entry] : this->images) {
        if(entry.image) {
            resourceManager->destroyResource(entry.image);
        }
    }

    this->images.clear();
    this->load_queue.clear();
    this->id_blacklist.clear();
}
