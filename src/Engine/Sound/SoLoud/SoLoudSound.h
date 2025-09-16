#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef SOLOUD_SOUND_H
#define SOLOUD_SOUND_H

#include "Sound.h"
#ifdef MCENGINE_FEATURE_SOLOUD

#include <memory>

// fwd decls to avoid include external soloud headers here
namespace SoLoud {
class Soloud;
class AudioSource;
class SLFXStream;
}  // namespace SoLoud

class SoLoudThreadWrapper;

// defined in SoLoudSoundEngine, soloud instance singleton pointer
extern std::unique_ptr<SoLoudThreadWrapper> soloud;

class SoLoudSound final : public Sound {
    NOCOPY_NOMOVE(SoLoudSound)
    friend class SoLoudSoundEngine;

   public:
    SoLoudSound(std::string filepath, bool stream, bool overlayable, bool loop)
        : Sound(std::move(filepath), stream, overlayable, loop) {}
    ~SoLoudSound() override;

    // Sound interface implementation
    void setPositionMS(u32 ms) override;
    void setSpeed(float speed) override;
    void setPitch(float pitch) override;
    void setFrequency(float frequency) override;
    void setPan(float pan) override;
    void setLoop(bool loop) override;

    float getPosition() override;
    u32 getPositionMS() override;
    u32 getLengthMS() override;
    float getSpeed() override;
    float getPitch() override;
    // i.e. we would be hearing audio 15ms sooner than if we were using BASS
    i32 getBASSStreamLatencyCompensation() const override;
    inline float getFrequency() override { return this->fFrequency; }

    bool isPlaying() override;
    bool isFinished() override;

    void rebuild(std::string newFilePath) override;

    // inspection
    SOUND_TYPE(SoLoudSound, SOLOUD, Sound)
   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

    void setHandleVolume(SOUNDHANDLE handle, float volume) override;
    [[nodiscard]] bool isHandleValid(SOUNDHANDLE queryHandle) const override;

   private:
    SOUNDHANDLE getHandle();

    // helpers to access Wav/SLFXStream internals
    [[nodiscard]] double getSourceLengthInSeconds() const;
    [[nodiscard]] double getStreamPositionInSeconds() const;

    // current playback parameters
    float fFrequency{44100.0f};         // sample rate in Hz
    bool bIsLoopingActuallySet{false};  // looping has been set on the soloud audio handle

    // SoLoud-specific members
    SoLoud::AudioSource *audioSource{nullptr};  // base class pointer, could be either SLFXStream or Wav
    SOUNDHANDLE handle{0};                      // most recently played instance of this sound

    // these are some caching workarounds for limitations of the main soloud instance running on the main thread
    // while its device audio callback being threaded (possibly, not necessarily, pulseaudio + miniaudio creates
    // separate thread for example) this causes the internal audio mutex (global lock) to be held for each voice handle
    // query, which can add up and be unnecessarily slow

    // avoid calling soloud->isValidVoiceHandle too often, because it locks the entire internal audio mutex
    bool valid_handle_cached();
    double soloud_valid_handle_cache_time{-1.};
    // same with soloud->getPause(), for getPosition queries
    bool is_playing_cached();
    bool cached_pause_state{false};
    double soloud_paused_handle_cache_time{-1.};
};

#else
class SoLoudSound : public Sound {};
#endif
#endif
