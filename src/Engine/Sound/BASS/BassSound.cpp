// Copyright (c) 2014, PG, All rights reserved.
#include "BassSound.h"

#ifdef MCENGINE_FEATURE_BASS

#include <algorithm>
#include <chrono>
#include <thread>

#include "BassManager.h"
#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Timing.h"
#include "Logging.h"

void BassSound::init() {
    if(this->bIgnored || this->sFilePath.length() < 2 || !(this->isAsyncReady())) return;

    this->setReady(this->isAsyncReady());
}

void BassSound::initAsync() {
    Sound::initAsync();
    if(this->bIgnored) return;

    UString file_path{this->sFilePath};

    SOUNDHANDLE handle = 0;

    if(this->bStream) {
        u32 flags = BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_STREAM_PRESCAN;
        if(cv::snd_async_buffer.getInt() > 0) flags |= BASS_ASYNCFILE;
        if constexpr(Env::cfg(OS::WINDOWS)) flags |= BASS_UNICODE;

        if(this->isInterrupted()) return;
        this->stream = BASS_StreamCreateFile(BASS_FILE_NAME, file_path.plat_str(), 0, 0, flags);
        if(!this->stream) {
            debugLog("BASS_StreamCreateFile() error on file {}: {}", this->sFilePath.c_str(),
                     BassManager::getErrorUString());
            return;
        }

        if(this->isInterrupted()) return;
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
        if(this->isInterrupted()) return;
        handle = this->stream;
    } else {
        u32 flags = BASS_SAMPLE_FLOAT;
        if constexpr(Env::cfg(OS::WINDOWS)) flags |= BASS_UNICODE;

        if(this->isInterrupted()) return;
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

        if(this->isInterrupted()) return;
        handle = this->sample;
    }

    // Only compute the length once
    i64 length = BASS_ChannelGetLength(handle, BASS_POS_BYTE);
    f64 lengthInSeconds = BASS_ChannelBytes2Seconds(handle, length);
    f64 lengthInMilliSeconds = lengthInSeconds * 1000.0;
    this->length = (u32)lengthInMilliSeconds;

    this->fSpeed = 1.0f;
    this->setAsyncReady(true);
}

void BassSound::destroy() {
    if(!this->isAsyncReady()) {
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
    this->setReady(false);
    this->setAsyncReady(false);
    this->bPaused = false;
    this->paused_position_ms = 0;
    this->bIgnored = false;
    this->fLastPlayTime = 0.f;
}

void BassSound::setPositionMS(u32 ms) {
    if(!this->isReady() || ms > this->getLengthMS()) {
        logIfCV(debug_snd, "can't set position to {}ms: {}", ms,
                !this->isReady() ? "not ready" : fmt::format("{} > {}", ms, this->getLengthMS()));
        return;
    }
    assert(this->bStream);  // can't call setPositionMS() on a sample

    f64 seconds = (f64)ms / 1000.0;
    i64 target_pos = BASS_ChannelSeconds2Bytes(this->stream, seconds);
    if(target_pos < 0) {
        debugLog("BASS_ChannelSeconds2Bytes( stream , {} ) error on file {}: {}", seconds, this->sFilePath.c_str(),
                 BassManager::getErrorUString());
        return;
    }

    if(!BASS_Mixer_ChannelSetPosition(this->stream, target_pos, BASS_POS_BYTE | BASS_POS_MIXER_RESET)) {
        logIfCV(debug_snd, "BASS_Mixer_ChannelSetPosition( stream , {} ) error on file {}: {}", ms,
                this->sFilePath.c_str(), BassManager::getErrorUString());
        return;
    }

    // when paused, position change is immediate
    if(this->bPaused) {
        this->paused_position_ms = ms;
        logIfCV(debug_snd, "set paused position to {}ms", ms);
        this->interpolator.reset(static_cast<f64>(ms), Timing::getTimeReal(), this->getSpeed());
        return;
    }

    // when playing, poll until position updates (with timeout)
    // this is necessary because BASS_Mixer_ChannelGetPosition takes time to reflect the change
    const auto start = Timing::getTicksMS();
    constexpr u64 timeoutMS = 100;
    constexpr u32 toleranceMS = 50;

    u32 actual = 0;
    while(true) {
        i64 posBytes = BASS_Mixer_ChannelGetPosition(this->stream, BASS_POS_BYTE);
        if(posBytes >= 0) {
            f64 posSec = BASS_ChannelBytes2Seconds(this->stream, posBytes);
            actual = static_cast<u32>(posSec * 1000.0);

            // check if we're within tolerance of target
            if(actual >= ms && actual <= ms + toleranceMS) {
                break;
            }
        }

        // check timeout
        if(Timing::getTicksMS() - start > timeoutMS) {
            debugLog("timeout waiting for position update on {} (wanted {}ms, got {}ms)", this->sFilePath.c_str(), ms,
                     actual);
            break;
        }

        Timing::sleepNS(100000);
    }

    logIfCV(debug_snd, "set position to actual: {}ms desired: {}ms", actual, ms);
    this->interpolator.reset(static_cast<f64>(actual), Timing::getTimeReal(), this->getSpeed());
}

void BassSound::setSpeed(float speed) {
    if(!this->isReady()) return;
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
    if(!this->isReady()) return;

    frequency = (frequency > 99.0f ? std::clamp<float>(frequency, 100.0f, 100000.0f) : 0.0f);

    for(const auto& [handle, _] : this->getActiveHandles()) {
        BASS_ChannelSetAttribute(handle, BASS_ATTRIB_FREQ, frequency);
    }
}

void BassSound::setPan(float pan) {
    if(!this->isReady()) return;

    this->fPan = std::clamp<float>(pan, -1.0f, 1.0f);

    for(const auto& [handle, _] : this->getActiveHandles()) {
        BASS_ChannelSetAttribute(handle, BASS_ATTRIB_PAN, this->fPan);
    }
}

void BassSound::setLoop(bool loop) {
    if(!this->isReady()) return;
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
    if(!this->isReady()) return 0;
    assert(this->bStream);  // can't call getPositionMS() on a sample

    if(this->bPaused) {
        this->interpolator.reset(static_cast<f64>(this->paused_position_ms), Timing::getTimeReal(), this->getSpeed());
        logIfCV(debug_snd, "paused pos {}ms", this->paused_position_ms);
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
    u32 ret = this->interpolator.update(rawPositionMS, Timing::getTimeReal(), this->getSpeed(), this->isLooped(),
                                        static_cast<u64>(this->length), this->isPlaying());

    logIfCV(debug_snd, "pos {}ms", ret);
    return ret;
}

u32 BassSound::getLengthMS() const {
    if(!this->isReady()) return 0;
    return this->length;
}

float BassSound::getSpeed() const { return this->fSpeed; }

float BassSound::getFrequency() const {
    auto default_freq = cv::snd_freq.getFloat();
    if(!this->isReady()) return default_freq;
    assert(this->bStream);  // can't call getFrequency() on a sample

    float frequency = default_freq;
    BASS_ChannelGetAttribute(this->stream, BASS_ATTRIB_FREQ, &frequency);
    return frequency;
}

bool BassSound::isPlaying() const {
    return this->isReady() && this->bStarted && !this->bPaused &&
           !const_cast<BassSound*>(this)->getActiveHandles().empty();
}

bool BassSound::isFinished() const { return this->getPositionMS() >= this->getLengthMS(); }

bool BassSound::isHandleValid(SOUNDHANDLE queryHandle) const { return BASS_Mixer_ChannelGetMixer(queryHandle) != 0; }

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
