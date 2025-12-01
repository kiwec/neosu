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

class ConVar;
class DirectoryCache;

class File {
    NOCOPY_NOMOVE(File)
   public:
    enum class MODE : uint8_t { READ, WRITE };

    enum class FILETYPE : uint8_t { NONE, FILE, FOLDER, MAYBE_INSENSITIVE, OTHER };

   public:
    File(std::string_view filePath, MODE mode = MODE::READ);
    ~File() = default;

    [[nodiscard]] constexpr bool canRead() const {
        return this->bReady && this->ifstream && this->ifstream->good() && this->fileMode == MODE::READ;
    }
    [[nodiscard]] constexpr bool canWrite() const {
        return this->bReady && this->ofstream && this->ofstream->good() && this->fileMode == MODE::WRITE;
    }

    void write(const u8 *buffer, uSz size);
    bool writeLine(std::string_view line, bool insertNewline = true);

    std::string readLine();
    std::string readToString();

    // returns actual amount read
    uSz readBytes(uSz start, uSz amount, std::unique_ptr<u8[]> &out);

    // WARNING: this is NOT a null-terminated string! DO NOT USE THIS with UString/std::string!
    const std::unique_ptr<u8[]> &readFile();

    // moves the file buffer out, allowing immediate destruction of the file object
    [[nodiscard]] std::unique_ptr<u8[]> &&takeFileBuffer();

    [[nodiscard]] constexpr uSz getFileSize() const { return this->iFileSize; }
    [[nodiscard]] inline std::string_view getPath() const { return this->sFilePath; }

    // public path resolution methods
    // modifies the input path with the actual found path
    [[nodiscard]] static File::FILETYPE existsCaseInsensitive(std::string &filePath);
    [[nodiscard]] static File::FILETYPE exists(std::string_view filePath);

    // fs::path works differently depending on the type of string it was constructed with
    // so use this to get a unicode-constructed path on windows (convert), utf8 otherwise (passthrough)
    [[nodiscard]] static std::filesystem::path getFsPath(std::string_view utf8path);

    // passthrough to "_wfopen" on Windows, "fopen" otherwise
    [[nodiscard]] static FILE *fopen_c(const char *__restrict utf8filename, const char *__restrict modes);

   private:
    // private implementation helpers
    bool openForReading();
    bool openForWriting();

    // internal path resolution helpers
    [[nodiscard]] static File::FILETYPE existsCaseInsensitive(std::string &filePath, std::filesystem::path &path);
    [[nodiscard]] static File::FILETYPE exists(std::string_view filePath, const std::filesystem::path &path);

    std::string sFilePath;
    std::filesystem::path fsPath;

    // file streams
    std::unique_ptr<std::ifstream> ifstream;
    std::unique_ptr<std::ofstream> ofstream;

    // buffer for full file reading
    std::unique_ptr<u8[]> vFullBuffer;

    uSz iFileSize;

    MODE fileMode;
    bool bReady;
};

#endif
