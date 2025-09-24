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
#include "Sync.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// encapsulation of directory caching logic
//------------------------------------------------------------------------------
class DirectoryCache final {
   public:
    DirectoryCache() = default;

    // case-insensitive string utilities
    struct CaseInsensitiveHash {
        size_t operator()(const std::string &str) const {
            size_t hash = 0;
            for(auto &c : str) {
                hash = hash * 31 + std::tolower(c);
            }
            return hash;
        }
    };

    struct CaseInsensitiveEqual {
        bool operator()(const std::string &lhs, const std::string &rhs) const {
            return std::ranges::equal(lhs, rhs, [](char a, char b) -> bool {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
        }
    };

    // directory entry type
    struct DirectoryEntry {
        std::unordered_map<std::string, std::pair<std::string, File::FILETYPE>, CaseInsensitiveHash,
                           CaseInsensitiveEqual>
            files;
        std::chrono::steady_clock::time_point lastAccess;
        fs::file_time_type lastModified;
    };

    // look up a file with case-insensitive matching
    std::pair<std::string, File::FILETYPE> lookup(const fs::path &dirPath, const std::string &filename) {
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
// public
File::FILETYPE File::existsCaseInsensitive(std::string &filePath) {
    if(filePath.empty()) return FILETYPE::NONE;

    UString filePathUStr{filePath};
    auto fsPath = fs::path(filePathUStr.plat_str());

    return existsCaseInsensitive(filePath, fsPath);
}

File::FILETYPE File::exists(const std::string &filePath) {
    if(filePath.empty()) return FILETYPE::NONE;

    UString filePathUStr{filePath};
    const auto fsPath = fs::path(filePathUStr.plat_str());
    return exists(filePath, fsPath);
}

// private (cache the fs::path)
File::FILETYPE File::existsCaseInsensitive(std::string &filePath, fs::path &path) {
    auto retType = File::exists(filePath, path);

    if(retType == File::FILETYPE::NONE)
        return File::FILETYPE::NONE;
    else if(!(retType == File::FILETYPE::MAYBE_INSENSITIVE))
        return retType;  // direct match

    // no point in continuing, windows is already case insensitive
    if constexpr(Env::cfg(OS::WINDOWS)) return File::FILETYPE::NONE;

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
    if(!(resolvedPath.back() == '/') && !(resolvedPath.back() == '\\')) resolvedPath.append("/");
    resolvedPath.append(resolvedName);

    if(cv::debug_file.getBool())
        debugLog("File: Case-insensitive match found for {:s} -> {:s}\n", path.string(), resolvedPath);

    // now update the given paths with the actual found path
    filePath = resolvedPath;
    UString filePathUStr{filePath};
    path = fs::path(filePathUStr.plat_str());
    return fileType;
}

File::FILETYPE File::exists(const std::string &filePath, const fs::path &path) {
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

//------------------------------------------------------------------------------
// File implementation
//------------------------------------------------------------------------------
File::File(std::string filePath, TYPE type) : sFilePath(filePath), fileType(type), bReady(false), iFileSize(0) {
    if(type == TYPE::READ) {
        if(!openForReading()) return;
    } else if(type == TYPE::WRITE) {
        if(!openForWriting()) return;
    }

    if(cv::debug_file.getBool()) debugLog("File: Opening {:s}\n", filePath);

    this->bReady = true;
}

bool File::openForReading() {
    // resolve the file path (handles case-insensitive matching)
    UString filePathUStr{this->sFilePath};
    fs::path path(filePathUStr.plat_str());
    auto fileType = existsCaseInsensitive(this->sFilePath, path);

    if(fileType != File::FILETYPE::FILE) {
        if(cv::debug_file.getBool())
            debugLog("File Error: Path {:s} {:s}\n", this->sFilePath,
                     fileType == File::FILETYPE::NONE ? "doesn't exist" : "is not a file");
        return false;
    }

    // create and open input file stream
    this->ifstream = std::make_unique<std::ifstream>();
    this->ifstream->open(path, std::ios::in | std::ios::binary);

    // check if file opened successfully
    if(!this->ifstream || !this->ifstream->good()) {
        debugLog("File Error: Couldn't open file {:s}\n", this->sFilePath);
        return false;
    }

    // get file size
    std::error_code ec;
    this->iFileSize = fs::file_size(path, ec);

    if(ec) {
        debugLog("File Error: Couldn't get file size for {:s}\n", this->sFilePath);
        return false;
    }

    // validate file size
    if(this->iFileSize == 0) {  // empty file is valid
        return true;
    } else if(std::cmp_greater(this->iFileSize, 1024 * 1024 * cv::file_size_max.getInt())) {  // size sanity check
        debugLog("File Error: FileSize of {:s} is > {} MB!!!\n", this->sFilePath, cv::file_size_max.getInt());
        return false;
    }

    return true;
}

bool File::openForWriting() {
    // get filesystem path
    UString filePathUStr{this->sFilePath};
    fs::path path(filePathUStr.plat_str());

    // create parent directories if needed
    if(!path.parent_path().empty()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if(ec) {
            debugLog("File Error: Couldn't create parent directories for {:s} (error: {:s})\n", this->sFilePath,
                     ec.message());
            // continue anyway, the file open might still succeed if the directory exists
        }
    }

    // create and open output file stream
    this->ofstream = std::make_unique<std::ofstream>();
    this->ofstream->open(path, std::ios::out | std::ios::trunc | std::ios::binary);

    // check if file opened successfully
    if(!this->ofstream->good()) {
        debugLog("File Error: Couldn't open file {:s} for writing\n", this->sFilePath);
        return false;
    }

    return true;
}

void File::write(const u8 *buffer, size_t size) {
    if(cv::debug_file.getBool()) debugLog("{:s} (canWrite: {})\n", this->sFilePath, canWrite());

    if(!canWrite()) return;

    this->ofstream->write(reinterpret_cast<const char *>(buffer), static_cast<std::streamsize>(size));
}

bool File::writeLine(const std::string &line, bool insertNewline) {
    if(!canWrite()) return false;

    std::string writeLine{line};
    if(insertNewline) writeLine = writeLine + "\n";
    this->ofstream->write(writeLine.c_str(), static_cast<std::streamsize>(writeLine.length()));
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

        return {line.c_str()};
    }

    return "";
}

std::string File::readString() {
    const auto size = getFileSize();
    if(size < 1) return "";

    return {reinterpret_cast<const char *>(readFile()), size};
}

const u8 *File::readFile() {
    if(cv::debug_file.getBool()) debugLog("{:s} (canRead: {})\n", this->sFilePath, this->bReady && canRead());

    // return cached buffer if already read
    if(!this->vFullBuffer.empty()) return this->vFullBuffer.data();

    if(!this->bReady || !canRead()) return nullptr;

    // allocate buffer for file contents
    this->vFullBuffer.resize(this->iFileSize);

    // read entire file
    this->ifstream->seekg(0, std::ios::beg);
    if(this->ifstream->read(reinterpret_cast<char *>(this->vFullBuffer.data()),
                            static_cast<std::streamsize>(this->iFileSize)))
        return this->vFullBuffer.data();

    return nullptr;
}

std::vector<u8> File::takeFileBuffer() {
    if(cv::debug_file.getBool()) debugLog("{:s} (canRead: {})\n", this->sFilePath, this->bReady && canRead());

    // if buffer is already populated, move it out
    if(!this->vFullBuffer.empty()) return std::move(this->vFullBuffer);

    if(!this->bReady || !this->canRead()) return {};

    // allocate buffer for file contents
    this->vFullBuffer.resize(this->iFileSize);

    // read entire file
    this->ifstream->seekg(0, std::ios::beg);
    if(this->ifstream->read(reinterpret_cast<char *>(this->vFullBuffer.data()),
                            static_cast<std::streamsize>(this->iFileSize)))
        return std::move(this->vFullBuffer);

    // read failed, clear buffer and return empty vector
    this->vFullBuffer.clear();
    return {};
}
