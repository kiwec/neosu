#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef SOLOUD_SOUNDENGINE_H
#define SOLOUD_SOUNDENGINE_H

#include "SoundEngine.h"
#ifdef MCENGINE_FEATURE_SOLOUD

#include "UString.h"

#include <map>
#include <memory>

// fwd decls to avoid include external soloud headers here
namespace SoLoud {
class Soloud;
struct DeviceInfo;
}  // namespace SoLoud

class SoLoudSound;
class SoLoudThreadWrapper;

class SoLoudSoundEngine final : public SoundEngine {
    NOCOPY_NOMOVE(SoLoudSoundEngine)
   public:
    SoLoudSoundEngine();
    ~SoLoudSoundEngine() override;

    void restart() override;

    bool play(Sound *snd, f32 pan = 0.f, f32 pitch = 0.f, f32 playVolume = 1.f, bool startPaused = false) override;
    void pause(Sound *snd) override;
    void stop(Sound *snd) override;

    inline bool isReady() override { return this->bReady; }

    void setOutputDevice(const OUTPUT_DEVICE &device) override;
    void setMasterVolume(float volume) override;

    void allowInternalCallbacks() override;

    SOUND_ENGINE_TYPE(SoLoudSoundEngine, SOLOUD, SoundEngine)
   private:
    // internal helpers for play()
    bool playSound(SoLoudSound *soloudSound, f32 pan, f32 pitch, f32 playVolume, bool startPaused);
    bool updateExistingSound(SoLoudSound *soloudSound, SOUNDHANDLE handle, f32 pan, f32 pitch, f32 playVolume, bool startPaused);

    void setVolumeGradual(SOUNDHANDLE handle, float targetVol, float fadeTimeMs = 10.0f);
    void updateOutputDevices(bool printInfo) override;

    bool initializeOutputDevice(const OUTPUT_DEVICE &device) override;

    void setOutputDeviceByName(std::string_view desiredDeviceName);
    bool setOutputDeviceInt(const OUTPUT_DEVICE &desiredDevice, bool force = false);

    int iMaxActiveVoices;
    void onMaxActiveChange(float newMax);

    std::map<int, SoLoud::DeviceInfo> mSoloudDevices;

    bool bReady{false};
    bool bWasBackendEverReady{false};

    // for backend
    static OutputDriver getMAorSDLCV();
};

// raw pointer access to the s_SLInstance singleton, for SoLoudSound to use
extern std::unique_ptr<SoLoudThreadWrapper> soloud;

#else
class SoLoudSoundEngine : public SoundEngine {};
#endif
#endif
