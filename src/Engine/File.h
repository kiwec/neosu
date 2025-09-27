//========== Copyright (c) 2016, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		file wrapper, for cross-platform unicode path support
//
// $NoKeywords: $file
//===============================================================================//

#pragma once
#ifndef FILE_H
#define FILE_H

#include "noinclude.h"
#include "types.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

class ConVar;
class DirectoryCache;

class File {
    NOCOPY_NOMOVE(File)
   public:
    enum class TYPE : uint8_t { READ, WRITE };

    enum class FILETYPE : uint8_t { NONE, FILE, FOLDER, MAYBE_INSENSITIVE, OTHER };

   public:
    File(std::string filePath, TYPE type = TYPE::READ);
    ~File() = default;

    [[nodiscard]] constexpr bool canRead() const {
        return this->bReady && this->ifstream && this->ifstream->good() && this->fileType == TYPE::READ;
    }
    [[nodiscard]] constexpr bool canWrite() const {
        return this->bReady && this->ofstream && this->ofstream->good() && this->fileType == TYPE::WRITE;
    }

    void write(const u8 *buffer, size_t size);
    bool writeLine(std::string_view line, bool insertNewline = true);

    std::string readLine();
    std::string readString();

    const u8 *readFile();  // WARNING: this is NOT a null-terminated string! DO NOT USE THIS with UString/std::string!

    [[nodiscard]] std::vector<u8>
    takeFileBuffer();  // moves the file buffer out, allowing immediate destruction of the file object

    [[nodiscard]] constexpr size_t getFileSize() const { return this->iFileSize; }
    [[nodiscard]] inline std::string getPath() const { return this->sFilePath; }

    // public path resolution methods
    // modifies the input path with the actual found path
    [[nodiscard]] static File::FILETYPE existsCaseInsensitive(std::string &filePath);
    [[nodiscard]] static File::FILETYPE exists(std::string_view filePath);

   private:
    std::string sFilePath;
    TYPE fileType;
    bool bReady;
    size_t iFileSize;

    // file streams
    std::unique_ptr<std::ifstream> ifstream;
    std::unique_ptr<std::ofstream> ofstream;

    // buffer for full file reading
    std::vector<u8> vFullBuffer;

    // private implementation helpers
    bool openForReading();
    bool openForWriting();

    // internal path resolution helpers
    [[nodiscard]] static File::FILETYPE existsCaseInsensitive(std::string &filePath, std::filesystem::path &path);
    [[nodiscard]] static File::FILETYPE exists(std::string_view filePath, const std::filesystem::path &path);

    // directory cache
    static std::unique_ptr<DirectoryCache> s_directoryCache;
};

#endif
