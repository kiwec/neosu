#pragma once
#include "types.h"

#include <vector>
#include <string>

class DatabaseBeatmap;
struct MD5Hash;
struct Packet;

namespace Downloader {

struct BeatmapMetadata {
    std::string version;
    u8 mode;
};

struct BeatmapSetMetadata {
    std::string osz_filename{};
    std::string artist{};
    std::string title{};
    std::string creator{};
    u8 ranking_status{0};
    f32 avg_user_rating{10.0};
    u64 last_update{0};  // TODO: wrong type?
    i32 set_id{0};
    i32 topic_id{0};
    bool has_video{false};
    bool has_storyboard{false};
    u64 osz_filesize{0};
    u64 osz_filesize_novideo{0};

    std::vector<BeatmapMetadata> beatmaps;
};

BeatmapSetMetadata parse_beatmapset_metadata(std::string_view server_response);

void abort_downloads();

// Downloads `url` and stores downloaded file data into `out`
// When file is fully downloaded, `progress` is 1 and `out` is not NULL
// When download fails, `progress` is -1
void download(const char *url, float *progress, std::vector<u8> &out, int *response_code);

// Downloads and extracts given beatmapset
// When download/extraction fails, `progress` is -1
void download_beatmapset(u32 set_id, float *progress);

// Downloads given beatmap (unless it already exists)
// When download/extraction fails, `progress` is -1
DatabaseBeatmap *download_beatmap(i32 beatmap_id, MD5Hash beatmap_md5, float *progress);
DatabaseBeatmap *download_beatmap(i32 beatmap_id, i32 beatmapset_id, float *progress);
void process_beatmapset_info_response(const Packet &packet);

i32 extract_beatmapset_id(const u8 *data, size_t data_s);
bool extract_beatmapset(const u8 *data, size_t data_s, std::string &map_dir);

}  // namespace Downloader
