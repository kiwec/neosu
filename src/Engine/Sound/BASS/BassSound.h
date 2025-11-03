#pragma once

#include "Sound.h"
#ifdef MCENGINE_FEATURE_BASS

class BassSoundEngine;

class BassSound final : public Sound {
    NOCOPY_NOMOVE(BassSound);
    friend class BassSoundEngine;

   public:
    BassSound(std::string filepath, bool stream, bool overlayable, bool loop)
        : Sound(std::move(filepath), stream, overlayable, loop) {};
    ~BassSound() override { this->destroy(); }

    void setPositionMS(u32 ms) override;

    void setSpeed(float speed) override;
    void setFrequency(float frequency) override;
    void setPan(float pan) override;
    void setLoop(bool loop) override;

    float getPosition() const override;
    u32 getPositionMS() const override;
    u32 getLengthMS() const override;
    float getSpeed() const override;
    float getFrequency() const override;

    bool isPlaying() const override;
    bool isFinished() const override;

    // inspection
    SOUND_TYPE(BassSound, BASS, Sound)

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

    void setHandleVolume(SOUNDHANDLE handle, float volume) override;
    [[nodiscard]] bool isHandleValid(SOUNDHANDLE queryHandle) const override;

   private:
    SOUNDHANDLE getNewHandle();

    SOUNDHANDLE srchandle{0};
};

#else
class BassSound : public Sound {};
#endif
