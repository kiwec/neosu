#pragma once
// Copyright (c) 2014, PG, All rights reserved.

#include "Resource.h"
#include "PlaybackInterpolator.h"

#include <unordered_map>

#define SOUND_TYPE(ClassName, TypeID, ParentClass)                      \
    static constexpr TypeId TYPE_ID = TypeID;                           \
    [[nodiscard]] TypeId getTypeId() const override { return TYPE_ID; } \
    [[nodiscard]] bool isTypeOf(TypeId typeId) const override {         \
        return typeId == TYPE_ID || ParentClass::isTypeOf(typeId);      \
    }

class SoundEngine;

using SOUNDHANDLE = uint32_t;

struct PlaybackParams {
    float pan{0.f};
    float pitch{0.f};
    float volume{1.f};
};

class Sound : public Resource {
    friend class SoundEngine;

   public:
    using TypeId = uint8_t;
    enum SndType : TypeId { BASS, SOLOUD };

   public:
    Sound(std::string filepath, bool stream, bool overlayable, bool loop)
        : Resource(std::move(filepath)), bStream(stream), bIsLooped(loop), bIsOverlayable(overlayable) {
        this->activeHandleCache.reserve(5);
    }

    // Factory method to create the appropriate sound object
    static Sound *createSound(std::string filepath, bool stream, bool overlayable, bool loop);

    virtual void setPositionMS(u32 ms) = 0;
    virtual void setSpeed(float speed) = 0;
    virtual void setPitch(float pitch) { this->fPitch = pitch; }
    virtual void setFrequency(float frequency) = 0;
    virtual void setPan(float pan) = 0;
    virtual void setLoop(bool loop) = 0;

    // override currently audible sounds' (if any) volumes to "volume" (* baseVolume)
    void setPlayingVolume(float volume);
    // NOTE: this will also update currently playing handle(s) for this sound
    void setBaseVolume(float volume);

    inline float getBaseVolume() { return this->fBaseVolume; }
    virtual float getPosition() = 0;
    virtual u32 getPositionMS() = 0;
    virtual u32 getLengthMS() = 0;
    virtual float getFrequency() = 0;
    virtual float getPan() { return this->fPan; }
    virtual float getSpeed() { return this->fSpeed; }
    virtual float getPitch() { return this->fPitch; }

    virtual bool isPlaying() = 0;
    virtual bool isFinished() = 0;

    virtual void rebuild(std::string newFilePath) = 0;

    [[nodiscard]] constexpr bool isStream() const { return this->bStream; }
    [[nodiscard]] constexpr bool isLooped() const { return this->bIsLooped; }
    [[nodiscard]] constexpr bool isOverlayable() const { return this->bIsOverlayable; }

    // type inspection
    [[nodiscard]] Type getResType() const final { return SOUND; }

    Sound *asSound() final { return this; }
    [[nodiscard]] const Sound *asSound() const final { return this; }

    // type inspection
    [[nodiscard]] virtual TypeId getTypeId() const = 0;
    [[nodiscard]] virtual bool isTypeOf(TypeId /*type_id*/) const { return false; }
    template <typename T>
    [[nodiscard]] bool isType() const {
        return isTypeOf(T::TYPE_ID);
    }
    template <typename T>
    T *as() {
        return isType<T>() ? static_cast<T *>(this) : nullptr;
    }
    template <typename T>
    const T *as() const {
        return isType<T>() ? static_cast<const T *>(this) : nullptr;
    }

   protected:
    void init() override = 0;
    void initAsync() override;
    void destroy() override = 0;

    inline void setLastPlayTime(f64 lastPlayTime) { this->fLastPlayTime = lastPlayTime; }
    [[nodiscard]] constexpr f64 getLastPlayTime() const { return this->fLastPlayTime; }

    // backend-specific query
    virtual bool isHandleValid(SOUNDHANDLE queryHandle) const = 0;
    // backend-specific setter
    virtual void setHandleVolume(SOUNDHANDLE handle, float volume) = 0;

    // currently playing sound instances (updates cache)
    const std::unordered_map<SOUNDHANDLE, PlaybackParams> &getActiveHandles();
    inline void addActiveInstance(SOUNDHANDLE handle, PlaybackParams instance) {
        this->activeHandleCache[handle] = instance;
    }

    PlaybackInterpolator interpolator;

    std::unordered_map<SOUNDHANDLE, PlaybackParams> activeHandleCache;

    f64 fLastPlayTime{0.0};

    float fPan{0.0f};
    float fSpeed{1.0f};
    float fPitch{1.0f};

    // persistent across all plays for the sound object, only modifiable by setBaseVolume
    float fBaseVolume{1.0f};

    u32 paused_position_ms{0};
    u32 length{0};

    bool bStream;
    bool bIsLooped;
    bool bIsOverlayable;

    bool bIgnored{false};  // early check for audio file validity
    bool bStarted{false};
    bool bPaused{false};

   private:
    static bool isValidAudioFile(const std::string &filePath, const std::string &fileExt);
};
