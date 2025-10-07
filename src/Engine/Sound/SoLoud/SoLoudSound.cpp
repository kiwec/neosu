// Copyright (c) 2025, WH, All rights reserved.
#include "SoLoudSound.h"

#ifdef MCENGINE_FEATURE_SOLOUD

#include "SoLoudThread.h"

#include "SoLoudFX.h"
#include "SoLoudSoundEngine.h"

#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "ResourceManager.h"
#include "Logging.h"

#include "soloud_file.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"

void SoLoudSound::init() {
    if(this->bIgnored || this->sFilePath.length() < 2 || !(this->bAsyncReady.load())) return;

    if(!this->audioSource)
        debugLog("Couldn't load sound \"{}\", stream = {}, file = {}", this->sFilePath, this->bStream, this->sFilePath);
    else
        this->bReady = true;
}

SoLoudSound::~SoLoudSound() { this->destroy(); }

void SoLoudSound::initAsync() {
    Sound::initAsync();
    if(this->bIgnored) return;

    // clean up any previous instance
    if(this->audioSource) {
        if(this->bStream)
            delete static_cast<SoLoud::SLFXStream *>(this->audioSource);
        else
            delete static_cast<SoLoud::Wav *>(this->audioSource);

        this->audioSource = nullptr;
    }

    // create the appropriate audio source based on streaming flag
    SoLoud::result result = SoLoud::SO_NO_ERROR;
    if(this->bStream) {
        // use SLFXStream for streaming audio (music, etc.) includes rate/pitch processing like BASS_FX_TempoCreate
        auto *stream = new SoLoud::SLFXStream(cv::snd_soloud_prefer_ffmpeg.getInt() > 0);
        result = stream->load(this->sFilePath.c_str());

        if(result == SoLoud::SO_NO_ERROR) {
            this->audioSource = stream;
            this->fFrequency = stream->mBaseSamplerate;

            this->audioSource->setInaudibleBehavior(
                true, false);  // keep ticking the sound if it goes to 0 volume, and don't kill it

            if(cv::debug_snd.getBool())
                debugLog(
                    "SoLoudSound: Created SLFXStream for {:s} with speed={:f}, pitch={:f}, looping={:s}, "
                    "decoder={:s}",
                    this->sFilePath, this->fSpeed, this->fPitch, this->bIsLooped ? "true" : "false",
                    stream->getDecoder());
        } else {
            delete stream;
            debugLog("Sound Error: SLFXStream::load() error {} on file {:s}", result, this->sFilePath);
            return;
        }
    } else {
        SoLoud::DiskFile df(File::fopen_c(this->sFilePath.c_str(), "rb"));

        if(!df.mFileHandle) {  // fopen failed
            debugLog("Sound Error: SoLoud::Wav::load() error {} on file {:s}", result, this->sFilePath);
            return;
        }

        // use Wav for non-streaming audio (hit sounds, effects, etc.)
        auto *wav = new SoLoud::Wav(cv::snd_soloud_prefer_ffmpeg.getInt() > 1);
        // the file's contents are immediately read into an internal buffer, so we don't have to leave it open
        // this is untrue for streams, but the SLFXStream wrapper handles wide path conversion internally
        result = wav->loadFile(&df);

        if(result == SoLoud::SO_NO_ERROR) {
            this->audioSource = wav;
            this->fFrequency = wav->mBaseSamplerate;

            this->audioSource->setInaudibleBehavior(
                true, true);  // keep ticking the sound if it goes to 0 volume, but do kill it if necessary
        } else {
            delete wav;
            debugLog("Sound Error: SoLoud::Wav::load() error {} on file {:s}", result, this->sFilePath);
            return;
        }
    }

    // only play one music track at a time
    this->audioSource->setSingleInstance(this->bStream || !this->bIsOverlayable);
    this->audioSource->setLooping(this->bIsLooped);

    this->bAsyncReady = true;
}

SOUNDHANDLE SoLoudSound::getHandle() { return this->handle; }

void SoLoudSound::destroy() {
    if(!this->bReady) return;

    this->bReady = false;

    // stop the sound if it's playing
    if(this->handle != 0) {
        if(soloud) soloud->stop(this->handle);
        this->handle = 0;
    }

    // clean up audio source
    if(this->audioSource) {
        if(this->bStream)
            delete static_cast<SoLoud::SLFXStream *>(this->audioSource);
        else
            delete static_cast<SoLoud::Wav *>(this->audioSource);

        this->audioSource = nullptr;
    }

    // need to reset this because the soloud handle has been destroyed
    this->bIsLoopingActuallySet = false;
    this->fFrequency = 44100.0f;
    this->fPitch = 1.0f;
    this->fSpeed = 1.0f;
    this->fPan = 0.0f;
    this->activeHandleCache.clear();
    this->fLastPlayTime = 0.0f;
    this->bIgnored = false;
    this->bAsyncReady = false;

    // reset position cache state
    this->cached_stream_position = 0.0;
    this->soloud_stream_position_cache_time = -1.0;
    this->force_sync_position_next = true;
}

void SoLoudSound::setPositionMS(u32 ms) {
    if(!this->bReady || !this->audioSource || !this->handle) return;

    auto msD = static_cast<double>(ms);

    auto streamLengthMS = static_cast<double>(getLengthMS());
    if(msD > streamLengthMS) return;

    double positionInSeconds = msD / 1000.0;

    if(cv::debug_snd.getBool()) debugLog("seeking to {:g}ms (length: {:g}ms)", msD, streamLengthMS);

    // seek
    soloud->seek(this->handle, positionInSeconds);

    // force next position query to be synchronous to get accurate post-seek position
    this->force_sync_position_next = true;

    // reset position interp vars with the new position
    this->interpolator.reset(getStreamPositionInSeconds() * 1000.0, Timing::getTimeReal(), getSpeed());
}

void SoLoudSound::setSpeed(float speed) {
    if(!this->bReady || !this->audioSource || !this->handle) return;

    // sample speed could be supported, but there is nothing using it right now so i will only bother when the time
    // comes
    if(!this->bStream) {
        debugLog("Programmer Error: tried to setSpeed on a sample!");
        return;
    }

    speed = std::clamp<float>(speed, 0.05f, 50.0f);

    if(this->fSpeed != speed) {
        float previousSpeed = this->fSpeed;
        this->fSpeed = speed;

        auto *filteredStream = static_cast<SoLoud::SLFXStream *>(this->audioSource);

        if(cv::nightcore_enjoyer.getBool()) {
            this->setPitch(1.0f);  // make sure the filter pitch is reset
            filteredStream->setSpeedFactor(1.0f);
            // then directly set the relative play speed
            soloud->setRelativePlaySpeed(this->handle, speed);
        } else {
            soloud->setRelativePlaySpeed(this->handle, 1.0f);
            // update the SLFXStream parameters
            filteredStream->setSpeedFactor(this->fSpeed);
        }

        if(cv::debug_snd.getBool())
            debugLog("SoLoudSound: Speed change {:s}: {:f}->{:f} (nightcore_enjoyer={})", this->sFilePath,
                     previousSpeed, this->fSpeed, cv::nightcore_enjoyer.getBool());
    }
}

void SoLoudSound::setPitch(float pitch) {
    if(!this->bReady || !this->audioSource) return;

    // sample pitch could be supported, but there is nothing using it right now so i will only bother when the time
    // comes
    if(!this->bStream) {
        debugLog("Programmer Error: tried to this->setPitch on a sample!");
        return;
    }

    pitch = std::clamp<float>(pitch, 0.0f, 2.0f);

    if(this->fPitch != pitch) {
        float previousPitch = this->fPitch;
        this->fPitch = pitch;

        // simply update the SLFXStream parameters
        auto *stream = static_cast<SoLoud::SLFXStream *>(this->audioSource);
        stream->setPitchFactor(this->fPitch);

        if(cv::debug_snd.getBool())
            debugLog("SoLoudSound: Pitch change {:s}: {:f}->{:f} (stream, updated live)", this->sFilePath,
                     previousPitch, this->fPitch);
    }
}

void SoLoudSound::setFrequency(float frequency) {
    if(!this->bReady || !this->audioSource) return;

    frequency = (frequency > 99.0f ? std::clamp<float>(frequency, 100.0f, 100000.0f) : 0.0f);

    if(this->fFrequency != frequency) {
        if(frequency > 0) {
            if(this->bStream) {
                float pitchRatio = frequency / this->fFrequency;

                // apply the frequency change through pitch
                // this isn't the only or even a good way, but it does the trick
                this->setPitch(this->fPitch * pitchRatio);
            } else if(this->handle) {
                soloud->setSamplerate(this->handle, frequency);
            }
            this->fFrequency = frequency;
        } else  // 0 means reset to default
        {
            if(this->bStream)
                this->setPitch(1.0f);
            else if(this->handle)
                soloud->setSamplerate(this->handle, frequency);
            this->fFrequency = this->audioSource->mBaseSamplerate;
        }
    }
}

void SoLoudSound::setPan(float pan) {
    if(!this->bReady || !this->handle) return;

    pan = std::clamp<float>(pan, -1.0f, 1.0f);

    this->fPan = pan;

    // apply to the active voice
    soloud->setPan(this->handle, pan);
}

void SoLoudSound::setLoop(bool loop) {
    if(!this->bReady || !this->audioSource || this->bIsLoopingActuallySet == loop) return;

    this->bIsLooped = loop;

    if(cv::debug_snd.getBool()) debugLog("setLoop {}", loop);

    // apply to the source
    this->audioSource->setLooping(loop);

    // apply to the active voice
    if(this->handle != 0) {
        soloud->setLooping(this->handle, loop);
        this->bIsLoopingActuallySet = loop;
    }
}

float SoLoudSound::getPosition() const {
    if(!this->bReady || !this->audioSource || !this->handle) return 0.0f;

    double streamLengthInSeconds = getSourceLengthInSeconds();
    if(streamLengthInSeconds <= 0.0) return 0.0f;

    double streamPositionInSeconds = getStreamPositionInSeconds();

    // update interped state while we're at it
    this->interpolator.update(streamPositionInSeconds * 1000.0, Timing::getTimeReal(), getSpeed(), isLooped(),
                              static_cast<u32>(streamLengthInSeconds * 1000.0), isPlaying());

    return std::clamp<float>(streamPositionInSeconds / streamLengthInSeconds, 0.0f, 1.0f);
}

i32 SoLoudSound::getBASSStreamLatencyCompensation() const {
    if(!this->bReady || !this->bStream || !this->audioSource || !this->handle) return 0.0f;

    return static_cast<i32>(std::round(static_cast<SoLoud::SLFXStream *>(this->audioSource)->getInternalLatency())) +
           cv::snd_soloud_hardcoded_offset.getInt();
}

// slightly tweaked interp algo from the SDL_mixer version, to smooth out position updates
u32 SoLoudSound::getPositionMS() const {
    if(!this->bReady || !this->audioSource || !this->handle) return 0;

    return this->interpolator.update(getStreamPositionInSeconds() * 1000.0, Timing::getTimeReal(), getSpeed(),
                                     isLooped(), getLengthMS(), isPlaying());
}

u32 SoLoudSound::getLengthMS() const {
    if(!this->bReady || !this->audioSource) return 0;

    const double lengthInMilliSeconds = getSourceLengthInSeconds() * 1000.0;
    // if (cv::debug_snd.getBool())
    // 	debugLog("lengthMS for {:s}: {:g}", this->sFilePath, lengthInMilliSeconds);
    return static_cast<u32>(lengthInMilliSeconds);
}

float SoLoudSound::getSpeed() const {
    if(!this->bReady) return 1.0f;

    return this->fSpeed;
}

float SoLoudSound::getPitch() const {
    if(!this->bReady) return 1.0f;

    return this->fPitch;
}

bool SoLoudSound::isPlaying() const {
    if(!this->bReady) return false;

    // a sound is playing if our handle is valid and the sound isn't paused
    return this->is_playing_cached();
}

bool SoLoudSound::isFinished() const {
    if(!this->bReady) return false;

    // a sound is finished if our handle is no longer valid
    return !this->valid_handle_cached();
}

void SoLoudSound::rebuild(std::string newFilePath) {
    this->sFilePath = std::move(newFilePath);
    resourceManager->reloadResource(this);
}

bool SoLoudSound::isHandleValid(SOUNDHANDLE queryHandle) const {
    return queryHandle != 0 && this->isReady() && soloud && soloud->isValidVoiceHandle(queryHandle);
}

void SoLoudSound::setHandleVolume(SOUNDHANDLE handle, float volume) {
    if(handle != 0 && this->isReady() && soloud) {
        // soloud does not support amplified (>1.0f) volume
        soloud->setVolume(handle, std::clamp<float>(volume, 0.f, 1.f));
    }
}

// soloud-specific accessors

double SoLoudSound::getStreamPositionInSeconds() const {
    if(!this->audioSource || !this->handle) return this->interpolator.getLastInterpolatedPositionMS() / 1000.0;

    const auto now = Timing::getTimeReal();

    // check if we need to force synchronous access (e.g. init, or after seek)
    if(this->force_sync_position_next) {
        this->force_sync_position_next = false;
        this->cached_stream_position.store(soloud->getStreamPosition(this->handle), std::memory_order_release);
        this->soloud_stream_position_cache_time.store(now, std::memory_order_release);
        return this->cached_stream_position;
    }

    // use cached value if recent enough (updated within last 10ms)
    if(now >= this->soloud_stream_position_cache_time + 0.01) {
        // cache is stale, trigger async update
        this->soloud_stream_position_cache_time.store(now, std::memory_order_relaxed);  // prevent multiple async calls
        soloud->updateCachedPosition(this->handle, this->soloud_stream_position_cache_time,
                                     this->cached_stream_position);
    }

    return this->cached_stream_position.load(std::memory_order_acquire);
}

double SoLoudSound::getSourceLengthInSeconds() const {
    if(!this->audioSource) return 0.0;
    if(this->bStream)
        return static_cast<SoLoud::SLFXStream *>(this->audioSource)->getLength();
    else
        return static_cast<SoLoud::Wav *>(this->audioSource)->getLength();
}

bool SoLoudSound::valid_handle_cached() const {
    if(this->handle == 0) return false;

    const auto now = engine->getTime();
    if(now >= this->soloud_valid_handle_cache_time + 0.01) {  // 10ms intervals should be fast enough
        this->soloud_valid_handle_cache_time = now;
        if(!soloud->isValidVoiceHandle(this->handle)) {
            const_cast<SoLoudSound*>(this)->handle = 0;
        }
    }

    return this->handle != 0;
}

bool SoLoudSound::is_playing_cached() const {
    if(!this->valid_handle_cached()) return false;

    const auto now = engine->getTime();
    if(now >= this->soloud_paused_handle_cache_time + 0.01) {
        this->soloud_paused_handle_cache_time = now;
        this->cached_pause_state = soloud->getPause(this->handle);
    }

    return this->cached_pause_state != true;
}

#endif  // MCENGINE_FEATURE_SOLOUD
