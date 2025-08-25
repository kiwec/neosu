#pragma once

#include "SoundEngine.h"
#ifdef MCENGINE_FEATURE_BASS

#include "BassSound.h"

class Sound;

#ifndef BASSASIO_H
struct BASS_ASIO_INFO;
#endif

class BassSoundEngine final : public SoundEngine {
    NOCOPY_NOMOVE(BassSoundEngine)
    friend class BassSound;

   public:
    BassSoundEngine();
    ~BassSoundEngine() override;

    void restart() override;
    void shutdown() override;

    bool play(Sound *snd, f32 pan = 0.0f, f32 pitch = 0.f, f32 volume = 1.f) override;
    void pause(Sound *snd) override;
    void stop(Sound *snd) override;

    bool isReady() override;
    bool hasExclusiveOutput() override;

    void setOutputDevice(const OUTPUT_DEVICE &device) override;
    void setMasterVolume(float volume) override;

    void updateOutputDevices(bool printInfo) override;
    bool initializeOutputDevice(const OUTPUT_DEVICE &device) override;

    void onFreqChanged(float oldValue, float newValue) override;
    void onParamChanged(float oldValue, float newValue) override;

#ifdef MCENGINE_PLATFORM_WINDOWS
    static DWORD ASIO_clamp(BASS_ASIO_INFO info, DWORD buflen);
#endif

    SOUND_ENGINE_TYPE(BassSoundEngine, BASS, SoundEngine)
   private:
    bool isASIO() { return this->currentOutputDevice.driver == OutputDriver::BASS_ASIO; }
    bool isWASAPI() { return this->currentOutputDevice.driver == OutputDriver::BASS_WASAPI; }
    bool init_bass_mixer(const OUTPUT_DEVICE &device);

    double ready_since{-1.0};
    SOUNDHANDLE g_bassOutputMixer = 0;
};

#else
class BassSoundEngine : public SoundEngine {};
#endif
