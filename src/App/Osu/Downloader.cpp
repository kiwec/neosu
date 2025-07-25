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
#include "SString.h"
#include "SongBrowser/SongBrowser.h"

namespace {  // static

class DownloadManager;

// shared global instance
std::shared_ptr<DownloadManager> s_download_manager;

class DownloadManager {
   private:
    struct DownloadRequest {
        std::string url;
        std::atomic<float> progress{0.0f};
        std::atomic<int> response_code{0};
        std::vector<u8> data;
        std::mutex data_mutex;
        std::atomic<bool> completed{false};
        std::chrono::steady_clock::time_point retry_after{};
    };

    std::atomic<bool> shutting_down{false};
    std::unordered_map<std::string, std::shared_ptr<DownloadRequest>> active_downloads;
    std::mutex active_mutex;

    // rate limiting and queuing
    std::queue<std::shared_ptr<DownloadRequest>> download_queue;
    std::mutex queue_mutex;
    std::atomic<bool> currently_downloading{false};
    std::chrono::steady_clock::time_point last_download_start{};

    void checkAndStartNextDownload() {
        if(this->shutting_down.load() || this->currently_downloading.load()) return;

        std::scoped_lock lock(this->queue_mutex);
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

        debugLog("Downloading %s\n", request->url.c_str());

        NetworkHandler::RequestOptions options;
        options.timeout = 30;
        options.connectTimeout = 5;
        options.userAgent = bancho->user_agent.toUtf8();
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
            std::scoped_lock lock(request->data_mutex);
            request->response_code.store(static_cast<int>(response.responseCode));

            if(response.success && response.responseCode == 200) {
                request->data = std::vector<u8>(response.body.begin(), response.body.end());
                request->progress.store(1.0f);
                request->completed.store(true);
            } else {
                if(!response.success) {
                    debugLog("Failed to download %s: network error\n", request->url.c_str());
                }
                if(response.responseCode == 429) {
                    // rate limited, retry after 5 seconds
                    request->progress.store(0.0f);
                    request->retry_after = std::chrono::steady_clock::now() + std::chrono::seconds(5);

                    // re-queue for retry
                    std::scoped_lock lock(this->queue_mutex);
                    this->download_queue.push(request);
                    return;
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

    DownloadManager& operator=(const DownloadManager&) = delete;
    DownloadManager& operator=(DownloadManager&&) = delete;
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager(DownloadManager&&) = delete;

    void shutdown() {
        if(!this->shutting_down.exchange(true)) {
            // clear download queue to prevent new work
            std::scoped_lock lock(this->queue_mutex);
            while(!this->download_queue.empty()) {
                this->download_queue.pop();
            }
        }
    }

    std::shared_ptr<DownloadRequest> start_download(const std::string& url) {
        if(this->shutting_down.load()) return nullptr;

        std::scoped_lock lock(this->active_mutex);

        // check if already downloading or cached
        auto it = this->active_downloads.find(url);
        if(it != this->active_downloads.end()) {
            return it->second;
        }

        // create new download request
        auto request = std::make_shared<DownloadRequest>();
        request->url = url;

        this->active_downloads[url] = request;

        // queue for download
        {
            std::scoped_lock queue_lock(this->queue_mutex);
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
    std::string line;
    for(size_t i = 0; i < s_osu_data; i++) {
        if(osu_data[i] == '\n') {
            if(!line.starts_with("//")) {
                sscanf(line.c_str(), " BeatmapSetID : %i \n", &set_id);
                if(set_id != -1) return set_id;
            }

            line = "";
        } else {
            line.push_back(static_cast<char>(osu_data[i]));
        }
    }

    if(!line.starts_with("//")) {
        sscanf(line.c_str(), " BeatmapSetID : %i \n", &set_id);
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

    *progress = request->progress.load();
    *response_code = request->response_code.load();

    if(request->completed.load()) {
        std::scoped_lock lock(request->data_mutex);
        if(*response_code == 200) {
            out = request->data;
        }
    }
}

i32 extract_beatmapset_id(const u8* data, size_t data_s) {
    debugLog("Reading beatmapset (%d bytes)\n", data_s);

    Archive archive(data, data_s);
    if(!archive.isValid()) {
        debugLog("Failed to open .osz file\n");
        return -1;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog(".osz file is empty!\n");
        return -1;
    }

    for(const auto& entry : entries) {
        if(entry.isDirectory()) continue;

        std::string filename = entry.getFilename();
        if(env->getFileExtensionFromFilePath(filename).compare("osu") != 0) continue;

        auto osu_data = entry.extractToMemory();
        if(osu_data.empty()) continue;

        i32 set_id = get_beatmapset_id_from_osu_file(osu_data.data(), osu_data.size());
        if(set_id != -1) return set_id;
    }

    return -1;
}

bool extract_beatmapset(const u8* data, size_t data_s, std::string& map_dir) {
    debugLog("Extracting beatmapset (%d bytes)\n", data_s);

    Archive archive(data, data_s);
    if(!archive.isValid()) {
        debugLog("Failed to open .osz file\n");
        return false;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog(".osz file is empty!\n");
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
            debugLog("Failed to extract file %s\n", filename.c_str());
        }

    skip_file:;
        // when a file can't be extracted we just ignore it (as long as the archive is valid)
        // we'll check for errors when loading the beatmap
    }

    return true;
}

void download_beatmapset(u32 set_id, float* progress) {
    // Check if we already have downloaded it
    std::string map_dir = fmt::format(MCENGINE_DATA_DIR "maps/{}/", set_id);
    if(env->directoryExists(map_dir)) {
        *progress = 1.f;
        return;
    }

    std::vector<u8> data;

    auto scheme = cv::use_https.getBool() ? "https://" : "http://";
    auto download_url = fmt::format("{:s}osu.{:s}/d/", scheme, bancho->endpoint.toUtf8());
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
    if(beatmap != NULL) {
        *progress = 1.f;
        return beatmap;
    }

    // XXX: Currently, we do not try to find the difficulty from unloaded database, or from neosu downloaded maps
    auto it = beatmap_to_beatmapset.find(beatmap_id);
    if(it == beatmap_to_beatmapset.end()) {
        if(queried_map_id == beatmap_id) {
            // We already queried for the beatmapset ID, and are waiting for the response
            *progress = 0.f;
            return NULL;
        }

        APIRequest request;
        request.type = GET_BEATMAPSET_INFO;
        request.path = UString::format("/web/osu-search-set.php?b=%d&u=%s&h=%s", beatmap_id, bancho->username.toUtf8(),
                                       bancho->pw_md5.hash.data());
        request.extra_int = beatmap_id;
        BANCHO::Net::send_api_request(request);

        queried_map_id = beatmap_id;

        *progress = 0.f;
        return NULL;
    }

    i32 set_id = it->second;
    if(set_id == 0) {
        // Already failed to load the beatmap
        *progress = -1.f;
        return NULL;
    }

    download_beatmapset(set_id, progress);
    if(*progress == -1.f) {
        // Download failed, don't retry
        beatmap_to_beatmapset[beatmap_id] = 0;
        return NULL;
    }

    // Download not finished
    if(*progress != 1.f) return NULL;

    auto mapset_path = UString::format(MCENGINE_DATA_DIR "maps/%d/", set_id);
    db->addBeatmapSet(mapset_path.toUtf8(), set_id);
    debugLog("Finished loading beatmapset %d.\n", set_id);

    beatmap = db->getBeatmapDifficulty(beatmap_md5);
    if(beatmap == NULL) {
        beatmap_to_beatmapset[beatmap_id] = 0;
        *progress = -1.f;
        return NULL;
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
    if(beatmap != NULL) {
        *progress = 1.f;
        return beatmap;
    }

    // XXX: Currently, we do not try to find the difficulty from unloaded database, or from neosu downloaded maps
    auto it = beatmap_to_beatmapset.find(beatmap_id);
    if(it == beatmap_to_beatmapset.end()) {
        if(queried_map_id == beatmap_id) {
            // We already queried for the beatmapset ID, and are waiting for the response
            *progress = 0.f;
            return NULL;
        }

        // We already have the set ID, skip the API request
        if(beatmapset_id != 0) {
            beatmap_to_beatmapset[beatmap_id] = beatmapset_id;
            *progress = 0.f;
            return NULL;
        }

        APIRequest request;
        request.type = GET_BEATMAPSET_INFO;
        request.path = UString::format("/web/osu-search-set.php?b=%d&u=%s&h=%s", beatmap_id, bancho->username.toUtf8(),
                                       bancho->pw_md5.hash.data());
        request.extra_int = beatmap_id;
        BANCHO::Net::send_api_request(request);

        queried_map_id = beatmap_id;

        *progress = 0.f;
        return NULL;
    }

    i32 set_id = it->second;
    if(set_id == 0) {
        // Already failed to load the beatmap
        *progress = -1.f;
        return NULL;
    }

    download_beatmapset(set_id, progress);
    if(*progress == -1.f) {
        // Download failed, don't retry
        beatmap_to_beatmapset[beatmap_id] = 0;
        return NULL;
    }

    // Download not finished
    if(*progress != 1.f) return NULL;

    auto mapset_path = UString::format(MCENGINE_DATA_DIR "maps/%d/", set_id);
    db->addBeatmapSet(mapset_path.toUtf8());
    debugLog("Finished loading beatmapset %d.\n", set_id);

    beatmap = db->getBeatmapDifficulty(beatmap_id);
    if(beatmap == NULL) {
        beatmap_to_beatmapset[beatmap_id] = 0;
        *progress = -1.f;
        return NULL;
    }

    *progress = 1.f;
    return beatmap;
}

void process_beatmapset_info_response(Packet packet) {
    i32 map_id = packet.extra_int;
    if(packet.size == 0) {
        beatmap_to_beatmapset[map_id] = 0;
        return;
    }

    // {set_id}.osz|{artist}|{title}|{creator}|{status}|10.0|{last_update}|{set_id}|0|0|0|0|0
    auto tokens = SString::split(std::string{(char*)packet.memory}, "|");
    if(tokens.size() < 13) return;

    beatmap_to_beatmapset[map_id] = static_cast<i32>(strtol(tokens[7].c_str(), NULL, 10));
}
}  // namespace Downloader
