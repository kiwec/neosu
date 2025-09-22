// Copyright (c) 2014, PG, All rights reserved.
#include "Sound.h"

#include "BassSound.h"
#include "SoLoudSound.h"

#include "ConVar.h"
#include "Environment.h"
#include "ByteBufferedFile.h"
#include "ResourceManager.h"
#include "SoundEngine.h"
#include "SString.h"
#include "Logging.h"

#include <memory>
#include <utility>

void Sound::initAsync() {
    if(cv::debug_rm.getBool()) debugLog("Resource Manager: Loading {:s}\n", this->sFilePath);

    // sanity check for malformed audio files
    std::string fileExtensionLowerCase{SString::lower(env->getFileExtensionFromFilePath(this->sFilePath))};

    if(this->sFilePath.empty() || fileExtensionLowerCase.empty()) {
        this->bIgnored = true;
    } else if(!this->isValidAudioFile(this->sFilePath, fileExtensionLowerCase)) {
        if(!cv::snd_force_load_unknown.getBool()) {
            debugLog("Sound: Ignoring malformed/corrupt .{:s} file {:s}\n", fileExtensionLowerCase, this->sFilePath);
            this->bIgnored = true;
        } else {
            debugLog(
                "Sound: snd_force_load_unknown=true, loading what seems to be a malformed/corrupt .{:s} file {:s}\n",
                fileExtensionLowerCase, this->sFilePath);
            this->bIgnored = false;
        }
    } else {
        this->bIgnored = false;
    }
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

// quick heuristic to check if it's going to be worth loading the audio,
// tolerate some junk data at the start of the files but check for valid headers
bool Sound::isValidAudioFile(const std::string& filePath, const std::string& fileExt) {
    ByteBufferedFile::Reader reader(UString{filePath});

    if(!reader.good()) return false;

    size_t fileSize = reader.total_size;

    if(fileExt == "wav") {
        if(fileSize < cv::snd_file_min_size.getVal<size_t>()) return false;

        // read first 512 bytes or entire file if smaller
        constexpr size_t max_search = 512;
        size_t bytes_to_read = std::min(fileSize, max_search);
        if(bytes_to_read < 12) return false;  // need at least RIFF(4) + size(4) + WAVE(4)

        std::vector<u8> buffer(bytes_to_read);
        if(reader.read_bytes(buffer.data(), bytes_to_read) != bytes_to_read) return false;

        const char* data = reinterpret_cast<const char*>(buffer.data());

        // check immediate RIFF header
        if(memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0) return true;

        // search for RIFF header with minimal tolerance
        for(size_t i = 1; i + 11 < bytes_to_read; i++) {
            if(memcmp(data + i, "RIFF", 4) == 0 && memcmp(data + i + 8, "WAVE", 4) == 0) return true;
        }
        return false;

    } else if(fileExt == "mp3") {  // mostly taken from ffmpeg/libavformat/mp3dec.c mp3_read_header()
        if(fileSize < cv::snd_file_min_size.getVal<size_t>()) return false;

        // only need to check first 3 bytes for ID3v2 tag
        u8 header[3];
        if(reader.read_bytes(header, 3) == 3) {
            // quick check for ID3v2 tag at start
            if(memcmp(header, "ID3", 3) == 0) return true;
        }

        // // search through first 2KB for MP3 sync with basic validation
        // size_t searchLimit = (fileSize < 2048) ? fileSize - 4 : 2048;
        // for (size_t i = 0; i < searchLimit; i++)
        // {
        // 	if (i + 3 < fileSize && (unsigned char)data[i] == 0xFF && ((unsigned char)data[i + 1] & 0xF8) == 0xF8)
        // 	{
        // 		// basic frame header validation
        // 		auto byte2 = (unsigned char)data[i + 1];
        // 		auto byte3 = (unsigned char)data[i + 2];

        // 		// check for reserved/invalid values
        // 		if ((byte2 & 0x18) != 0x08 && // MPEG version not reserved
        // 		    (byte2 & 0x06) != 0x00 && // layer not reserved
        // 		    (byte3 & 0xF0) != 0xF0 && // bitrate not invalid
        // 		    (byte3 & 0x0C) != 0x0C)   // sample rate not reserved
        // 			return true;
        // 	}
        // }
        // this is stupid actually, there's a lot of cases where a file has a .mp3 extension but contains AAC data, so
        // just return valid if it's big enough since SoLoud (+ffmpeg) and BASS (windows) can decode them
        return true;

    } else if(fileExt == "ogg") {
        if(fileSize < cv::snd_file_min_size.getVal<size_t>()) return false;

        // read first 1KB or entire file if smaller
        constexpr size_t max_search = 1024;
        size_t bytes_to_read = std::min(fileSize, max_search);
        if(bytes_to_read < 4) return false;

        std::vector<u8> buffer(bytes_to_read);
        if(reader.read_bytes(buffer.data(), bytes_to_read) != bytes_to_read) return false;

        const char* data = reinterpret_cast<const char*>(buffer.data());

        // check immediate OGG header
        if(memcmp(data, "OggS", 4) == 0) return true;

        // search first 1KB for OGG page header
        for(size_t i = 1; i + 3 < bytes_to_read; i++) {
            if(memcmp(data + i, "OggS", 4) == 0) return true;
        }
        return false;

    } else if(fileExt == "flac") {
        if(fileSize < std::max<size_t>(cv::snd_file_min_size.getVal<size_t>(), 96))  // account for larger header
            return false;

        // read first 1KB or entire file if smaller
        constexpr size_t max_search = 1024;
        size_t bytes_to_read = std::min(fileSize, max_search);
        if(bytes_to_read < 4) return false;

        std::vector<u8> buffer(bytes_to_read);
        if(reader.read_bytes(buffer.data(), bytes_to_read) != bytes_to_read) return false;

        const char* data = reinterpret_cast<const char*>(buffer.data());

        // check immediate FLAC header
        if(memcmp(data, "fLaC", 4) == 0) return true;

        // search first 1KB for FLAC header
        for(size_t i = 1; i + 3 < bytes_to_read; i++) {
            if(memcmp(data + i, "fLaC", 4) == 0) return true;
        }
        return false;
    }

    return false;  // don't let unsupported formats be read
}

const std::unordered_map<SOUNDHANDLE, PlaybackParams> &Sound::getActiveHandles() {
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
