// Copyright (c) 2025, WH, All rights reserved.
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "AvatarManager.h"

#include "Downloader.h"
#include "Bancho.h"
#include "ResourceManager.h"
#include "Engine.h"
#include "ConVar.h"
#include "File.h"
#include "Logging.h"

Image* AvatarManager::get_avatar(const std::pair<i32, std::string>& id_folder) {
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
    // nothing to do
    if(this->load_queue.empty()) {
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

    // process download queue (we might not drain it fully due to only checking download progress once,
    // but we'll check again next update)
    for(int i = 0; i < this->load_queue.size(); i++) {
        const auto& id_folder = this->load_queue.front();

        // we don't have to do an "expired" check if we just downloaded it
        bool exists_on_disk = this->newly_downloaded.contains(id_folder);
        if(!exists_on_disk) {
            struct stat attr;
            if(stat(id_folder.second.c_str(), &attr) == 0) {
                time_t now = time(nullptr);
                struct tm expiration_date = *localtime(&attr.st_mtime);
                expiration_date.tm_mday += 7;
                if(now <= mktime(&expiration_date)) {
                    exists_on_disk = true;
                }
            }
        }

        // if we have the file or the download just finished, create the entry
        // but only actually load it when it's needed (in get_avatar)
        if(exists_on_disk || this->download_avatar(id_folder)) {
            this->avatars[id_folder] = {
                .file_path = id_folder.second, .image = nullptr, .last_access_time = 0.0, .is_downloading = false};

            this->load_queue.pop_front();  // remove it from the queue
        }
    }
}

void AvatarManager::add_avatar(const std::pair<i32, std::string>& id_folder) {
    if(this->id_blacklist.contains(id_folder) || this->avatars.contains(id_folder)) {
        return;
    }

    if(resourceManager->getImage(id_folder.second)) {
        // shouldn't happen...
        debugLog("{} already tracked by ResourceManager, not adding", id_folder.second);
        return;
    }

    // avoid duplicates in queue
    if(std::ranges::find(this->load_queue, id_folder) == this->load_queue.end()) {
        this->load_queue.push_back(id_folder);
    }
}

void AvatarManager::remove_avatar(const std::pair<i32, std::string>& id_folder) {
    // dequeue if it's waiting to be loaded, that's all
    auto it = std::ranges::find(this->load_queue, id_folder);
    if(it != this->load_queue.end()) {
        this->load_queue.erase(it);
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
    // collect all loaded entries
    std::vector<std::map<std::pair<i32, std::string>, AvatarEntry>::iterator> loaded_entries;

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
    size_t to_unload = std::clamp<size_t>((size_t)(MAX_LOADED_AVATARS / 4.f), 0, loaded_entries.size() / 2);
    for(size_t i = 0; i < to_unload; ++i) {
        resourceManager->destroyResource(loaded_entries[i]->second.image);
        loaded_entries[i]->second.image = nullptr;
    }
}

bool AvatarManager::download_avatar(const std::pair<i32, std::string>& id_folder) {
    float progress = -1.f;
    std::vector<u8> img_data;
    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto img_url = fmt::format("{:s}a.{}/{:d}", scheme, BanchoState::endpoint, id_folder.first);
    int response_code;
    // TODO: constantly requesting the full download is a bad API, should be a way to just check if it's already downloading
    Downloader::download(img_url.c_str(), &progress, img_data, &response_code);
    if(progress == -1.f) this->id_blacklist.insert(id_folder);
    if(img_data.empty()) return false;

    File output_file(id_folder.second, File::TYPE::WRITE);
    if(output_file.canWrite()) {
        output_file.write(img_data.data(), img_data.size());
        this->newly_downloaded.insert(id_folder);
    }

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
    this->newly_downloaded.clear();
}
