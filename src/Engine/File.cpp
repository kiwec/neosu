//========== Copyright (c) 2012, PG & 2025, WH, All rights reserved. ============//
//
// purpose:		file wrapper, for cross-platform unicode path support
//
// $NoKeywords: $file
//===============================================================================//

#include "File.h"
#include "ConVar.h"
#include "Engine.h"
#include "UString.h"
#include "Logging.h"
#include "SyncMutex.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdio>

namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// encapsulation of directory caching logic
//------------------------------------------------------------------------------
class DirectoryCache final {
   private:
    struct StringHashNcase {
       private:
        [[nodiscard]] inline size_t hashFunc(std::string_view str) const {
            size_t hash = 0;
            for(auto &c : str) {
                hash = hash * 31 + std::tolower(static_cast<unsigned char>(c));
            }
            return hash;
        }

       public:
        using is_transparent = void;

        size_t operator()(const std::string &s) const { return hashFunc(s); }
        size_t operator()(std::string_view sv) const { return hashFunc(sv); }
    };

    struct StringEqualNcase {
       private:
        [[nodiscard]] inline bool equality(std::string_view lhs, std::string_view rhs) const {
            return std::ranges::equal(
                lhs, rhs, [](unsigned char a, unsigned char b) -> bool { return std::tolower(a) == std::tolower(b); });
        }

       public:
        using is_transparent = void;

        bool operator()(const std::string &lhs, const std::string &rhs) const { return equality(lhs, rhs); }
        bool operator()(const std::string &lhs, std::string_view rhs) const { return equality(lhs, rhs); }
        bool operator()(std::string_view lhs, const std::string &rhs) const { return equality(lhs, rhs); }
    };

    template <typename T>
    using sv_ncase_unordered_map = std::unordered_map<std::string, T, StringHashNcase, StringEqualNcase>;

   public:
    DirectoryCache() = default;

    // directory entry type
    struct DirectoryEntry {
        sv_ncase_unordered_map<std::pair<std::string, File::FILETYPE>> files;
        std::chrono::steady_clock::time_point lastAccess;
        fs::file_time_type lastModified;
    };

    // look up a file with case-insensitive matching
    std::pair<std::string, File::FILETYPE> lookup(const fs::path &dirPath, std::string_view filename) {
        Sync::scoped_lock lock(this->mutex);

        std::string dirKey(dirPath.string());
        auto it = this->cache.find(dirKey);

        DirectoryEntry *entry = nullptr;

        // check if cache exists and is still valid
        if(it != this->cache.end()) {
            // check if directory has been modified
            std::error_code ec;
            auto currentModTime = fs::last_write_time(dirPath, ec);

            if(!ec && currentModTime != it->second.lastModified)
                this->cache.erase(it);  // cache is stale, remove it
            else
                entry = &it->second;
        }

        // create new entry if needed
        if(!entry) {
            // evict old entries if we're at capacity
            if(this->cache.size() >= DIR_CACHE_MAX_ENTRIES) evictOldEntries();

            // build new cache entry
            DirectoryEntry newEntry;
            newEntry.lastAccess = std::chrono::steady_clock::now();

            std::error_code ec;
            newEntry.lastModified = fs::last_write_time(dirPath, ec);

            // scan directory and populate cache
            for(const auto &dirEntry : fs::directory_iterator(dirPath, ec)) {
                if(ec) break;

                std::string filename(dirEntry.path().filename().string());
                File::FILETYPE type = File::FILETYPE::OTHER;

                if(dirEntry.is_regular_file())
                    type = File::FILETYPE::FILE;
                else if(dirEntry.is_directory())
                    type = File::FILETYPE::FOLDER;

                // store both the actual filename and its type
                newEntry.files[filename] = {filename, type};
            }

            // insert into cache
            auto [insertIt, inserted] = this->cache.emplace(dirKey, std::move(newEntry));
            entry = inserted ? &insertIt->second : nullptr;
        }

        if(!entry) return {{}, File::FILETYPE::NONE};

        // update last access time
        entry->lastAccess = std::chrono::steady_clock::now();

        // find the case-insensitive match
        auto fileIt = entry->files.find(filename);
        if(fileIt != entry->files.end()) return fileIt->second;

        return {{}, File::FILETYPE::NONE};
    }

   private:
    static constexpr size_t DIR_CACHE_MAX_ENTRIES = 1000;
    static constexpr size_t DIR_CACHE_EVICT_COUNT = DIR_CACHE_MAX_ENTRIES / 4;

    // evict least recently used entries when cache is full
    void evictOldEntries() {
        const size_t entriesToRemove = std::min(DIR_CACHE_EVICT_COUNT, this->cache.size());

        if(entriesToRemove == this->cache.size()) {
            this->cache.clear();
            return;
        }

        // collect entries with their access times
        std::vector<std::pair<std::chrono::steady_clock::time_point, decltype(this->cache)::iterator>> entries;
        entries.reserve(this->cache.size());

        for(auto it = this->cache.begin(); it != this->cache.end(); ++it)
            entries.emplace_back(it->second.lastAccess, it);

        // sort by access time (oldest first)
        std::ranges::sort(entries, [](const auto &a, const auto &b) { return a.first < b.first; });

        // remove the oldest entries
        for(size_t i = 0; i < entriesToRemove; ++i) this->cache.erase(entries[i].second);
    }

    // cache storage
    std::unordered_map<std::string, DirectoryEntry> cache;

    // thread safety
    Sync::mutex mutex;
};

// init static directory cache
#ifndef MCENGINE_PLATFORM_WINDOWS
std::unique_ptr<DirectoryCache> File::s_directoryCache = std::make_unique<DirectoryCache>();
#else
std::unique_ptr<DirectoryCache> File::s_directoryCache;
#endif

//------------------------------------------------------------------------------
// path resolution methods
//------------------------------------------------------------------------------
// public static
File::FILETYPE File::existsCaseInsensitive(std::string &filePath) {
    if(filePath.empty()) return FILETYPE::NONE;

    auto fsPath = getFsPath(filePath);
    return File::existsCaseInsensitive(filePath, fsPath);
}

File::FILETYPE File::exists(std::string_view filePath) {
    if(filePath.empty()) return FILETYPE::NONE;

    return File::exists(filePath, File::getFsPath(filePath));
}

// private (cache the fs::path)
File::FILETYPE File::existsCaseInsensitive(std::string &filePath, fs::path &path) {
    // windows is already case insensitive
    if constexpr(Env::cfg(OS::WINDOWS)) {
        return File::exists(filePath);
    }

    auto retType = File::exists(filePath, path);

    if(retType == File::FILETYPE::NONE)
        return File::FILETYPE::NONE;
    else if(!(retType == File::FILETYPE::MAYBE_INSENSITIVE))
        return retType;  // direct match

    auto parentPath = path.parent_path();

    // verify parent directory exists
    std::error_code ec;
    auto parentStatus = fs::status(parentPath, ec);
    if(ec || parentStatus.type() != fs::file_type::directory) return File::FILETYPE::NONE;

    // try case-insensitive lookup using cache
    auto [resolvedName, fileType] =
        s_directoryCache->lookup(parentPath, {path.filename().string()});  // takes the bare filename

    if(fileType == File::FILETYPE::NONE) return File::FILETYPE::NONE;  // no match, even case-insensitively

    std::string resolvedPath(parentPath.string());
    if(!(resolvedPath.back() == '/') && !(resolvedPath.back() == '\\')) resolvedPath.push_back('/');
    resolvedPath.append(resolvedName);

    if(cv::debug_file.getBool())
        debugLog("File: Case-insensitive match found for {:s} -> {:s}", path.string(), resolvedPath);

    // now update the input path reference with the actual found path
    filePath = resolvedPath;
    path = fs::path(resolvedPath);
    return fileType;
}

File::FILETYPE File::exists(std::string_view filePath, const fs::path &path) {
    if(filePath.empty()) return File::FILETYPE::NONE;

    std::error_code ec;
    auto status = fs::status(path, ec);

    if(ec || status.type() == fs::file_type::not_found)
        return File::FILETYPE::MAYBE_INSENSITIVE;  // path not found, try case-insensitive lookup

    if(status.type() == fs::file_type::regular)
        return File::FILETYPE::FILE;
    else if(status.type() == fs::file_type::directory)
        return File::FILETYPE::FOLDER;
    else
        return File::FILETYPE::OTHER;
}

// public static helpers
// fs::path works differently depending on the type of string it was constructed with (annoying)
fs::path File::getFsPath(std::string_view utf8path) {
    if(utf8path.empty()) return fs::path{};
#ifdef MCENGINE_PLATFORM_WINDOWS
    const UString filePathUStr{utf8path.data(), static_cast<int>(utf8path.length())};
    return fs::path{filePathUStr.wchar_str()};
#else
    return fs::path{utf8path};
#endif
}

FILE *File::fopen_c(const char *__restrict utf8filename, const char *__restrict modes) {
    if(utf8filename == nullptr || utf8filename[0] == '\0') return nullptr;
#ifdef MCENGINE_PLATFORM_WINDOWS
    const UString wideFilename{utf8filename};
    const UString wideModes{modes};
    return _wfopen(wideFilename.wchar_str(), wideModes.wchar_str());
#else
    return fopen(utf8filename, modes);
#endif
}

//------------------------------------------------------------------------------
// File implementation
//------------------------------------------------------------------------------
File::File(std::string_view filePath, MODE mode)
    : sFilePath(filePath), fsPath(getFsPath(this->sFilePath)), fileMode(mode), bReady(false), iFileSize(0) {
    if(mode == MODE::READ) {
        if(!openForReading()) return;
    } else if(mode == MODE::WRITE) {
        if(!openForWriting()) return;
    }

    if(cv::debug_file.getBool()) debugLog("File: Opening {:s}", this->sFilePath);

    this->bReady = true;
}

bool File::openForReading() {
    // resolve the file path (handles case-insensitive matching)
    auto fileType = File::existsCaseInsensitive(this->sFilePath, this->fsPath);

    if(fileType != File::FILETYPE::FILE) {
        if(cv::debug_file.getBool())
            debugLog("File Error: Path {:s} {:s}", this->sFilePath,
                     fileType == File::FILETYPE::NONE ? "doesn't exist" : "is not a file");
        return false;
    }

    // create and open input file stream
    this->ifstream = std::make_unique<std::ifstream>();
    this->ifstream->open(this->fsPath, std::ios::in | std::ios::binary);

    // check if file opened successfully
    if(!this->ifstream || !this->ifstream->good()) {
        debugLog("File Error: Couldn't open file {:s}", this->sFilePath);
        return false;
    }

    // get file size
    std::error_code ec;
    this->iFileSize = fs::file_size(this->fsPath, ec);

    if(ec) {
        debugLog("File Error: Couldn't get file size for {:s}", this->sFilePath);
        return false;
    }

    // validate file size
    if(this->iFileSize == 0) {  // empty file is valid
        return true;
    } else if(std::cmp_greater(this->iFileSize, 1024 * 1024 * cv::file_size_max.getInt())) {  // size sanity check
        debugLog("File Error: FileSize of {:s} is > {} MB!!!", this->sFilePath, cv::file_size_max.getInt());
        return false;
    }

    return true;
}

bool File::openForWriting() {
    // create parent directories if needed
    if(!this->fsPath.parent_path().empty()) {
        std::error_code ec;
        fs::create_directories(this->fsPath.parent_path(), ec);
        if(ec) {
            debugLog("File Error: Couldn't create parent directories for {:s} (error: {:s})", this->sFilePath,
                     ec.message());
            // continue anyway, the file open might still succeed if the directory exists
        }
    }

    // create and open output file stream
    this->ofstream = std::make_unique<std::ofstream>();
    this->ofstream->open(this->fsPath, std::ios::out | std::ios::trunc | std::ios::binary);

    // check if file opened successfully
    if(!this->ofstream->good()) {
        debugLog("File Error: Couldn't open file {:s} for writing", this->sFilePath);
        return false;
    }

    return true;
}

void File::write(const u8 *buffer, size_t size) {
    if(cv::debug_file.getBool()) debugLog("{:s} (canWrite: {})", this->sFilePath, canWrite());

    if(!canWrite()) return;

    this->ofstream->write(reinterpret_cast<const char *>(buffer), static_cast<std::streamsize>(size));
}

bool File::writeLine(std::string_view line, bool insertNewline) {
    if(!canWrite()) return false;

    // useless...
    if(insertNewline) {
        std::string lineNewline{std::string{line} + '\n'};
        this->ofstream->write(lineNewline.data(), static_cast<std::streamsize>(lineNewline.length()));
    } else {
        this->ofstream->write(line.data(), static_cast<std::streamsize>(line.length()));
    }
    return !this->ofstream->bad();
}

std::string File::readLine() {
    if(!canRead()) return "";

    std::string line;
    if(std::getline(*this->ifstream, line)) {
        // handle CRLF line endings
        if(!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        return line;
    }

    return "";
}

std::string File::readString() {
    const auto size = getFileSize();
    if(size < 1) return "";

    return {reinterpret_cast<const char *>(readFile().get()), size};
}

const std::unique_ptr<u8[]> &File::readFile() {
    if(cv::debug_file.getBool()) debugLog("{:s} (canRead: {})", this->sFilePath, this->bReady && canRead());

    // return cached buffer if already read
    if(!!this->vFullBuffer) return this->vFullBuffer;

    if(!this->bReady || !canRead()) {
        this->vFullBuffer.reset();
        return this->vFullBuffer;
    }

    // allocate buffer for file contents
    this->vFullBuffer = std::make_unique_for_overwrite<u8[]>(this->iFileSize);

    // read entire file
    this->ifstream->seekg(0, std::ios::beg);
    if(this->ifstream->read(reinterpret_cast<char *>(this->vFullBuffer.get()),
                            static_cast<std::streamsize>(this->iFileSize))) {
        return this->vFullBuffer;
    }

    this->vFullBuffer.reset();
    return this->vFullBuffer;
}

std::unique_ptr<u8[]> &&File::takeFileBuffer() {
    if(cv::debug_file.getBool()) debugLog("{:s} (canRead: {})", this->sFilePath, this->bReady && canRead());

    // if buffer is already populated, move it out
    if(!!this->vFullBuffer) return std::move(this->vFullBuffer);

    if(!this->bReady || !this->canRead()) {
        this->vFullBuffer.reset();
        return std::move(this->vFullBuffer);
    }

    // allocate buffer for file contents
    this->vFullBuffer = std::make_unique_for_overwrite<u8[]>(this->iFileSize);

    // read entire file
    this->ifstream->seekg(0, std::ios::beg);
    if(this->ifstream->read(reinterpret_cast<char *>(this->vFullBuffer.get()),
                            static_cast<std::streamsize>(this->iFileSize))) {
        return std::move(this->vFullBuffer);
    }

    // read failed, clear buffer and return empty vector
    this->vFullBuffer.reset();
    return std::move(this->vFullBuffer);
}
