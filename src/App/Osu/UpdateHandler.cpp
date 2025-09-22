// Copyright (c) 2016, PG, All rights reserved.
#include "UpdateHandler.h"

#include "Archival.h"
#include "BanchoNetworking.h"
#include "ConVar.h"
#include "crypto.h"
#include "Engine.h"
#include "File.h"
#include "NetworkHandler.h"
#include "SString.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Logging.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <fstream>

using enum UpdateHandler::STATUS;

void UpdateHandler::onBleedingEdgeChanged(float oldVal, float newVal) {
    if(this->getStatus() != STATUS_IDLE && this->getStatus() != STATUS_ERROR) {
        debugLog("Can't change release stream while an update is in progress!\n");
        cv::bleedingedge.setValue(oldVal, false);
    }

    const bool oldState = !!static_cast<int>(oldVal);
    const bool newState = !!static_cast<int>(newVal);
    if(oldState == newState) return;

    this->checkForUpdates(true);
}

void UpdateHandler::checkForUpdates(bool force_update) {
    if(this->getStatus() != STATUS_IDLE && this->getStatus() != STATUS_ERROR) {
        debugLog("We're already updating!\n");
        return;
    }

    UString versionUrl = "https://" NEOSU_DOMAIN;
    if(cv::bleedingedge.getBool()) {
        versionUrl.append("/bleedingedge/" OS_NAME ".txt");
    } else {
        versionUrl.append("/update/" OS_NAME "/latest-version.txt");
    }

    debugLog("UpdateHandler: Checking for a newer version from {}\n", versionUrl);
    NetworkHandler::RequestOptions options;
    options.timeout = 10;
    options.connectTimeout = 5;

    this->status = STATUS_CHECKING_FOR_UPDATE;
    networkHandler->httpRequestAsync(
        versionUrl,
        [this, force_update](const NetworkHandler::Response &response) {
            this->onVersionCheckComplete(response.body, response.success, force_update);
        },
        options);
}

void UpdateHandler::onVersionCheckComplete(const std::string &response, bool success, bool force_update) {
    if(!success || response.empty()) {
        // Avoid setting STATUS_ERROR, since we don't want a big red button to show up for offline players
        // We DO want it to show up if the update check was requested manually.
        this->status = force_update ? STATUS_ERROR : STATUS_IDLE;
        debugLog("UpdateHandler ERROR: Failed to check for updates :/\n");
        return;
    }

    auto lines = SString::split(response, "\n");
    f32 latest_version = strtof(lines[0].c_str(), nullptr);
    u64 latest_build_tms = 0;
    std::string online_update_hash;
    if(lines.size() > 1) latest_build_tms = std::strtoull(lines[1].c_str(), nullptr, 10);
    if(lines.size() > 2) online_update_hash = lines[2];
    if(latest_version == 0.f && latest_build_tms == 0) {
        this->status = force_update ? STATUS_ERROR : STATUS_IDLE;
        debugLog("UpdateHandler ERROR: Failed to parse version number\n");
        return;
    }

    u64 current_build_tms = cv::build_timestamp.getVal<u64>();
    bool should_update =
        force_update || (cv::version.getFloat() < latest_version) || (current_build_tms < latest_build_tms);
    if(!should_update) {
        // We're already up to date
        this->status = STATUS_IDLE;
        debugLog("UpdateHandler: We're already up to date (current v{:.2f} ({:d}), latest v{:.2f} ({:d}))\n",
                 cv::version.getFloat(), current_build_tms, latest_version, latest_build_tms);
        return;
    }

    // XXX: Blocking file read
    if(!online_update_hash.empty() && env->fileExists("update.zip")) {
        std::array<u8, 32> file_hash{};
        crypto::hash::sha256_f("update.zip", file_hash.data());
        auto downloaded_update_hash = crypto::conv::encodehex(file_hash);
        if(downloaded_update_hash == online_update_hash) {
            debugLog("UpdateHandler: Update already downloaded (hash = {})", downloaded_update_hash);
            this->status = STATUS_DOWNLOAD_COMPLETE;
            return;
        }
    }

    UString update_url;
    if(cv::bleedingedge.getBool()) {
        update_url = "https://" NEOSU_DOMAIN "/bleedingedge/" OS_NAME ".zip";
    } else {
        update_url = UString::format("https://" NEOSU_DOMAIN "/update/" OS_NAME "/v%.2f.zip", latest_version);
    }
    update_url.append(UString::format("?hash=%s", online_update_hash.c_str()));

    debugLog("UpdateHandler: Downloading latest update... (current v{:.2f} ({:d}), latest v{:.2f} ({:d}))\n",
             cv::version.getFloat(), current_build_tms, latest_version, latest_build_tms);
    debugLog("UpdateHandler: Downloading {:s}\n", update_url.toUtf8());
    NetworkHandler::RequestOptions options;
    options.timeout = 300;  // 5 minutes for large downloads
    options.connectTimeout = 10;
    options.followRedirects = true;

    this->status = STATUS_DOWNLOADING_UPDATE;
    networkHandler->httpRequestAsync(
        update_url,
        [this, online_update_hash](const NetworkHandler::Response &response) {
            this->onDownloadComplete(response.body, response.success, online_update_hash);
        },
        options);
}

void UpdateHandler::onDownloadComplete(const std::string &data, bool success, std::string hash) {
    if(!success || data.size() < 2) {
        debugLog("UpdateHandler ERROR: downloaded file is too small or failed ({:d} bytes)!\n", data.size());
        this->status = STATUS_ERROR;
        return;
    }

    std::array<u8, 32> file_hash{};
    crypto::hash::sha256(data.data(), data.size(), file_hash.data());
    auto downloaded_update_hash = crypto::conv::encodehex(file_hash);
    if(!hash.empty() && downloaded_update_hash != hash) {
        debugLog("UpdateHandler ERROR: downloaded file hash does not match! {} != {}\n", downloaded_update_hash, hash);
        this->status = STATUS_ERROR;
        return;
    }

    // write to disk
    debugLog("UpdateHandler: Downloaded file has {:d} bytes, writing ...\n", data.size());
    std::ofstream file("update.zip", std::ios::out | std::ios::binary);
    if(!file.good()) {
        debugLog("UpdateHandler ERROR: Can't write file!\n");
        this->status = STATUS_ERROR;
        return;
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.close();

    debugLog("UpdateHandler: Update finished successfully.\n");
    this->status = STATUS_DOWNLOAD_COMPLETE;
}

void UpdateHandler::installUpdate() {
    debugLog("UpdateHandler: installing\n");
    Archive archive("update.zip");
    if(!archive.isValid()) {
        debugLog("UpdateHandler ERROR: couldn't open archive!\n");
        this->status = STATUS_ERROR;
        return;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog("UpdateHandler ERROR: archive is empty!\n");
        this->status = STATUS_ERROR;
        return;
    }

    // separate raw dirs and files
    std::string mainDirectory = "neosu/";
    std::vector<Archive::Entry> files, dirs;
    for(const auto &entry : entries) {
        auto fileName = entry.getFilename();
        if(!fileName.starts_with(mainDirectory)) {
            debugLog("UpdateHandler WARNING: Ignoring \"{:s}\" because it's not in the main dir!\n", fileName.c_str());
            continue;
        }

        if(entry.isDirectory()) {
            dirs.push_back(entry);
        } else {
            files.push_back(entry);
        }

        debugLog("UpdateHandler: Filename: \"{:s}\", isDir: {}, uncompressed size: {}\n", entry.getFilename().c_str(),
                 entry.isDirectory(), entry.getUncompressedSize());
    }

    // repair/create missing/new dirs
    for(const auto &dir : dirs) {
        std::string newDir = dir.getFilename().substr(mainDirectory.length());
        if(newDir.length() == 0) continue;
        if(env->directoryExists(newDir)) continue;

        debugLog("UpdateHandler: Creating directory {:s}\n", newDir.c_str());
        env->createDirectory(newDir);
    }

    // extract and overwrite almost everything
    for(const auto &file : files) {
        std::string outFilePath = file.getFilename().substr(mainDirectory.length());
        if(outFilePath.length() == 0) continue;

        // .exe and .dll can't be directly overwritten on windows
        std::string old_path{};
        bool temp_created = false;
        if(outFilePath.length() > 4) {
            if(!strcasecmp(outFilePath.c_str() + outFilePath.length() - 4, ".exe") ||
               !strcasecmp(outFilePath.c_str() + outFilePath.length() - 4, ".dll")) {
                old_path = outFilePath;
                old_path.append(".old");
                env->deleteFile(old_path);
                env->renameFile(outFilePath, old_path);
                temp_created = true;
            }
        }

        debugLog("UpdateHandler: Writing {:s}\n", outFilePath.c_str());
        if(!file.extractToFile(outFilePath)) {
            debugLog("UpdateHandler: Failed to extract file {:s}\n", outFilePath.c_str());
            if(temp_created) {
                env->deleteFile(outFilePath);
                env->renameFile(old_path, outFilePath);
            }
            this->status = STATUS_ERROR;
            return;
        }
    }

    cv::is_bleedingedge.setValue(cv::bleedingedge.getBool());
    osu->getOptionsMenu()->save();

    // we're done updating; restart the game, since the user already clicked to update
    engine->restart();
}
