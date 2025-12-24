#pragma once

#include "noinclude.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>

struct archive;
struct archive_entry;

class Archive {
    NOCOPY_NOMOVE(Archive)
   public:
    class Entry {
       public:
        Entry(struct archive* archive, struct archive_entry* entry);
        ~Entry() = default;

        Entry& operator=(const Entry&) = default;
        Entry& operator=(Entry&&) noexcept = default;
        Entry(const Entry&) = default;
        Entry(Entry&&) noexcept = default;

        // entry information
        [[nodiscard]] std::string getFilename() const;
        [[nodiscard]] size_t getUncompressedSize() const;
        [[nodiscard]] size_t getCompressedSize() const;
        [[nodiscard]] bool isDirectory() const;
        [[nodiscard]] bool isFile() const;

        // extraction methods
        [[nodiscard]] const std::vector<u8> &getUncompressedData() const;
        [[nodiscard]] bool extractToFile(const std::string& outputPath) const;

       private:
        std::string sFilename;
        size_t iUncompressedSize;
        size_t iCompressedSize;
        bool bIsDirectory;
        std::vector<u8> data;  // store extracted data
    };

   public:
    // construct from file path
    explicit Archive(const std::string& filePath);

    // construct from memory buffer
    Archive(const u8* data, size_t size);

    ~Archive();

    // check if archive was opened successfully
    [[nodiscard]] bool isValid() const { return this->bValid; }

    // get all entries at once (useful for separating files/dirs)
    std::vector<Entry> getAllEntries();

    // iteration interface
    bool hasNext();
    Entry getCurrentEntry();
    bool moveNext();

    // convenience methods
    Entry* findEntry(const std::string& filename);
    bool extractAll(const std::string& outputDir, const std::vector<std::string>& ignorePaths = {},
                    bool skipDirectories = false);

   private:
    void initFromMemory(const u8* data, size_t size);
    void initFromFile(const std::string& filePath);
    void cleanup();
    static bool createDirectoryRecursive(const std::string& path);
    [[nodiscard]] static bool isPathSafe(const std::string& path);

    struct archive* archive;
    std::vector<u8> vMemoryBuffer;  // keep buffer alive for memory-based archives
    bool bValid;
    bool bIterationStarted;
    std::unique_ptr<Entry> currentEntry;
};
