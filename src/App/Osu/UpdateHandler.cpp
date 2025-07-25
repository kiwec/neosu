#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "Archival.h"
#include "BanchoNetworking.h"
#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "NetworkHandler.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "UpdateHandler.h"

UpdateHandler::UpdateHandler() {
    this->update_url = "";
    this->status = cv::auto_update.getBool() ? STATUS::STATUS_CHECKING_FOR_UPDATE : STATUS::STATUS_UP_TO_DATE;
    this->iNumRetries = 0;
}

void UpdateHandler::checkForUpdates() {
    if(!cv::auto_update.getBool() || cv::debug.getBool()) return;

    if(this->iNumRetries > 0) {
        debugLog("UpdateHandler: update check retry %i ...\n", this->iNumRetries);
    }

    this->requestUpdate();
}

void UpdateHandler::requestUpdate() {
    this->status = STATUS::STATUS_CHECKING_FOR_UPDATE;

    UString versionUrl = "https://" NEOSU_DOMAIN "/update/" OS_NAME "/latest-version.txt";

    debugLogF("UpdateHandler: Checking for a newer version from {}\n", versionUrl);

    NetworkHandler::RequestOptions options;
    options.timeout = 10;
    options.connectTimeout = 5;

    networkHandler->httpRequestAsync(
        versionUrl,
        [this](const NetworkHandler::Response &response) {
            this->onVersionCheckComplete(response.body, response.success);
        },
        options);
}

void UpdateHandler::onVersionCheckComplete(const std::string &response, bool success) {
    if(!success || response.empty()) {
        this->status = STATUS::STATUS_UP_TO_DATE;
        debugLog("UpdateHandler ERROR: Failed to check for updates :/\n");
        return;
    }

    float latest_version = strtof(response.c_str(), NULL);

    [[maybe_unused]] float latest_build_timestamp = 0.0f;

    if(latest_version == 0.f) {
        this->status = STATUS::STATUS_UP_TO_DATE;
        debugLog("UpdateHandler ERROR: Failed to parse version number\n");
        return;
    }

    float current_version = cv::version.getFloat();

    bool should_update = false;

    if constexpr(Env::cfg(STREAM::EDGE)) {
        float current_build_timestamp = cv::build_timestamp.getFloat();
        should_update = current_version < latest_version || (current_build_timestamp < latest_build_timestamp);
    } else {
        should_update = current_version < latest_version;
    }

    if(!should_update) {
        // We're already up to date
        this->status = STATUS::STATUS_UP_TO_DATE;
        debugLog("UpdateHandler: We're already up to date (current v%.2f, latest v%.2f)\n", current_version,
                 latest_version);
        return;
    }

    debugLog("UpdateHandler: Downloading latest update... (current v%.2f, latest v%.2f)\n", current_version,
             latest_version);
    this->update_url = UString::format("https://" NEOSU_DOMAIN "/update/" OS_NAME "/v%.2f.zip", latest_version);

    this->downloadUpdate();
}

void UpdateHandler::downloadUpdate() {
    UString url = this->update_url;

    debugLog("UpdateHandler: Downloading URL %s\n", url.toUtf8());
    this->status = STATUS::STATUS_DOWNLOADING_UPDATE;

    NetworkHandler::RequestOptions options;
    options.timeout = 300;  // 5 minutes for large downloads
    options.connectTimeout = 10;
    options.followRedirects = true;

    networkHandler->httpRequestAsync(
        url,
        [this](const NetworkHandler::Response &response) { this->onDownloadComplete(response.body, response.success); },
        options);
}

void UpdateHandler::onDownloadComplete(const std::string &data, bool success) {
    if(!success || data.length() < 2) {
        debugLog("UpdateHandler ERROR: downloaded file is too small or failed (%zu bytes)!\n", data.length());
        this->status = STATUS::STATUS_ERROR;

        // retry logic
        this->iNumRetries++;
        if(this->iNumRetries < 4) {
            this->checkForUpdates();
        }
        return;
    }

    // write to disk
    debugLog("UpdateHandler: Downloaded file has %zu bytes, writing ...\n", data.length());
    std::ofstream file(TEMP_UPDATE_DOWNLOAD_FILEPATH, std::ios::out | std::ios::binary);
    if(file.good()) {
        file.write(data.data(), static_cast<std::streamsize>(data.length()));
        file.close();

        debugLog("UpdateHandler: Update finished successfully.\n");
        this->installUpdate(TEMP_UPDATE_DOWNLOAD_FILEPATH);
    } else {
        debugLog("UpdateHandler ERROR: Can't write file!\n");
        this->status = STATUS::STATUS_ERROR;

        // retry logic
        this->iNumRetries++;
        if(this->iNumRetries < 4) {
            this->checkForUpdates();
        }
    }
}

void UpdateHandler::installUpdate(const std::string &zipFilePath) {
    debugLog("UpdateHandler: installing %s\n", zipFilePath.c_str());
    this->status = STATUS::STATUS_INSTALLING_UPDATE;

    if(!env->fileExists(zipFilePath)) {
        debugLog("UpdateHandler ERROR: \"%s\" does not exist!\n", zipFilePath.c_str());
        this->status = STATUS::STATUS_ERROR;
        return;
    }

    Archive archive(zipFilePath);
    if(!archive.isValid()) {
        debugLog("UpdateHandler ERROR: couldn't open archive!\n");
        this->status = STATUS::STATUS_ERROR;
        return;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog("UpdateHandler ERROR: archive is empty!\n");
        this->status = STATUS::STATUS_ERROR;
        return;
    }

    // find main directory (assuming first entry is the main subdirectory)
    std::string mainDirectory;
    if(!entries.empty() && entries[0].isDirectory()) {
        mainDirectory = entries[0].getFilename();
    } else {
        debugLog("UpdateHandler ERROR: first entry is not the main directory!\n");
        this->status = STATUS::STATUS_ERROR;
        return;
    }

    // separate raw dirs and files
    std::vector<Archive::Entry> files, dirs;
    for(const auto &entry : entries) {
        if(entry.isDirectory()) {
            dirs.push_back(entry);
        } else {
            files.push_back(entry);
        }

        debugLog("UpdateHandler: Filename: \"%s\", isDir: %i, uncompressed size: %u\n", entry.getFilename().c_str(),
                 (int)entry.isDirectory(), (unsigned int)entry.getUncompressedSize());
    }

    // repair/create missing/new dirs
    std::string cfgDir = MCENGINE_DATA_DIR "cfg" PREF_PATHSEP "";
    bool cfgDirExists = env->directoryExists(cfgDir);
    for(const auto &dir : dirs) {
        std::string dirName = dir.getFilename();
        size_t mainDirectoryOffset = dirName.find(mainDirectory);
        if(mainDirectoryOffset == 0 && dirName.length() - mainDirectoryOffset > 0 &&
           mainDirectoryOffset + mainDirectory.length() < dirName.length()) {
            std::string newDir = dirName.substr(mainDirectoryOffset + mainDirectory.length());

            if(!env->directoryExists(newDir)) {
                debugLog("UpdateHandler: Creating directory %s\n", newDir.c_str());
                env->createDirectory(newDir);
            }
        }
    }

    // extract and overwrite almost everything
    for(const auto &file : files) {
        std::string fileName = file.getFilename();

        // ignore cfg directory (don't want to overwrite user settings), except if it doesn't exist
        if(fileName.find(cfgDir) != std::string::npos && cfgDirExists) {
            debugLog("UpdateHandler: Ignoring file \"%s\"\n", fileName.c_str());
            continue;
        }

        size_t mainDirectoryOffset = fileName.find(mainDirectory);
        if(mainDirectoryOffset == 0 && fileName.length() - mainDirectoryOffset > 0 &&
           mainDirectoryOffset + mainDirectory.length() < fileName.length()) {
            std::string outFilePath = fileName.substr(mainDirectoryOffset + mainDirectory.length());

            // .exe and .dll can't be directly overwritten on windows
            if(outFilePath.length() > 4) {
                if(!strcmp(outFilePath.c_str() + outFilePath.length() - 4, ".exe") ||
                   !strcmp(outFilePath.c_str() + outFilePath.length() - 4, ".dll")) {
                    std::string old_path = outFilePath;
                    old_path.append(".old");
                    env->deleteFile(old_path);
                    env->renameFile(outFilePath, old_path);
                }
            }

            debugLog("UpdateHandler: Writing %s\n", outFilePath.c_str());
            if(!file.extractToFile(outFilePath)) {
                debugLog("UpdateHandler: Failed to extract file %s\n", outFilePath.c_str());
            }
        } else if(mainDirectoryOffset != 0) {
            debugLog("UpdateHandler WARNING: Ignoring file \"%s\" because it's not in the main dir!\n",
                     fileName.c_str());
        }
    }

    this->status = STATUS::STATUS_SUCCESS_INSTALLATION;
    env->deleteFile(zipFilePath);
}
