#include "Downloader.h"

#include "Archival.h"
#include "Bancho.h"
#include "BanchoApi.h"
#include "OsuConVars.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "NetworkHandler.h"
#include "Osu.h"
#include "Parsing.h"
#include "SString.h"
#include "SyncMutex.h"
#include "Logging.h"
#include "SongBrowser.h"
#include "Environment.h"

#include <atomic>
#include <memory>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <utility>

namespace {  // static

class DownloadManager;

// shared global instance
std::shared_ptr<DownloadManager> s_download_manager;

// TODO: allow more than 1 download at a time, while still respecting per-domain rate limits

class DownloadManager {
    NOCOPY_NOMOVE(DownloadManager)
   private:
    struct DownloadRequest {
        std::string url;
        std::atomic<float> progress{0.0f};
        std::atomic<int> response_code{0};
        std::vector<u8> data;
        Sync::mutex data_mutex;
        std::atomic<bool> completed{false};
        std::chrono::steady_clock::time_point retry_after{};
    };

    std::atomic<bool> shutting_down{false};
    Hash::unstable_stringmap<std::shared_ptr<DownloadRequest>> active_downloads;
    Sync::mutex active_mutex;

    // rate limiting and queuing
    std::queue<std::shared_ptr<DownloadRequest>> download_queue;
    Sync::mutex queue_mutex;
    std::atomic<bool> currently_downloading{false};
    std::chrono::steady_clock::time_point last_download_start{};

    void checkAndStartNextDownload() {
        if(this->shutting_down.load(std::memory_order_acquire) ||
           this->currently_downloading.load(std::memory_order_acquire))
            return;

        Sync::scoped_lock lock(this->queue_mutex);
        if(this->download_queue.empty()) return;

        auto now = std::chrono::steady_clock::now();

        // check if we need to wait for rate limiting (100ms between downloads)
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_download_start);
        if(elapsed < std::chrono::milliseconds(100)) return;

        // check for retry delays
        auto request = this->download_queue.front();
        if(request->retry_after > now) return;

        // ready to start next download
        this->download_queue.pop();
        this->startDownloadNow(request);
    }

    void startDownloadNow(const std::shared_ptr<DownloadRequest>& request) {
        if(this->shutting_down.load(std::memory_order_acquire)) return;
        this->currently_downloading.store(true, std::memory_order_release);
        this->last_download_start = std::chrono::steady_clock::now();

        debugLog("Downloading {:s}", request->url.c_str());

        NeoNet::RequestOptions options{
            .user_agent = BanchoState::user_agent,
            .progress_callback =
                [request](float progress) { request->progress.store(progress, std::memory_order_release); },
            .timeout = 30,
            .connect_timeout = 5,
            .follow_redirects = true,
        };

        // capture s_download_manager as a copy to keep DownloadManager alive during callback
        networkHandler->httpRequestAsync(request->url, std::move(options),
                                         [self = s_download_manager, request](NeoNet::Response response) {
                                             self->onDownloadComplete(request, std::move(response));
                                         });
    }

    void onDownloadComplete(const std::shared_ptr<DownloadRequest>& request, NeoNet::Response response) {
        if(this->shutting_down.load(std::memory_order_acquire)) return;
        this->currently_downloading.store(false, std::memory_order_release);

        // update request with results
        {
            Sync::scoped_lock lock(request->data_mutex);
            if(response.success) {
                request->response_code.store(static_cast<int>(response.response_code), std::memory_order_release);
                request->data = std::vector<u8>(response.body.begin(), response.body.end());

                if(response.response_code == 429) {
                    // rate limited, retry after 5 seconds
                    // TODO: read headers and if the usual retry-after are set, follow those
                    // TODO: per-domain rate limits
                    request->progress.store(0.0f, std::memory_order_release);
                    request->retry_after = std::chrono::steady_clock::now() + std::chrono::seconds(5);

                    // re-queue for retry
                    Sync::scoped_lock lock(this->queue_mutex);
                    this->download_queue.push(request);
                } else {
                    request->progress.store(1.f, std::memory_order_release);
                    request->completed.store(true, std::memory_order_release);
                }
            } else {
                // TODO: forward network error message in response
                debugLog("Failed to download {:s}: network error", request->url.c_str());
                request->progress.store(-1.f, std::memory_order_release);
                request->completed.store(true, std::memory_order_release);
            }
        }

        // check if we can start next download
        this->checkAndStartNextDownload();
    }

   public:
    DownloadManager() { this->last_download_start = std::chrono::steady_clock::now() - std::chrono::milliseconds(100); }

    ~DownloadManager() { this->shutdown(); }

    void shutdown() {
        if(!this->shutting_down.exchange(true)) {
            // clear download queue to prevent new work
            Sync::scoped_lock lock(this->queue_mutex);
            while(!this->download_queue.empty()) {
                this->download_queue.pop();
            }
        }
    }

    std::shared_ptr<DownloadRequest> start_download(const std::string& url) {
        if(this->shutting_down.load(std::memory_order_acquire)) return nullptr;

        Sync::scoped_lock lock(this->active_mutex);

        // check if already downloading or cached
        auto it = this->active_downloads.find(url);
        if(it != this->active_downloads.end()) {
            // if we have been rate limited, we might need to resume downloads manually
            if(!this->currently_downloading.load(std::memory_order_acquire)) {
                this->checkAndStartNextDownload();
            }

            return it->second;
        }

        // create new download request
        auto request = std::make_shared<DownloadRequest>();
        request->url = url;

        this->active_downloads[url] = request;

        // queue for download
        {
            Sync::scoped_lock queue_lock(this->queue_mutex);
            this->download_queue.push(request);
        }

        // try to start immediately if possible
        this->checkAndStartNextDownload();

        return request;
    }
};

// helper
std::unordered_map<i32, i32> beatmap_to_beatmapset;

i32 get_beatmapset_id_from_osu_file(const u8* osu_data, size_t s_osu_data) {
    i32 set_id = -1;
    bool inMetadata = false;

    std::string_view file((const char*)osu_data, (const char*)osu_data + s_osu_data);
    for(const auto line : SString::split_newlines(file)) {
        if(line.empty() || SString::is_comment(line)) continue;
        if(line.contains("[Metadata]")) {
            inMetadata = true;
            continue;
        }
        if(line.starts_with('[') && inMetadata) {
            break;
        }
        if(inMetadata) {
            if(Parsing::parse(line, "BeatmapSetID", ':', &set_id)) {
                return set_id;
            }
            continue;
        }
    }

    return -1;
}
}  // namespace

namespace Downloader {

void abort_downloads() {
    if(s_download_manager) {
        s_download_manager->shutdown();
        s_download_manager.reset();
    }
}

void download(const char* url, float* progress, std::vector<u8>& out, int* response_code) {
    if(!s_download_manager) {
        s_download_manager = std::make_shared<DownloadManager>();
    }

    auto request = s_download_manager->start_download(std::string(url));
    if(!request) {
        *progress = -1.0f;
        *response_code = 0;
        return;
    }

    *progress = std::min(0.99f, request->progress.load(std::memory_order_acquire));

    if(request->completed.load(std::memory_order_acquire)) {
        Sync::scoped_lock lock(request->data_mutex);
        *progress = 1.f;
        *response_code = request->response_code.load(std::memory_order_acquire);
        out = request->data;
    }
}

i32 extract_beatmapset_id(const u8* data, size_t data_s) {
    debugLog("Reading beatmapset ({:d} bytes)", data_s);
    i32 set_id = -1;

    Archive::Reader archive(data, data_s);
    if(!archive.isValid()) {
        debugLog("Failed to open .osz file");
        return set_id;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog(".osz file is empty!");
        return set_id;
    }

    for(const auto& entry : entries) {
        if(entry.isDirectory()) continue;

        std::string filename = entry.getFilename();
        if(env->getFileExtensionFromFilePath(filename).compare("osu") != 0) continue;

        const auto& osu_data = entry.getUncompressedData();
        if(osu_data.empty()) continue;

        set_id = get_beatmapset_id_from_osu_file(osu_data.data(), osu_data.size());
        if(set_id != -1) break;
    }

    return set_id;
}

bool extract_beatmapset(const u8* data, size_t data_s, std::string& map_dir) {
    debugLog("Extracting beatmapset ({:d} bytes)", data_s);

    Archive::Reader archive(data, data_s);
    if(!archive.isValid()) {
        debugLog("Failed to open .osz file");
        return false;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog(".osz file is empty!");
        return false;
    }

    if(!env->directoryExists(map_dir)) {
        env->createDirectory(map_dir);
    }

    for(const auto& entry : entries) {
        if(entry.isDirectory()) continue;

        std::string filename = entry.getFilename();
        const auto folders = SString::split(filename, '/');
        std::string file_path = map_dir;

        for(const auto& folder : folders) {
            if(!env->directoryExists(file_path)) {
                env->createDirectory(file_path);
            }

            if(folder == "..") {
                // security check: skip files with path traversal attempts
                goto skip_file;
            } else {
                file_path.append("/");
                file_path.append(folder);
            }
        }

        if(!entry.extractToFile(file_path)) {
            debugLog("Failed to extract file {:s}", filename.c_str());
        }

    skip_file:;
        // when a file can't be extracted we just ignore it (as long as the archive is valid)
        // we'll check for errors when loading the beatmap
    }

    return true;
}

void download_beatmapset(u32 set_id, float* progress) {
    // Check if we already have downloaded it
    std::string map_dir = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);
    if(env->directoryExists(map_dir)) {
        *progress = 1.f;
        return;
    }

    std::vector<u8> data;

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto download_url = fmt::format("{:s}osu.{}/d/", scheme, BanchoState::endpoint);
    if(cv::beatmap_mirror_override.getString().length() > 0) {
        download_url = cv::beatmap_mirror_override.getString();
    }
    download_url.append(fmt::format("{:d}", set_id));

    int response_code = 0;
    download(download_url.c_str(), progress, data, &response_code);
    if(response_code == 0 || *progress == -1.f) return;  // still downloading/errored

    // Server returned 404 or other, treat it as an error
    if(response_code != 200) {
        *progress = -1.f;
        return;
    }

    // Download succeeded: save map to disk
    if(!extract_beatmapset(data.data(), data.size(), map_dir)) {
        *progress = -1.f;
        return;
    }
}

DatabaseBeatmap* download_beatmap(i32 beatmap_id, MD5Hash beatmap_md5, float* progress) {
    static i32 queried_map_id = 0;

    auto beatmap = db->getBeatmapDifficulty(beatmap_md5);
    if(beatmap != nullptr) {
        *progress = 1.f;
        return beatmap;
    }

    // XXX: Currently, we do not try to find the difficulty from unloaded database, or from neosu downloaded maps
    auto it = beatmap_to_beatmapset.find(beatmap_id);
    if(it == beatmap_to_beatmapset.end()) {
        if(queried_map_id == beatmap_id) {
            // We already queried for the beatmapset ID, and are waiting for the response
            *progress = 0.f;
            return nullptr;
        }

        std::string url{"/web/osu-search-set.php?"};
        url.append(fmt::format("b={}", beatmap_id));
        BANCHO::Api::append_auth_params(url);

        BANCHO::Api::Request request;
        request.type = BANCHO::Api::GET_BEATMAPSET_INFO;
        request.path = url;
        request.extra_int = beatmap_id;
        BANCHO::Api::send_request(request);

        queried_map_id = beatmap_id;

        *progress = 0.f;
        return nullptr;
    }

    i32 set_id = it->second;
    if(set_id == 0) {
        // Already failed to load the beatmap
        *progress = -1.f;
        return nullptr;
    }

    download_beatmapset(set_id, progress);
    if(*progress == -1.f) {
        // Download failed, don't retry
        beatmap_to_beatmapset[beatmap_id] = 0;
        return nullptr;
    }

    // Download not finished
    if(*progress != 1.f) return nullptr;

    std::string mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);
    db->addBeatmapSet(mapset_path, set_id, true);
    debugLog("Finished loading beatmapset {:d}.", set_id);

    beatmap = db->getBeatmapDifficulty(beatmap_md5);
    if(beatmap == nullptr) {
        beatmap_to_beatmapset[beatmap_id] = 0;
        *progress = -1.f;
        return nullptr;
    }

    // Some beatmaps don't provide beatmap/beatmapset IDs in the .osu files
    // While we're clueless on the beatmap IDs of the other maps in the set,
    // we can still make sure at least the one we wanted is correct.
    beatmap->iID = beatmap_id;

    *progress = 1.f;
    return beatmap;
}

DatabaseBeatmap* download_beatmap(i32 beatmap_id, i32 beatmapset_id, float* progress) {
    static i32 queried_map_id = 0;

    auto beatmap = db->getBeatmapDifficulty(beatmap_id);
    if(beatmap != nullptr) {
        *progress = 1.f;
        return beatmap;
    }

    // XXX: Currently, we do not try to find the difficulty from unloaded database, or from neosu downloaded maps
    auto it = beatmap_to_beatmapset.find(beatmap_id);
    if(it == beatmap_to_beatmapset.end()) {
        if(queried_map_id == beatmap_id) {
            // We already queried for the beatmapset ID, and are waiting for the response
            *progress = 0.f;
            return nullptr;
        }

        // We already have the set ID, skip the API request
        if(beatmapset_id != 0) {
            beatmap_to_beatmapset[beatmap_id] = beatmapset_id;
            *progress = 0.f;
            return nullptr;
        }

        std::string url{"/web/osu-search-set.php?"};
        url.append(fmt::format("b={}", beatmap_id));
        BANCHO::Api::append_auth_params(url);

        BANCHO::Api::Request request;
        request.type = BANCHO::Api::GET_BEATMAPSET_INFO;
        request.path = url;
        request.extra_int = beatmap_id;
        BANCHO::Api::send_request(request);

        queried_map_id = beatmap_id;

        *progress = 0.f;
        return nullptr;
    }

    i32 set_id = it->second;
    if(set_id == 0) {
        // Already failed to load the beatmap
        *progress = -1.f;
        return nullptr;
    }

    download_beatmapset(set_id, progress);
    if(*progress == -1.f) {
        // Download failed, don't retry
        beatmap_to_beatmapset[beatmap_id] = 0;
        return nullptr;
    }

    // Download not finished
    if(*progress != 1.f) return nullptr;

    std::string mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);
    db->addBeatmapSet(mapset_path, -1, true);
    debugLog("Finished loading beatmapset {:d}.", set_id);

    beatmap = db->getBeatmapDifficulty(beatmap_id);
    if(beatmap == nullptr) {
        beatmap_to_beatmapset[beatmap_id] = 0;
        *progress = -1.f;
        return nullptr;
    }

    *progress = 1.f;
    return beatmap;
}

BeatmapSetMetadata parse_beatmapset_metadata(std::string_view server_response) {
    BeatmapSetMetadata meta;

    // Reference: https://github.com/osuTitanic/deck/blob/8384b74e/app/routes/web/direct.py#L28-L69
    const auto tokens = SString::split(server_response, '|');
    if(tokens.size() < 8) return meta;

    meta.osz_filename = tokens[0];
    meta.artist = tokens[1];
    meta.title = tokens[2];
    meta.creator = tokens[3];
    meta.ranking_status = Parsing::strto<u8>(tokens[4]);
    meta.avg_user_rating = Parsing::strto<f32>(tokens[5]);
    meta.last_update = Parsing::strto<u64>(tokens[6]);  // TODO: incorrect?
    meta.set_id = Parsing::strto<i32>(tokens[7]);

    if(tokens.size() < 9) return meta;
    meta.topic_id = Parsing::strto<i32>(tokens[8]);

    if(tokens.size() < 10) return meta;
    meta.has_video = Parsing::strto<bool>(tokens[9]);

    if(tokens.size() < 11) return meta;
    meta.has_storyboard = Parsing::strto<bool>(tokens[10]);

    if(tokens.size() < 12) return meta;
    meta.osz_filesize = Parsing::strto<u64>(tokens[11]);

    if(tokens.size() < 13) return meta;
    meta.osz_filesize_novideo = Parsing::strto<u64>(tokens[12]);

    if(tokens.size() < 14) return meta;
    const auto maps = SString::split(tokens[13], ',');

    for(const auto map : maps) {
        const auto spl = SString::split(map, '@');
        if(spl.size() < 2) continue;
        const std::string_view raw_diff = map.substr(0, map.find_last_of('@'));
        const std::string_view mode_str = spl.back();

        if(raw_diff.contains("★")) {
            // Mayflower's Hard★3.60@0
            // used by: catboy.best api

            const auto diff_srs = SString::split(raw_diff, "★"sv);
            std::string diffname;

            // handle a possible case where the diff name itself contains the separator
            // so only parse the final token as the SR
            assert(diff_srs.size() >= 2);
            for(auto tokit = diff_srs.begin(); tokit != diff_srs.end() - 1; tokit++) {
                diffname += *tokit;
            }

            const f32 sr = Parsing::strto<f32>(diff_srs.back());
            meta.beatmaps.push_back(
                BeatmapMetadata{.diffname = diffname, .star_rating = sr, .mode = Parsing::strto<u8>(mode_str)});
        } else if(raw_diff.contains("⭐") && raw_diff[0] == '[') {
            // [3.60⭐] Mayflower's Hard {cs: 3.5 / od: 6.0 / ar: 8.0 / hp: 3.5}@0
            // used by: bancho.py, banchus (akatsuki), osu.direct api

            // maybe can try parsing beatmap settings in the future
            const uSz star_idx = raw_diff.find("⭐"sv);
            const uSz star_end_idx = star_idx + "⭐"sv.size();

            // 1, -1 to remove left bracket
            const std::string_view sr_text = raw_diff.substr(1, star_idx - 1);
            const uSz bracket_cs_idx = raw_diff.find(" {cs: ");

            // + 2 to remove right bracket and space
            const uSz diffbegin = (star_end_idx + 2 > raw_diff.size()) ? star_end_idx : star_end_idx + 2;
            const uSz diffend = bracket_cs_idx == std::string::npos ? std::string::npos : bracket_cs_idx - diffbegin;

            const std::string_view diff_text = raw_diff.substr(diffbegin, diffend);

            const f32 sr = Parsing::strto<f32>(sr_text);
            meta.beatmaps.push_back(
                BeatmapMetadata{.diffname{diff_text}, .star_rating = sr, .mode = Parsing::strto<u8>(mode_str)});
        } else {
            // Mayflower's Hard@0
            // used by: ripple, titanic
            meta.beatmaps.push_back(
                BeatmapMetadata{.diffname{raw_diff}, .star_rating = 0.f, .mode = Parsing::strto<u8>(mode_str)});
        }
    }

    return meta;
}

void process_beatmapset_info_response(const Packet& packet) {
    i32 map_id = packet.extra_int;
    if(packet.size == 0) {
        beatmap_to_beatmapset[map_id] = 0;
        return;
    }

    auto metadata = parse_beatmapset_metadata((char*)packet.memory);
    beatmap_to_beatmapset[map_id] = metadata.set_id;
}
}  // namespace Downloader
