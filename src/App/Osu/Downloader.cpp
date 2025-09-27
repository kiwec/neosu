#include "Downloader.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <utility>

#include "Archival.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "NetworkHandler.h"
#include "Osu.h"
#include "Parsing.h"
#include "SString.h"
#include "SyncMutex.h"
#include "Logging.h"
#include "SongBrowser/SongBrowser.h"

namespace {  // static

class DownloadManager;

// shared global instance
std::shared_ptr<DownloadManager> s_download_manager;

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
    std::unordered_map<std::string, std::shared_ptr<DownloadRequest>> active_downloads;
    Sync::mutex active_mutex;

    // rate limiting and queuing
    std::queue<std::shared_ptr<DownloadRequest>> download_queue;
    Sync::mutex queue_mutex;
    std::atomic<bool> currently_downloading{false};
    std::chrono::steady_clock::time_point last_download_start{};

    void checkAndStartNextDownload() {
        if(this->shutting_down.load() || this->currently_downloading.load()) return;

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
        if(this->shutting_down.load()) return;
        this->currently_downloading.store(true);
        this->last_download_start = std::chrono::steady_clock::now();

        debugLog("Downloading {:s}", request->url.c_str());

        NetworkHandler::RequestOptions options;
        options.timeout = 30;
        options.connectTimeout = 5;
        options.userAgent = BanchoState::user_agent.toUtf8();
        options.followRedirects = true;
        options.progressCallback = [request](float progress) { request->progress.store(progress); };

        // capture s_download_manager as a copy to keep DownloadManager alive during callback
        networkHandler->httpRequestAsync(
            UString(request->url),
            [self = s_download_manager, request](NetworkHandler::Response response) {
                self->onDownloadComplete(request, std::move(response));
            },
            options);
    }

    void onDownloadComplete(const std::shared_ptr<DownloadRequest>& request, NetworkHandler::Response response) {
        if(this->shutting_down.load()) return;
        this->currently_downloading.store(false);

        // update request with results
        {
            Sync::scoped_lock lock(request->data_mutex);
            request->response_code.store(static_cast<int>(response.responseCode));

            if(response.success && response.responseCode == 200) {
                request->data = std::vector<u8>(response.body.begin(), response.body.end());
                request->progress.store(1.0f);
                request->completed.store(true);
            } else {
                if(!response.success) {
                    debugLog("Failed to download {:s}: network error", request->url.c_str());
                }
                if(response.responseCode == 429) {
                    // rate limited, retry after 5 seconds
                    // TODO: read headers and if the usual retry-after are set, follow those
                    // TODO: per-domain rate limits
                    request->progress.store(0.0f);
                    request->retry_after = std::chrono::steady_clock::now() + std::chrono::seconds(5);

                    // re-queue for retry
                    Sync::scoped_lock lock(this->queue_mutex);
                    this->download_queue.push(request);
                } else {
                    request->progress.store(-1.0f);
                    request->completed.store(true);
                }
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
        if(this->shutting_down.load()) return nullptr;

        Sync::scoped_lock lock(this->active_mutex);

        // check if already downloading or cached
        auto it = this->active_downloads.find(url);
        if(it != this->active_downloads.end()) {
            // if we have been rate limited, we might need to resume downloads manually
            if(!this->currently_downloading.load()) {
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

    std::string file((const char*)osu_data, s_osu_data);
    auto lines = SString::split(file, "\n");
    for(auto& line : lines) {
        if(line.empty()) continue;
        if(line.starts_with("//")) continue;
        if(line.back() == '\r') line.pop_back();

        if(Parsing::parse(line.c_str(), "BeatmapSetID", ':', &set_id)) {
            return set_id;
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

    *progress = std::min(0.99f, request->progress.load());

    if(request->completed.load()) {
        Sync::scoped_lock lock(request->data_mutex);
        *progress = 1.f;
        *response_code = request->response_code.load();
        if(*response_code == 200) {
            out = request->data;
        }
    }
}

i32 extract_beatmapset_id(const u8* data, size_t data_s) {
    debugLog("Reading beatmapset ({:d} bytes)", data_s);
    i32 set_id = -1;

    Archive archive(data, data_s);
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

        auto osu_data = entry.extractToMemory();
        if(osu_data.empty()) continue;

        set_id = get_beatmapset_id_from_osu_file(osu_data.data(), osu_data.size());
        if(set_id != -1) break;
    }

    return set_id;
}

bool extract_beatmapset(const u8* data, size_t data_s, std::string& map_dir) {
    debugLog("Extracting beatmapset ({:d} bytes)", data_s);

    Archive archive(data, data_s);
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
        const auto folders = SString::split(filename, "/");
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
                file_path.append(folder.c_str());
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
    if(response_code != 200) return;

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

        UString url{"/web/osu-search-set.php?"};
        url.append(UString::fmt("b={}", beatmap_id));
        BANCHO::Net::append_auth_params(url);

        APIRequest request;
        request.type = GET_BEATMAPSET_INFO;
        request.path = url;
        request.extra_int = beatmap_id;
        BANCHO::Net::send_api_request(request);

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
    db->addBeatmapSet(mapset_path, set_id);
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

        UString url{"/web/osu-search-set.php?"};
        url.append(UString::fmt("b={}", beatmap_id));
        BANCHO::Net::append_auth_params(url);

        APIRequest request;
        request.type = GET_BEATMAPSET_INFO;
        request.path = url;
        request.extra_int = beatmap_id;
        BANCHO::Net::send_api_request(request);

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
    db->addBeatmapSet(mapset_path);
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

void process_beatmapset_info_response(Packet& packet) {
    i32 map_id = packet.extra_int;
    if(packet.size == 0) {
        beatmap_to_beatmapset[map_id] = 0;
        return;
    }

    // {set_id}.osz|{artist}|{title}|{creator}|{status}|10.0|{last_update}|{set_id}|0|0|0|0|0
    auto tokens = SString::split(std::string{(char*)packet.memory}, "|");
    if(tokens.size() < 13) return;

    beatmap_to_beatmapset[map_id] = static_cast<i32>(strtol(tokens[7].c_str(), nullptr, 10));
}
}  // namespace Downloader
