#pragma once
#include "config.h"

#ifdef MCENGINE_FEATURE_BASS
#include "SoundEngine.h"

class Sound;
class BassSound;

#if defined(MCENGINE_PLATFORM_WINDOWS) && !defined(BASSASIO_H)
namespace bass_EXTERN {
extern "C" {
using BASS_ASIO_INFO = struct BASS_ASIO_INFO;
}
}  // namespace bass_EXTERN
using bass_EXTERN::BASS_ASIO_INFO;
#endif

#include <functional>

class BassSoundEngine final : public SoundEngine {
    NOCOPY_NOMOVE(BassSoundEngine)
    friend class BassSound;

   public:
    BassSoundEngine();
    ~BassSoundEngine() override;

    // factory
    Sound *createSound(std::string filepath, bool stream, bool overlayable, bool loop) override;

    void restart() override;
    void shutdown() override;
    void onFocusGained() override { ; }  // TODO: implement shared/exclusive toggling
    void onFocusLost() override { ; }

    bool play(Sound *snd, f32 pan = 0.0f, f32 pitch = 0.f, f32 volume = 1.f, bool startPaused = false) override;
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
    static uint32_t ASIO_clamp(const BASS_ASIO_INFO &info, uint32_t buflen);
    inline void setOnASIOBufferChangeCB(std::function<void(const BASS_ASIO_INFO &info)> cb) {
        this->asio_buffer_change_cb = std::move(cb);
    }
#endif

    SOUND_ENGINE_TYPE(BassSoundEngine, BASS, SoundEngine)

   private:
    bool isASIO() { return this->currentOutputDevice.driver == OutputDriver::BASS_ASIO; }
    bool isWASAPI() { return this->currentOutputDevice.driver == OutputDriver::BASS_WASAPI; }
    bool init_bass_mixer(const OUTPUT_DEVICE &device);

    bool actuallyPlay(BassSound *bassSound, SOUNDHANDLE playHandle, u64 positionUS);

    double ready_since{-1.0};
    SOUNDHANDLE g_bassOutputMixer = 0;

#ifdef MCENGINE_PLATFORM_WINDOWS
    std::function<void(const BASS_ASIO_INFO &info)> asio_buffer_change_cb{nullptr};
#endif
};

#endif
