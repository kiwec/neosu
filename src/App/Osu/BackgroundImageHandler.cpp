#include "BackgroundImageHandler.h"

#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "ResourceManager.h"

BackgroundImageHandler::BackgroundImageHandler() { this->bFrozen = false; }

BackgroundImageHandler::~BackgroundImageHandler() {
    for(auto &i : this->cache) {
        resourceManager->destroyResource(i.backgroundImagePathLoader);
        resourceManager->destroyResource(i.image);
    }
    this->cache.clear();
}

void BackgroundImageHandler::update(bool allowEviction) {
    for(size_t i = 0; i < this->cache.size(); i++) {
        ENTRY &entry = this->cache[i];

        // NOTE: avoid load/unload jitter if framerate is below eviction delay
        const bool wasUsedLastFrame = entry.wasUsedLastFrame;
        entry.wasUsedLastFrame = false;

        // check and handle evictions
        if(!wasUsedLastFrame &&
           (engine->getTime() >= entry.evictionTime && engine->getFrameCount() >= entry.evictionTimeFrameCount)) {
            if(allowEviction) {
                if(!this->bFrozen && !engine->isMinimized()) {
                    if(entry.backgroundImagePathLoader != NULL) entry.backgroundImagePathLoader->interruptLoad();
                    if(entry.image != NULL) entry.image->interruptLoad();

                    resourceManager->destroyResource(entry.backgroundImagePathLoader);
                    resourceManager->destroyResource(entry.image);

                    this->cache.erase(this->cache.begin() + i);
                    i--;
                    continue;
                }
            } else {
                entry.evictionTime = engine->getTime() + cv::background_image_eviction_delay_seconds.getFloat();
                entry.evictionTimeFrameCount =
                    engine->getFrameCount() + (unsigned long)std::max(0, cv::background_image_eviction_delay_frames.getInt());
            }
        } else if(wasUsedLastFrame) {
            // check and handle scheduled loads
            if(entry.isLoadScheduled) {
                if(engine->getTime() >= entry.loadingTime) {
                    entry.isLoadScheduled = false;

                    if(entry.backgroundImageFileName.length() < 2) {
                        // if the backgroundImageFileName is not loaded, then we have to create a full
                        // DatabaseBeatmapBackgroundImagePathLoader
                        entry.image = NULL;
                        this->handleLoadPathForEntry(entry);
                    } else {
                        // if backgroundImageFileName is already loaded/valid, then we can directly load the image
                        entry.backgroundImagePathLoader = NULL;
                        this->handleLoadImageForEntry(entry);
                    }
                }
            } else {
                // no load scheduled (potential load-in-progress if it was necessary), handle backgroundImagePathLoader
                // loading finish
                if(entry.image == NULL && entry.backgroundImagePathLoader != NULL &&
                   entry.backgroundImagePathLoader->isReady()) {
                    if(entry.backgroundImagePathLoader->getLoadedBackgroundImageFileName().length() > 1) {
                        entry.backgroundImageFileName =
                            entry.backgroundImagePathLoader->getLoadedBackgroundImageFileName();
                        this->handleLoadImageForEntry(entry);
                    }

                    resourceManager->destroyResource(entry.backgroundImagePathLoader);
                    entry.backgroundImagePathLoader = NULL;
                }
            }
        }
    }

    // reset flags
    this->bFrozen = false;

    // DEBUG:
    // debugLog("m_cache.size() = {:d}\n", (int)this->cache.size());
}

void BackgroundImageHandler::handleLoadPathForEntry(ENTRY &entry) {
    entry.backgroundImagePathLoader = new DatabaseBeatmapBackgroundImagePathLoader(entry.osuFilePath);

    // start path load
    resourceManager->requestNextLoadAsync();
    resourceManager->loadResource(entry.backgroundImagePathLoader);
}

void BackgroundImageHandler::handleLoadImageForEntry(ENTRY &entry) {
    std::string fullBackgroundImageFilePath = entry.folder;
    fullBackgroundImageFilePath.append(entry.backgroundImageFileName);

    // start image load
    resourceManager->requestNextLoadAsync();
    resourceManager->requestNextLoadUnmanaged();
    entry.image = resourceManager->loadImageAbsUnnamed(fullBackgroundImageFilePath, true);
}

Image *BackgroundImageHandler::getLoadBackgroundImage(const DatabaseBeatmap *beatmap) {
    if(beatmap == NULL || !cv::load_beatmap_background_images.getBool() || !beatmap->draw_background) return NULL;

    // NOTE: no references to beatmap are kept anywhere (database can safely be deleted/reloaded without having to
    // notify the BackgroundImageHandler)

    const float newLoadingTime = engine->getTime() + cv::background_image_loading_delay.getFloat();
    const float newEvictionTime = engine->getTime() + cv::background_image_eviction_delay_seconds.getFloat();
    const unsigned long newEvictionTimeFrameCount =
        engine->getFrameCount() + (unsigned long)std::max(0, cv::background_image_eviction_delay_frames.getInt());

    // 1) if the path or image is already loaded, return image ref immediately (which may still be NULL) and keep track
    // of when it was last requested
    for(auto &i : this->cache) {
        ENTRY &entry = i;

        if(entry.osuFilePath == beatmap->getFilePath()) {
            entry.wasUsedLastFrame = true;
            entry.evictionTime = newEvictionTime;
            entry.evictionTimeFrameCount = newEvictionTimeFrameCount;

            // HACKHACK: to improve future loading speed, if we have already loaded the backgroundImageFileName, force
            // update the database backgroundImageFileName and fullBackgroundImageFilePath this is similar to how it
            // worked before the rework, but 100% safe(r) since we are not async
            if(i.image != NULL && i.backgroundImageFileName.length() > 1 &&
               beatmap->getBackgroundImageFileName().length() < 2) {
                const_cast<DatabaseBeatmap *>(beatmap)->sBackgroundImageFileName = i.backgroundImageFileName;
                const_cast<DatabaseBeatmap *>(beatmap)->sFullBackgroundImageFilePath = beatmap->getFolder();
                const_cast<DatabaseBeatmap *>(beatmap)->sFullBackgroundImageFilePath.append(i.backgroundImageFileName);
            }

            return entry.image;
        }
    }

    // 2) not found in cache, so create a new entry which will get handled in the next update
    {
        // try evicting stale not-yet-loaded-nor-started-loading entries on overflow
        const int maxCacheEntries = cv::background_image_cache_size.getInt();
        {
            if(this->cache.size() >= maxCacheEntries) {
                for(size_t i = 0; i < this->cache.size(); i++) {
                    if(this->cache[i].isLoadScheduled && !this->cache[i].wasUsedLastFrame) {
                        this->cache.erase(this->cache.begin() + i);
                        i--;
                        continue;
                    }
                }
            }
        }

        // create entry
        ENTRY entry;
        {
            entry.isLoadScheduled = true;
            entry.wasUsedLastFrame = true;
            entry.loadingTime = newLoadingTime;
            entry.evictionTime = newEvictionTime;
            entry.evictionTimeFrameCount = newEvictionTimeFrameCount;

            entry.osuFilePath = beatmap->getFilePath();
            entry.folder = beatmap->getFolder();
            entry.backgroundImageFileName = beatmap->getBackgroundImageFileName();

            entry.backgroundImagePathLoader = NULL;
            entry.image = NULL;
        }
        if(this->cache.size() < maxCacheEntries) this->cache.push_back(entry);
    }

    return NULL;
}
