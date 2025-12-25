// Copyright (c) 2014, PG, All rights reserved.
#include "Sound.h"

#include "BassSound.h"
#include "SoLoudSound.h"

#include "ConVar.h"
#include "Environment.h"
#include "File.h"
#include "ResourceManager.h"
#include "SoundEngine.h"
#include "SString.h"
#include "Logging.h"

#include <memory>
#include <utility>

void Sound::initAsync() {
    if(!this->sRebuildFilePath.empty()) {
        this->sFilePath = std::move(this->sRebuildFilePath);
        this->sRebuildFilePath.clear();
        this->doPathFixup();
    }

    logIfCV(debug_rm, "Resource Manager: Loading {:s}", this->sFilePath);

    // sanity check for malformed audio files
    std::string fileExtensionLowerCase{SString::to_lower(env->getFileExtensionFromFilePath(this->sFilePath))};

    if(this->sFilePath.empty() || fileExtensionLowerCase.empty()) {
        this->bIgnored = true;
    } else if(!this->isValidAudioFile(this->sFilePath, fileExtensionLowerCase)) {
        if(!cv::snd_force_load_unknown.getBool()) {
            debugLog("Sound: Ignoring malformed/corrupt .{:s} file {:s}", fileExtensionLowerCase, this->sFilePath);
            this->bIgnored = true;
        } else {
            logIfCV(debug_snd,
                    "Sound: snd_force_load_unknown=true, loading what seems to be a malformed/corrupt .{:s} file "
                    "{:s}",
                    fileExtensionLowerCase, this->sFilePath);
            this->bIgnored = false;
        }
    } else {
        this->bIgnored = false;
    }
}

void Sound::rebuild(std::string_view newFilePath, bool async) {
    if(!newFilePath.empty()) {
        this->sRebuildFilePath = newFilePath;
    }

    resourceManager->reloadResource(this, async);
}

Sound* Sound::createSound(std::string filepath, bool stream, bool overlayable, bool loop) {
#if !defined(MCENGINE_FEATURE_BASS) && !defined(MCENGINE_FEATURE_SOLOUD)
#error No sound backend available!
#endif
#ifdef MCENGINE_FEATURE_BASS
    if(soundEngine->getTypeId() == BASS) return new BassSound(std::move(filepath), stream, overlayable, loop);
#endif
#ifdef MCENGINE_FEATURE_SOLOUD
    if(soundEngine->getTypeId() == SOLOUD) return new SoLoudSound(std::move(filepath), stream, overlayable, loop);
#endif
    return nullptr;
}

// quick heuristic to check if it's going to be worth loading the audio
bool Sound::isValidAudioFile(std::string_view filePath, std::string_view fileExt) {
    File testFile(filePath);

    if(!testFile.canRead()) return false;

    size_t fileSize = testFile.getFileSize();

    // account for larger flac header
    size_t minSize = fileExt == "flac" ? std::max<size_t>(cv::snd_file_min_size.getVal<size_t>(), 96)
                                       : cv::snd_file_min_size.getVal<size_t>();

    if(fileExt == "wav" || fileExt == "mp3" || fileExt == "ogg" || fileExt == "flac") {
        return fileSize >= minSize;
    }

    return false;  // don't let unsupported formats be read
}

const std::unordered_map<SOUNDHANDLE, PlaybackParams>& Sound::getActiveHandles() {
    // update cache with actual validity from backend
    std::erase_if(this->activeHandleCache,
                  [this](const auto& handleInstance) { return !this->isHandleValid(handleInstance.first); });
    return this->activeHandleCache;
}

void Sound::setBaseVolume(float volume) {
    this->fBaseVolume = std::clamp<float>(volume, 0.0f, 2.0f);

    // propagate the changed volume to the active voice handles
    for(const auto& [handle, instance] : this->getActiveHandles()) {
        const auto& vol = instance.volume;
        this->setHandleVolume(handle, this->fBaseVolume * vol);
    }
}
