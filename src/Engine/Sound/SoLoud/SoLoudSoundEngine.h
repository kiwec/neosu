#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef SOLOUD_SOUNDENGINE_H
#define SOLOUD_SOUNDENGINE_H

#include "config.h"
#ifdef MCENGINE_FEATURE_SOLOUD
#include "SoundEngine.h"

#include <map>
#include <memory>
#include <optional>

// fwd decls to avoid include external soloud headers here
namespace SoLoud {
class Soloud;
struct DeviceInfo;
}  // namespace SoLoud

class SoLoudSound;

class SoLoudSoundEngine final : public SoundEngine {
    NOCOPY_NOMOVE(SoLoudSoundEngine)
   public:
    SoLoudSoundEngine();
    ~SoLoudSoundEngine() override;

    // factory
    Sound *createSound(std::string filepath, bool stream, bool overlayable, bool loop) override;

    void restart() override;
    void update() override;
    void onFocusGained() override;
    void onFocusLost() override;

    bool play(Sound *snd, f32 pan = 0.f, f32 pitch = 0.f, f32 playVolume = 1.f, bool startPaused = false) override;
    void pause(Sound *snd) override;
    void stop(Sound *snd) override;

    inline bool isReady() override { return this->bReady; }

    inline bool isASIO() override { return this->currentOutputDevice.driver == OutputDriver::SOLOUD_ASIO; }
    std::optional<OutputBufferLimits> getOutputBufferLimits() override;
    void openDeviceControlPanel() override;

    void setOutputDevice(const OUTPUT_DEVICE &device) override;
    void setMasterVolume(f32 volume) override;

    void allowInternalCallbacks() override;

    SOUND_ENGINE_TYPE(SoLoudSoundEngine, SOLOUD, SoundEngine)
   private:
    // internal helpers for play()
    bool playSound(SoLoudSound *soloudSound, f32 pan, f32 pitch, f32 playVolume, bool startPaused);
    bool updateExistingSound(SoLoudSound *soloudSound, SOUNDHANDLE handle, f32 pan, f32 pitch, f32 playVolume,
                             bool startPaused);

    void updateOutputDevices(bool printInfo) override;

    bool initializeOutputDevice(const OUTPUT_DEVICE &device) override;

    void setOutputDeviceByName(std::string_view desiredDeviceName);
    bool setOutputDeviceInt(const OUTPUT_DEVICE &desiredDevice, bool force = false);
    bool switchShareModes(const std::optional<OUTPUT_DEVICE> &toKnownDevice = {});

    void onMaxActiveChange(f32 newMax);
    void updateLastDevice();

    // keyed by the index into the MA/SDL enumeration (-1 for the default device), asio drivers start at ASIO_ID_BASE
    static constexpr int ASIO_ID_BASE = 1000;
    std::map<int, SoLoud::DeviceInfo> mSoloudDevices;

    std::optional<OUTPUT_DEVICE> lastMADevice;  // for switching between them (remember existing)
    std::optional<OUTPUT_DEVICE> lastSDLDevice;

    int iMaxActiveVoices;

    bool bReady{false};
    bool bWasBackendEverReady{false};

    // device loss recovery, see update()
    double fLastDeviceLostRestart{0.0};
    int iDeviceLostRestarts{0};

    // for backend
    static OutputDriver getMAorSDLCV();
    // parse resampler string from cvar
    static unsigned int getResamplerFromCV();
};

// raw pointer access to the s_SLInstance singleton, for SoLoudSound to use
extern SoLoud::Soloud *soloud;

#endif
#endif
