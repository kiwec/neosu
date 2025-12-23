#include "Archival.h"

#include "Environment.h"
#include "SString.h"
#include "Logging.h"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cstring>
#include <fstream>

//------------------------------------------------------------------------------
// Archive::Entry implementation
//------------------------------------------------------------------------------

Archive::Entry::Entry(struct archive* archive, struct archive_entry* entry) {
    if(!entry) {
        // create empty entry
        this->sFilename = "";
        this->iUncompressedSize = 0;
        this->iCompressedSize = 0;
        this->bIsDirectory = false;
        return;
    }

    this->sFilename = archive_entry_pathname(entry);
    this->iUncompressedSize = archive_entry_size(entry);
    this->iCompressedSize = 0;  // libarchive doesn't always provide compressed size
    this->bIsDirectory = archive_entry_filetype(entry) == AE_IFDIR;

    // extract data immediately while archive is positioned correctly
    if(!this->bIsDirectory && archive) {
        if(this->iUncompressedSize > 0) {
            this->data.reserve(this->iUncompressedSize);
        }

        const void* buff;
        size_t size;
        la_int64_t offset;

        int result;
        while((result = archive_read_data_block(archive, &buff, &size, &offset)) == ARCHIVE_OK) {
            const u8* bytes = static_cast<const u8*>(buff);
            this->data.insert(this->data.end(), bytes, bytes + size);
        }

        // check for errors during data extraction
        if(result != ARCHIVE_EOF) {
            debugLog("Archive: failed to extract data for '{:s}': {:s}", this->sFilename.c_str(),
                     archive_error_string(archive));
            // clear any partial data on error
            this->data.clear();
        }
    }
}

std::string Archive::Entry::getFilename() const { return this->sFilename; }

size_t Archive::Entry::getUncompressedSize() const { return this->iUncompressedSize; }

size_t Archive::Entry::getCompressedSize() const { return this->iCompressedSize; }

bool Archive::Entry::isDirectory() const { return this->bIsDirectory; }

bool Archive::Entry::isFile() const { return !this->bIsDirectory; }

const std::vector<u8> &Archive::Entry::getUncompressedData() const { return this->data; }

bool Archive::Entry::extractToFile(const std::string& outputPath) const {
    if(this->bIsDirectory) return false;

    std::ofstream file(outputPath, std::ios::binary);
    if(!file.good()) {
        debugLog("Archive: failed to create output file {:s}", outputPath.c_str());
        return false;
    }

    if(this->data.empty()) {
        debugLog("Archive WARNING: extracted data for path {:s} is empty.", outputPath.c_str());
    }

    file.write(reinterpret_cast<const char*>(this->data.data()), static_cast<std::streamsize>(this->data.size()));
    if(file.bad()) {
        debugLog("Archive: failed to write to file {:s}", outputPath.c_str());
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Archive implementation
//------------------------------------------------------------------------------

Archive::Archive(const std::string& filePath) : archive(nullptr), bValid(false), bIterationStarted(false) {
    initFromFile(filePath);
}

Archive::Archive(const u8* data, size_t size) : archive(nullptr), bValid(false), bIterationStarted(false) {
    initFromMemory(data, size);
}

Archive::~Archive() { cleanup(); }

void Archive::initFromFile(const std::string& filePath) {
    this->archive = archive_read_new();
    if(!this->archive) {
        debugLog("Archive: failed to create archive reader");
        return;
    }

    // enable all supported formats and filters
    archive_read_support_format_all(this->archive);
    archive_read_support_filter_all(this->archive);

    int r = archive_read_open_filename(this->archive, filePath.c_str(), 10240);
    if(r != ARCHIVE_OK) {
        debugLog("Archive: failed to open file {:s}: {:s}", filePath.c_str(), archive_error_string(this->archive));
        cleanup();
        return;
    }

    this->bValid = true;
}

void Archive::initFromMemory(const u8* data, size_t size) {
    if(!data || size == 0) {
        debugLog("Archive: invalid memory buffer");
        return;
    }

    // copy data to our own buffer to ensure it stays alive
    this->vMemoryBuffer.assign(data, data + size);

    this->archive = archive_read_new();
    if(!this->archive) {
        debugLog("Archive: failed to create archive reader");
        return;
    }

    // enable all supported formats and filters
    archive_read_support_format_all(this->archive);
    archive_read_support_filter_all(this->archive);

    int r = archive_read_open_memory(this->archive, this->vMemoryBuffer.data(), this->vMemoryBuffer.size());
    if(r != ARCHIVE_OK) {
        debugLog("Archive: failed to open memory buffer: {:s}", archive_error_string(this->archive));
        cleanup();
        return;
    }

    this->bValid = true;
}

void Archive::cleanup() {
    this->currentEntry.reset();
    if(this->archive) {
        archive_read_free(this->archive);
        this->archive = nullptr;
    }
    this->bValid = false;
    this->bIterationStarted = false;
}

std::vector<Archive::Entry> Archive::getAllEntries() {
    std::vector<Entry> entries;
    if(!this->bValid) return entries;

    // restart iteration if needed
    if(this->bIterationStarted) {
        cleanup();
        if(!this->vMemoryBuffer.empty()) {
            initFromMemory(this->vMemoryBuffer.data(), this->vMemoryBuffer.size());
        } else {
            debugLog("Archive: cannot restart iteration on file-based archive");
            return entries;
        }
    }

    struct archive_entry* entry;
    while(archive_read_next_header(this->archive, &entry) == ARCHIVE_OK) {
        entries.emplace_back(this->archive, entry);
        // skip file data to move to next entry
        archive_read_data_skip(this->archive);
    }

    this->bIterationStarted = true;
    return entries;
}

bool Archive::hasNext() {
    if(!this->bValid) return false;

    if(!this->bIterationStarted) {
        struct archive_entry* entry;
        int r = archive_read_next_header(this->archive, &entry);
        if(r == ARCHIVE_OK) {
            this->currentEntry = std::make_unique<Entry>(this->archive, entry);
            this->bIterationStarted = true;
            return true;
        } else if(r == ARCHIVE_EOF) {
            return false;
        } else {
            debugLog("Archive: error reading next header: {:s}", archive_error_string(this->archive));
            return false;
        }
    }

    return this->currentEntry != nullptr;
}

Archive::Entry Archive::getCurrentEntry() {
    if(!hasNext()) {
        // return empty entry if none available
        struct archive_entry* dummy = archive_entry_new();
        Entry empty(nullptr, dummy);
        archive_entry_free(dummy);
        return empty;
    }

    return *this->currentEntry;
}

bool Archive::moveNext() {
    if(!this->bValid) return false;

    // skip current entry data
    if(this->currentEntry) {
        archive_read_data_skip(this->archive);
        this->currentEntry.reset();
    }

    struct archive_entry* entry;
    int r = archive_read_next_header(this->archive, &entry);
    if(r == ARCHIVE_OK) {
        this->currentEntry = std::make_unique<Entry>(this->archive, entry);
        return true;
    } else if(r == ARCHIVE_EOF) {
        return false;
    } else {
        debugLog("Archive: error reading next header: {:s}", archive_error_string(this->archive));
        return false;
    }
}

Archive::Entry* Archive::findEntry(const std::string& filename) {
    auto entries = getAllEntries();

    auto it =
        std::ranges::find_if(entries, [&filename](const Entry& entry) { return entry.getFilename() == filename; });

    if(it != entries.end()) {
        // note: this returns a pointer to a temporary object
        // caller should use immediately or copy the entry
        static Entry found = *it;
        found = *it;
        return &found;
    }

    return nullptr;
}

bool Archive::extractAll(const std::string& outputDir, const std::vector<std::string>& ignorePaths,
                         bool skipDirectories) {
    if(!this->bValid) return false;

    auto entries = getAllEntries();
    if(entries.empty()) return false;

    // separate directories and files
    std::vector<Entry> directories, files;
    for(const auto& entry : entries) {
        if(entry.isDirectory()) {
            directories.push_back(entry);
        } else {
            files.push_back(entry);
        }
    }

    // create directories first (unless skipping)
    if(!skipDirectories) {
        for(const auto& dir : directories) {
            std::string dirPath = outputDir + "/" + dir.getFilename();

            // check ignore list
            bool shouldIgnore = std::ranges::any_of(ignorePaths, [&dirPath](const std::string& ignorePath) {
                return dirPath.find(ignorePath) != std::string::npos;
            });

            if(shouldIgnore) continue;

            if(!isPathSafe(dir.getFilename())) {
                debugLog("Archive: skipping unsafe directory path {:s}", dir.getFilename().c_str());
                continue;
            }

            createDirectoryRecursive(dirPath);
        }
    }

    // extract files
    for(const auto& file : files) {
        std::string filePath = outputDir + "/" + file.getFilename();

        // check ignore list
        bool shouldIgnore = std::ranges::any_of(ignorePaths, [&filePath](const std::string& ignorePath) {
            return filePath.find(ignorePath) != std::string::npos;
        });

        if(shouldIgnore) {
            debugLog("Archive: ignoring file {:s}", filePath.c_str());
            continue;
        }

        if(!isPathSafe(file.getFilename())) {
            debugLog("Archive: skipping unsafe file path {:s}", file.getFilename().c_str());
            continue;
        }

        // ensure parent directory exists
        auto folders = SString::split(file.getFilename(), '/');
        std::string currentPath = outputDir;
        for(size_t i = 0; i < folders.size() - 1; i++) {
            currentPath = fmt::format("{}/{}", currentPath, folders[i]);
            createDirectoryRecursive(currentPath);
        }

        if(!file.extractToFile(filePath)) {
            debugLog("Archive: failed to extract file {:s}", filePath.c_str());
            return false;
        }
    }

    return true;
}

bool Archive::createDirectoryRecursive(const std::string& path) {
    if(env->directoryExists(path)) return true;

    return env->createDirectory(path);
}

bool Archive::isPathSafe(const std::string& path) {
    // check for path traversal attempts
    return path.find("..") == std::string::npos;
}
