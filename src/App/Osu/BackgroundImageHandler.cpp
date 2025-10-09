// Copyright (c) 2020, PG, All rights reserved.
#include "BackgroundImageHandler.h"

#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "File.h"
#include "Parsing.h"
#include "ResourceManager.h"

#include "Skin.h"

class BGImageHandler::MapBGImagePathLoader final : public Resource {
    NOCOPY_NOMOVE(MapBGImagePathLoader)
   public:
    MapBGImagePathLoader(const std::string &filePath) : Resource(filePath) {}
    ~MapBGImagePathLoader() override { destroy(); }

    [[nodiscard]] inline const std::string &getParsedBGFileName() const { return this->parsed_bg_filename; }

    [[nodiscard]] Type getResType() const override { return APPDEFINED; }  // TODO: handle this better?

   protected:
    void init() override {
        // (nothing)
        this->bReady.store(true, std::memory_order_release);
    }

    void initAsync() override {
        if(this->isInterrupted()) return;

        File file(this->sFilePath);

        if(this->isInterrupted() || !file.canRead()) return;
        const uSz file_size = file.getFileSize();

        static constexpr const uSz CHUNK_SIZE = 64ULL;

        std::array<std::string, CHUNK_SIZE> lines;
        bool found = false, quit = false, is_events_block = false;

        uSz lines_in_chunk = std::min<uSz>(file_size, CHUNK_SIZE);

        std::string temp_parsed_filename;
        temp_parsed_filename.reserve(64);

        while(!found && !quit && lines_in_chunk > 0) {
            // read 64 lines at a time
            for(uSz i = 0; i < lines_in_chunk; i++) {
                if(this->isInterrupted()) {
                    return;
                }
                if(!file.canRead()) {
                    // cut short
                    lines_in_chunk = i;
                    break;
                }
                lines[i] = file.readLine();
            }

            for(uSz i = 0; i < lines_in_chunk; i++) {
                if(this->isInterrupted()) {
                    return;
                }

                std::string_view cur_line = lines[i];

                // ignore comments, but only if at the beginning of a line (e.g. allow Artist:DJ'TEKINA//SOMETHING)
                if(cur_line.starts_with("//")) continue;

                if(!is_events_block && cur_line.contains("[Events]")) {
                    is_events_block = true;
                    continue;
                } else if(cur_line.contains("[TimingPoints]") || cur_line.contains("[Colours]") ||
                          cur_line.contains("[HitObjects]")) {
                    quit = true;
                    break;  // NOTE: stop early
                }

                if(!is_events_block) continue;

                // parse events block for filename
                i32 type, start;
                if(Parsing::parse(cur_line, &type, ',', &start, ',', &temp_parsed_filename) && type == 0) {
                    this->parsed_bg_filename = temp_parsed_filename;
                    found = true;
                    break;
                }
            }
        }

        this->bAsyncReady.store(true, std::memory_order_release);
        // NOTE: on purpose. there is nothing to do in init(), so finish 1 frame early
        this->bReady.store(true, std::memory_order_release);
    };

    void destroy() override { /* nothing */ }

   private:
    std::string parsed_bg_filename;
};

BGImageHandler::BGImageHandler() {
    this->max_cache_size =
        std::clamp<u32>(static_cast<u32>(std::round(cv::background_image_cache_size.getFloat())), 0, 128);
    cv::background_image_cache_size.setCallback(SA::MakeDelegate<&BGImageHandler::cacheSizeCB>(this));

    this->eviction_delay_frames = std::clamp<u32>(cv::background_image_eviction_delay_frames.getVal<u32>(), 0, 1024);
    cv::background_image_eviction_delay_frames.setCallback(SA::MakeDelegate<&BGImageHandler::evictionDelayCB>(this));

    this->image_loading_delay = std::clamp<f32>(cv::background_image_loading_delay.getFloat(), 0.f, 2.f);
    cv::background_image_loading_delay.setCallback(SA::MakeDelegate<&BGImageHandler::loadingDelayCB>(this));
}

BGImageHandler::~BGImageHandler() {
    for(const auto &[_, entry] : this->cache) {
        resourceManager->destroyResource(entry.bg_image_path_ldr);
        resourceManager->destroyResource(entry.image);
    }
    this->cache.clear();

    cv::background_image_cache_size.removeCallback();
    cv::background_image_eviction_delay_frames.removeCallback();
    cv::background_image_loading_delay.removeCallback();
}

void BGImageHandler::update(bool allow_eviction) {
    const auto engine_time = engine->getTime();
    const auto engine_framecount = engine->getFrameCount();

    for(auto it = this->cache.begin(); it != this->cache.end();) {
        auto &[osu_path, entry] = *it;

        // NOTE: avoid load/unload jitter if framerate is below eviction delay
        const bool was_used_last_frame = entry.used_last_frame;
        entry.used_last_frame = false;

        // check and handle evictions
        if(!was_used_last_frame && (engine_framecount >= entry.evict_framecnt)) {
            if(allow_eviction) {
                if(!this->frozen && !engine->isMinimized()) {
                    if(entry.bg_image_path_ldr != nullptr) entry.bg_image_path_ldr->interruptLoad();
                    if(entry.image != nullptr) entry.image->interruptLoad();

                    resourceManager->destroyResource(entry.bg_image_path_ldr,
                                                     ResourceManager::DestroyMode::FORCE_ASYNC);
                    resourceManager->destroyResource(entry.image, ResourceManager::DestroyMode::FORCE_ASYNC);

                    it = this->cache.erase(it);
                    continue;
                }
            } else {
                entry.evict_framecnt = engine_framecount + this->eviction_delay_frames;
            }
        } else if(was_used_last_frame) {
            // check and handle scheduled loads
            if(entry.load_scheduled) {
                if(engine_time >= entry.loading_time) {
                    entry.load_scheduled = false;

                    if(entry.bg_image_filename.length() < 2) {
                        // if the backgroundImageFileName is not loaded, then we have to create a full
                        // DatabaseBeatmapBackgroundImagePathLoader
                        entry.image = nullptr;
                        this->handleLoadPathForEntry(osu_path, entry);
                    } else {
                        // if backgroundImageFileName is already loaded/valid, then we can directly load the image
                        entry.bg_image_path_ldr = nullptr;
                        this->handleLoadImageForEntry(entry);
                    }
                }
            } else {
                // no load scheduled (potential load-in-progress if it was necessary), handle backgroundImagePathLoader
                // loading finish
                if(entry.image == nullptr && entry.bg_image_path_ldr != nullptr && entry.bg_image_path_ldr->isReady()) {
                    std::string bg_loaded_name = entry.bg_image_path_ldr->getParsedBGFileName();
                    if(bg_loaded_name.length() > 1) {
                        entry.bg_image_filename = bg_loaded_name;
                        this->handleLoadImageForEntry(entry);
                    }

                    resourceManager->destroyResource(entry.bg_image_path_ldr,
                                                     ResourceManager::DestroyMode::FORCE_ASYNC);
                    entry.bg_image_path_ldr = nullptr;
                }
            }
        }

        ++it;
    }

    // reset flags
    this->frozen = false;

    // DEBUG:
    // debugLog("m_cache.size() = {:d}", (int)this->cache.size());
}

void BGImageHandler::handleLoadPathForEntry(const std::string &path, ENTRY &entry) {
    entry.bg_image_path_ldr = new MapBGImagePathLoader(path);

    // start path load
    resourceManager->requestNextLoadAsync();
    resourceManager->loadResource(entry.bg_image_path_ldr);
}

void BGImageHandler::handleLoadImageForEntry(ENTRY &entry) {
    std::string full_bg_image_path = fmt::format("{}{}", entry.folder, entry.bg_image_filename);

    // start image load
    resourceManager->requestNextLoadAsync();
    resourceManager->requestNextLoadUnmanaged();
    entry.image = resourceManager->loadImageAbsUnnamed(full_bg_image_path, true);
}

const Image *BGImageHandler::getLoadBackgroundImage(const DatabaseBeatmap *beatmap, bool load_immediately) {
    if(beatmap == nullptr || !cv::load_beatmap_background_images.getBool() || !beatmap->draw_background) return nullptr;

    // NOTE: no references to beatmap are kept anywhere (database can safely be deleted/reloaded without having to
    // notify the BackgroundImageHandler)

    const f32 new_loading_time = engine->getTime() + (load_immediately ? 0.f : this->image_loading_delay);
    const u32 new_eviction_framecnt = engine->getFrameCount() + this->eviction_delay_frames;

    std::string_view beatmap_filepath = beatmap->getFilePath();

    if(const auto &it = this->cache.find(beatmap_filepath); it != this->cache.end()) {
        // 1) if the path or image is already loaded, return image ref immediately (which may still be NULL) and keep track
        // of when it was last requested
        auto &entry = it->second;

        entry.used_last_frame = true;
        entry.evict_framecnt = new_eviction_framecnt;

        // HACKHACK: to improve future loading speed, if we have already loaded the backgroundImageFileName, force
        // update the database backgroundImageFileName and fullBackgroundImageFilePath this is similar to how it
        // worked before the rework, but 100% safe(r) since we are not async
        if(entry.image != nullptr && entry.bg_image_filename.length() > 1 &&
           beatmap->getBackgroundImageFileName().length() < 2) {
            const_cast<DatabaseBeatmap *>(beatmap)->sBackgroundImageFileName = entry.bg_image_filename;
            const_cast<DatabaseBeatmap *>(beatmap)->sFullBackgroundImageFilePath =
                fmt::format("{}{}", entry.folder, entry.bg_image_filename);
        }

        return this->getImageOrSkinFallback(entry.image);
    } else {
        // 2) not found in cache, so create a new entry which will get handled in the next update

        // try evicting stale not-yet-loaded-nor-started-loading entries on overflow
        if(this->cache.size() >= this->max_cache_size) {
            for(auto it = this->cache.begin(); it != this->cache.end();) {
                const auto &[osu_path, entry] = *it;
                if(entry.load_scheduled && !entry.used_last_frame) {
                    it = this->cache.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // create entry
        ENTRY entry{
            .folder = beatmap->getFolder(),
            .bg_image_filename = beatmap->getBackgroundImageFileName(),
            .bg_image_path_ldr = nullptr,
            .image = nullptr,
            .evict_framecnt = new_eviction_framecnt,
            .loading_time = new_loading_time,
            .load_scheduled = true,
            .used_last_frame = true,
        };

        if(this->cache.size() < this->max_cache_size) this->cache.try_emplace(beatmap->getFilePath(), entry);
    }

    return nullptr;
}

const Image *BGImageHandler::getImageOrSkinFallback(const Image *candidate_loaded) const {
    const Image *ret = candidate_loaded;
    // if we got an image but it failed for whatever reason, return the user skin as a fallback instead
    if(candidate_loaded && candidate_loaded->failedLoad()) {
        const Image *skin_bg = nullptr;
        if(osu->getSkin() && (skin_bg = osu->getSkin()->getMenuBackground()) && skin_bg != MISSING_TEXTURE &&
           !skin_bg->failedLoad()) {
            ret = skin_bg;
        }
    }
    return ret;
}
