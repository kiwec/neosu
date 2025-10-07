// Copyright (c) 2014, PG, All rights reserved.
#include "BassSound.h"

#ifdef MCENGINE_FEATURE_BASS

#include <algorithm>
#include <utility>

#include "BassManager.h"
#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Timing.h"
#include "Logging.h"

void BassSound::init() {
    if(this->bIgnored || this->sFilePath.length() < 2 || !(this->bAsyncReady.load())) return;

    this->bReady = this->bAsyncReady.load();
}

void BassSound::initAsync() {
    Sound::initAsync();
    if(this->bIgnored) return;

    UString file_path{this->sFilePath};

    if(this->bStream) {
        u32 flags = BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_STREAM_PRESCAN;
        if(cv::snd_async_buffer.getInt() > 0) flags |= BASS_ASYNCFILE;
        if constexpr(Env::cfg(OS::WINDOWS)) flags |= BASS_UNICODE;

        if(this->bInterrupted.load()) return;
        this->stream = BASS_StreamCreateFile(BASS_FILE_NAME, file_path.plat_str(), 0, 0, flags);
        if(!this->stream) {
            debugLog("BASS_StreamCreateFile() error on file {}: {}", this->sFilePath.c_str(),
                     BassManager::getErrorUString());
            return;
        }

        if(this->bInterrupted.load()) return;
        this->stream = BASS_FX_TempoCreate(this->stream, BASS_FX_FREESOURCE | BASS_STREAM_DECODE);
        if(!this->stream) {
            debugLog("BASS_FX_TempoCreate() error on file {}: {}", this->sFilePath.c_str(),
                     BassManager::getErrorUString());
            return;
        }

        BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO, false);
        BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS, 4.0f);
        BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS, 30.0f);
        BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO_OPTION_OLDPOS, 1);  // use old position calculation

        // Only compute the length once
        if(this->bInterrupted.load()) return;
        i64 length = BASS_ChannelGetLength(this->stream, BASS_POS_BYTE);
        f64 lengthInSeconds = BASS_ChannelBytes2Seconds(this->stream, length);
        f64 lengthInMilliSeconds = lengthInSeconds * 1000.0;
        this->length = (u32)lengthInMilliSeconds;
    } else {
        u32 flags = BASS_SAMPLE_FLOAT;
        if constexpr(Env::cfg(OS::WINDOWS)) flags |= BASS_UNICODE;

        if(this->bInterrupted.load()) return;
        this->sample = BASS_SampleLoad(false, file_path.plat_str(), 0, 0, 1, flags);
        if(!this->sample) {
            auto code = BASS_ErrorGetCode();
            if(code == BASS_ERROR_EMPTY) {
                debugLog("BassSound: Ignoring empty file {}", this->sFilePath.c_str());
                return;
            } else {
                debugLog("BASS_SampleLoad() error on file {}: {}", this->sFilePath.c_str(),
                         BassManager::getErrorUString(code));
                return;
            }
        }

        // Only compute the length once
        if(this->bInterrupted.load()) return;
        i64 length = BASS_ChannelGetLength(this->sample, BASS_POS_BYTE);
        f64 lengthInSeconds = BASS_ChannelBytes2Seconds(this->sample, length);
        f64 lengthInMilliSeconds = lengthInSeconds * 1000.0;
        this->length = (u32)lengthInMilliSeconds;
    }

    this->fSpeed = 1.0f;
    this->bAsyncReady = true;
}

void BassSound::destroy() {
    if(!this->bAsyncReady) {
        this->interruptLoad();
    }

    if(this->sample != 0) {
        BASS_SampleStop(this->sample);
        BASS_SampleFree(this->sample);
        this->sample = 0;
    }

    if(this->stream != 0) {
        BASS_Mixer_ChannelRemove(this->stream);
        BASS_ChannelStop(this->stream);
        BASS_StreamFree(this->stream);
        this->stream = 0;
    }

    for(const auto& [handle, _] : this->activeHandleCache) {
        BASS_Mixer_ChannelRemove(handle);
        BASS_ChannelStop(handle);
        BASS_ChannelFree(handle);
    }
    this->activeHandleCache.clear();

    this->bStarted = false;
    this->bReady = false;
    this->bAsyncReady = false;
    this->bPaused = false;
    this->paused_position_ms = 0;
    this->bIgnored = false;
}

void BassSound::setPositionMS(u32 ms) {
    if(!this->bReady || ms > this->getLengthMS()) return;
    assert(this->bStream);  // can't call setPositionMS() on a sample

    f64 seconds = (f64)ms / 1000.0;
    i64 target_pos = BASS_ChannelSeconds2Bytes(this->stream, seconds);
    if(target_pos < 0) {
        debugLog("BASS_ChannelSeconds2Bytes( stream , {} ) error on file {}: {}", seconds, this->sFilePath.c_str(),
                 BassManager::getErrorUString());
        return;
    }

    if(!BASS_Mixer_ChannelSetPosition(this->stream, target_pos, BASS_POS_BYTE | BASS_POS_MIXER_RESET)) {
        if(cv::debug_snd.getBool()) {
            debugLog("BASS_Mixer_ChannelSetPosition( stream , {} ) error on file {}: {}", ms, this->sFilePath.c_str(),
                     BassManager::getErrorUString());
        }
    }

    if(this->bPaused) {
        this->paused_position_ms = ms;
    }

    this->interpolator.reset(static_cast<f64>(ms), Timing::getTimeReal(), this->getSpeed());
}

void BassSound::setSpeed(float speed) {
    if(!this->bReady) return;
    assert(this->bStream);  // can't call setSpeed() on a sample

    speed = std::clamp<float>(speed, 0.05f, 50.0f);

    float freq = cv::snd_freq.getFloat();
    BASS_ChannelGetAttribute(this->stream, BASS_ATTRIB_FREQ, &freq);

    BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO, 1.0f);
    BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO_FREQ, freq);

    if(cv::nightcore_enjoyer.getBool()) {
        BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO_FREQ, speed * freq);
    } else {
        BASS_ChannelSetAttribute(this->stream, BASS_ATTRIB_TEMPO, (speed - 1.0f) * 100.0f);
    }

    this->fSpeed = speed;
}

void BassSound::setFrequency(float frequency) {
    if(!this->bReady) return;

    frequency = (frequency > 99.0f ? std::clamp<float>(frequency, 100.0f, 100000.0f) : 0.0f);

    for(const auto& [handle, _] : this->getActiveHandles()) {
        BASS_ChannelSetAttribute(handle, BASS_ATTRIB_FREQ, frequency);
    }
}

void BassSound::setPan(float pan) {
    if(!this->bReady) return;

    this->fPan = std::clamp<float>(pan, -1.0f, 1.0f);

    for(const auto& [handle, _] : this->getActiveHandles()) {
        BASS_ChannelSetAttribute(handle, BASS_ATTRIB_PAN, this->fPan);
    }
}

void BassSound::setLoop(bool loop) {
    if(!this->bReady) return;
    assert(this->bStream);  // can't call setLoop() on a sample

    this->bIsLooped = loop;
    BASS_ChannelFlags(this->stream, this->bIsLooped ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP);
}

float BassSound::getPosition() const {
    f32 length = this->getLengthMS();
    if(length <= 0.f) return 0.f;

    return (f32)this->getPositionMS() / length;
}

u32 BassSound::getPositionMS() const {
    if(!this->bReady) return 0;
    assert(this->bStream);  // can't call getPositionMS() on a sample

    if(this->bPaused) {
        return this->paused_position_ms;
    }

    if(!this->isPlaying()) {
        // We 'pause' even when stopping the sound, so it is safe to assume the sound hasn't started yet.
        return 0;
    }

    i64 positionBytes = BASS_Mixer_ChannelGetPosition(this->stream, BASS_POS_BYTE);
    if(positionBytes < 0) {
        assert(false);  // invalid handle
        return 0;
    }

    f64 positionInSeconds = BASS_ChannelBytes2Seconds(this->stream, positionBytes);
    f64 rawPositionMS = positionInSeconds * 1000.0;
    return this->interpolator.update(rawPositionMS, Timing::getTimeReal(), this->getSpeed(), this->isLooped(),
                                     static_cast<u64>(this->length), this->isPlaying());
}

u32 BassSound::getLengthMS() const {
    if(!this->bReady) return 0;
    return this->length;
}

float BassSound::getSpeed() const { return this->fSpeed; }

float BassSound::getFrequency() const {
    auto default_freq = cv::snd_freq.getFloat();
    if(!this->bReady) return default_freq;
    assert(this->bStream);  // can't call getFrequency() on a sample

    float frequency = default_freq;
    BASS_ChannelGetAttribute(this->stream, BASS_ATTRIB_FREQ, &frequency);
    return frequency;
}

bool BassSound::isPlaying() const {
    return this->bReady && this->bStarted && !this->bPaused &&
           !const_cast<BassSound*>(this)->getActiveHandles().empty();
}

bool BassSound::isFinished() const { return this->getPositionMS() >= this->getLengthMS(); }

void BassSound::rebuild(std::string newFilePath) {
    this->sFilePath = std::move(newFilePath);
    resourceManager->reloadResource(this);
}

bool BassSound::isHandleValid(SOUNDHANDLE queryHandle) const { return BASS_Mixer_ChannelGetMixer(queryHandle); }

void BassSound::setHandleVolume(SOUNDHANDLE handle, float volume) {
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, volume);
}

// Kind of bad naming, this gets an existing handle for streams, and creates a new one for samples
// Will be stored in active instances if playback succeeds
SOUNDHANDLE BassSound::getNewHandle() {
    if(this->bStream) {
        return this->stream;
    } else {
        auto chan = BASS_SampleGetChannel(this->sample, BASS_SAMCHAN_STREAM | BASS_STREAM_DECODE);
        return chan;
    }
}

#endif
