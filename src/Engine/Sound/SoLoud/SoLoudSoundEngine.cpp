// Copyright (c) 2025, WH, All rights reserved.
#include "SoLoudSoundEngine.h"

#ifdef MCENGINE_FEATURE_SOLOUD

#include "SString.h"
#include "SoLoudSound.h"

#include "ConVar.h"
#include "Engine.h"

#include "Environment.h"

#include <utility>
#include <soloud.h>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_audio.h>

std::unique_ptr<SoLoud::Soloud> soloud = nullptr;

SoundEngine::OutputDriver SoLoudSoundEngine::getMAorSDLCV() {
    OutputDriver out{OutputDriver::SOLOUD_MA};

    const auto &cvBackend = cv::snd_soloud_backend.getString();

    if(SString::contains_ncase(cvBackend, "sdl")) {
        out = OutputDriver::SOLOUD_SDL;
    } else {
        out = OutputDriver::SOLOUD_MA;
    }

    return out;
}

SoLoudSoundEngine::SoLoudSoundEngine() : SoundEngine() {
    if(!soloud) {
        soloud = std::make_unique<SoLoud::Soloud>();
    }

    cv::snd_freq.setValue(SoLoud::Soloud::AUTO);  // let it be auto-negotiated (the snd_freq callback will adjust if
                                                  // needed, if this is manually set in a config)
    cv::snd_freq.setDefaultFloat(SoLoud::Soloud::AUTO);

    this->iMaxActiveVoices =
        std::clamp<int>(cv::snd_sanity_simultaneous_limit.getInt(), 64,
                        255);  // TODO: lower this minimum (it will crash if more than this many sounds play at once...)

    OUTPUT_DEVICE defaultOutputDevice{.isDefault = true, .driver = getMAorSDLCV()};
    cv::snd_output_device.setValue(defaultOutputDevice.name);
    this->outputDevices.push_back(defaultOutputDevice);
    this->currentOutputDevice = defaultOutputDevice;

    this->mSoloudDevices = {};
}

void SoLoudSoundEngine::restart() {
    // if switching backends, we need to reinit from scratch (device enumeration needs refresh)
    this->setOutputDeviceInt(this->bWasBackendEverReady ? this->getWantedDevice()
                                                        : OUTPUT_DEVICE{.isDefault = true, .driver = getMAorSDLCV()},
                             true);
}

bool SoLoudSoundEngine::play(Sound *snd, f32 pan, f32 pitch, f32 playVolume) {
    if(!this->isReady() || snd == nullptr || !snd->isReady()) return false;

    // @spec: adding 1 here because kiwec changed the calling code in some way that i dont understand yet
    pitch += 1.0f;

    pan = std::clamp<float>(pan, -1.0f, 1.0f);
    pitch = std::clamp<float>(pitch, 0.01f, 2.0f);

    auto *soloudSound = snd->as<SoLoudSound>();
    if(!soloudSound) return false;

    auto existingHandle = soloudSound->getHandle();

    // check if we have a non-stale voice handle for the most recently played instance
    if(existingHandle != 0 && !soloud->isValidVoiceHandle(existingHandle)) {
        existingHandle = 0;
        soloudSound->handle = 0;
    }

    if(existingHandle != 0 && !soloudSound->isOverlayable()) {
        // if we do and it's not overlayable, update this last instance
        return this->updateExistingSound(soloudSound, existingHandle, pan, pitch, playVolume);
    } else {
        // otherwise try playing a new instance
        return this->playSound(soloudSound, pan, pitch, playVolume);
    }
}

bool SoLoudSoundEngine::updateExistingSound(SoLoudSound *soloudSound, SOUNDHANDLE handle, f32 pan, f32 pitch,
                                            f32 playVolume) {
    assert(soloudSound);
    if(soloudSound->getPitch() != pitch) {
        soloudSound->setPitch(pitch);
    }

    if(soloudSound->getSpeed() != pan) {
        soloudSound->setPan(pan);
    }

    soloudSound->setHandleVolume(handle, soloudSound->getBaseVolume() * playVolume);

    // update existing handle in cache with new params
    PlaybackParams newParams{.pan = pan, .pitch = pitch, .volume = playVolume};
    soloudSound->activeHandleCache[handle] = newParams;

    // make sure it's not paused
    soloud->setPause(handle, false);
    soloudSound->setLastPlayTime(engine->getTime());

    if(cv::debug_snd.getBool()) {
        debugLog("handle was already valid, for non-overlayable sound {}\n", soloudSound->getName());
    }
    return true;
}

bool SoLoudSoundEngine::playSound(SoLoudSound *soloudSound, f32 pan, f32 pitch, f32 playVolume) {
    if(!soloudSound) return false;

    // check if we should allow playing this frame
    const bool allowPlayFrame = !soloudSound->isOverlayable() || !cv::snd_restrict_play_frame.getBool() ||
                                engine->getTime() > soloudSound->getLastPlayTime();
    if(!allowPlayFrame) return false;

    if(cv::debug_snd.getBool()) {
        debugLog(
            "SoLoudSoundEngine: Attempting to play {:s} (stream={:d}) with speed={:f}, pitch={:f}, playVolume={:f}\n",
            soloudSound->sFilePath, soloudSound->bStream ? 1 : 0, soloudSound->fSpeed, pitch, playVolume);
    }

    // play the sound with appropriate method
    SOUNDHANDLE handle = 0;

    if(soloudSound->bStream) {
        // reset these, because they're "sticky" properties
        soloudSound->setPitch(pitch);
        soloudSound->setPan(pan);

        // streaming audio (music) - play SLFXStream directly (it handles SoundTouch internally)
        // start it at 0 volume and fade it in when we play it (to avoid clicks/pops)
        handle = soloud->play(*soloudSound->audioSource, 0, pan, true /* paused */);
        if(handle)
            soloud->setProtectVoice(
                handle,
                true);  // protect the music channel (don't let it get interrupted when many sounds play back at once)
                        // NOTE: this doesn't seem to work properly, not sure why... need to setMaxActiveVoiceCount
                        // higher than the default 16 as a workaround, otherwise rapidly overlapping samples like from
                        // buzzsliders can cause glitches in music playback
                        // TODO: a better workaround would be to manually prevent samples from playing if
                        // it would lead to getMaxActiveVoiceCount() <= getActiveVoiceCount()
    } else {
        // non-streams don't go through the SoLoudFX wrapper
        handle = soloud->play(*soloudSound->audioSource, soloudSound->fBaseVolume * playVolume, pan, true /* paused */);
    }

    // finalize playback
    if(handle != 0) {
        // store the handle and mark playback time
        soloudSound->handle = handle;

        PlaybackParams newInstance{.pan = pan, .pitch = pitch, .volume = playVolume};
        soloudSound->addActiveInstance(handle, newInstance);

        if(soloudSound->bStream) {  // fade it in if it's a stream (since we started it paused with 0 volume)
            this->setVolumeGradual(handle, soloudSound->fBaseVolume * playVolume);

            if(cv::debug_snd.getBool())
                debugLog("SoLoudSoundEngine: Playing streaming audio through SLFXStream with speed={:f}, pitch={:f}\n",
                         soloudSound->getSpeed(), soloudSound->getPitch());
        } else {  // set playback pitch
            // calculate final pitch by combining all pitch modifiers
            float playbackPitch = pitch * soloudSound->getPitch() * soloudSound->getSpeed();

            // set relative play speed (affects both pitch and speed)
            soloud->setRelativePlaySpeed(handle, playbackPitch);

            if(cv::debug_snd.getBool())
                debugLog(
                    "SoLoudSoundEngine: Playing non-streaming audio with playbackPitch={:f} (pitch={:f} * "
                    "soundPitch={:f}, soundSpeed={:f})\n",
                    playbackPitch, pitch, soloudSound->getPitch(), soloudSound->getSpeed());
        }

        // we started it paused, so unpause it now
        soloud->setPause(handle, false);

        soloudSound->setLastPlayTime(engine->getTime());

        return true;
    }

    if(cv::debug_snd.getBool()) debugLog("SoLoudSoundEngine: Failed to play sound {:s}\n", soloudSound->sFilePath);

    return false;
}

void SoLoudSoundEngine::pause(Sound *snd) {
    if(!this->isReady() || snd == nullptr || !snd->isReady()) return;

    auto *soloudSound = snd->as<SoLoudSound>();
    if(!soloudSound || soloudSound->handle == 0) return;

    soloud->setPause(soloudSound->handle, true);
    soloudSound->setLastPlayTime(0.0);
}

void SoLoudSoundEngine::stop(Sound *snd) {
    if(!this->isReady() || snd == nullptr || !snd->isReady()) return;

    auto *soloudSound = snd->as<SoLoudSound>();
    if(!soloudSound || soloudSound->handle == 0) return;

    soloudSound->setPositionMS(0);
    soloudSound->setLastPlayTime(0.0);
    soloudSound->setFrequency(0.0);
    soloud->stop(soloudSound->handle);
    soloudSound->handle = 0;
}

void SoLoudSoundEngine::setOutputDeviceByName(const UString &desiredDeviceName) {
    for(const auto &device : this->outputDevices) {
        if(device.name == desiredDeviceName) {
            this->setOutputDeviceInt(device);
            return;
        }
    }

    debugLog("couldn't find output device \"{:s}\"!\n", desiredDeviceName.toUtf8());
    this->initializeOutputDevice(this->getDefaultDevice());  // initialize default
}

void SoLoudSoundEngine::setOutputDevice(const SoundEngine::OUTPUT_DEVICE &device) {
    this->setOutputDeviceInt(device, false);
}

bool SoLoudSoundEngine::setOutputDeviceInt(const SoundEngine::OUTPUT_DEVICE &desiredDevice, bool force) {
    if(force || !this->bReady || !this->bWasBackendEverReady) {
        // TODO: This is blocking main thread, can freeze for a long time on some sound cards
        auto previous = this->currentOutputDevice;
        if(!this->initializeOutputDevice(desiredDevice)) {
            if((desiredDevice.id == previous.id && desiredDevice.driver == previous.driver) ||
               !this->initializeOutputDevice(previous)) {
                // We failed to reinitialize the device, don't start an infinite loop, just give up
                this->currentOutputDevice = {};
                return false;
            }
        }
        return true;
    }
    for(const auto &device : this->outputDevices) {
        if(device.name == desiredDevice.name) {
            if(device.id != this->currentOutputDevice.id &&
               !(device.isDefault && this->currentOutputDevice.isDefault)) {
                auto previous = this->currentOutputDevice;
                if(cv::debug_snd.getBool()) {
                    debugLog("switching devices, current id {} default {}, new id {} default {}\n", previous.id,
                             previous.isDefault, device.id, device.isDefault);
                }
                if(!this->initializeOutputDevice(desiredDevice)) this->initializeOutputDevice(previous);
            } else {
                // multiple ids can map to the same device (e.g. default device), just update the name
                this->currentOutputDevice.name = device.name;
                debugLog("\"{:s}\" already is the current device.\n", desiredDevice.name);
                return false;
            }

            return true;
        }
    }

    debugLog("couldn't find output device \"{:s}\"!\n", desiredDevice.name);
    return false;
}

// delay setting these until after everything is fully init, so we don't restart multiple times while reading config
void SoLoudSoundEngine::allowInternalCallbacks() {
    // convar callbacks
    cv::snd_freq.setCallback(SA::MakeDelegate<&SoLoudSoundEngine::restart>(this));
    cv::snd_restart.setCallback(SA::MakeDelegate<&SoLoudSoundEngine::restart>(this));

    static auto backendSwitchCB = [&](const UString &arg) -> void {
        const bool nowSDL = arg.findIgnoreCase("sdl") != -1;
        const auto curDriver = soundEngine->getOutputDriverType();
        // don't do anything if we're already ready with the same output driver
        if(this->bWasBackendEverReady &&
           ((nowSDL && curDriver == OutputDriver::SOLOUD_SDL) || (!nowSDL && curDriver == OutputDriver::SOLOUD_MA)))
            return;

        // needed due to different device enumeration between backends
        this->bWasBackendEverReady = false;
        this->restart();
    };

    cv::snd_soloud_backend.setCallback(backendSwitchCB);
    cv::snd_sanity_simultaneous_limit.setCallback(SA::MakeDelegate<&SoLoudSoundEngine::onMaxActiveChange>(this));
    cv::snd_output_device.setCallback(SA::MakeDelegate<&SoLoudSoundEngine::setOutputDeviceByName>(this));

    bool doRestart = !this->bWasBackendEverReady ||  //
                     (cv::snd_freq.getDefaultFloat() != cv::snd_freq.getFloat()) ||
                     (cv::snd_restart.getDefaultFloat() != cv::snd_restart.getFloat()) ||
                     (cv::snd_soloud_backend.getDefaultString() != cv::snd_soloud_backend.getString());

    if(doRestart) {
        this->restart();
    }

    bool doMaxActive =
        cv::snd_sanity_simultaneous_limit.getDefaultFloat() != cv::snd_sanity_simultaneous_limit.getFloat();
    if(doMaxActive) {
        this->onMaxActiveChange(cv::snd_sanity_simultaneous_limit.getFloat());
    }

    // if we restarted already, then we already set the output device to the desired one
    bool doChangeOutput = !doRestart && (cv::snd_output_device.getDefaultString() != cv::snd_output_device.getString());
    if(doChangeOutput) {
        this->setOutputDeviceByName(UString{cv::snd_output_device.getString()});
    }
}

SoLoudSoundEngine::~SoLoudSoundEngine() {
    if(soloud && this->isReady()) {
        soloud->deinit();
    }
    soloud.reset();
}

void SoLoudSoundEngine::setMasterVolume(float volume) {
    if(!this->isReady()) return;

    this->fMasterVolume = std::clamp<float>(volume, 0.0f, 1.0f);

    // if (cv::debug_snd.getBool())
    // 	debugLog("setting global volume to {:f}\n", fVolume);
    soloud->setGlobalVolume(this->fMasterVolume);
}

void SoLoudSoundEngine::setVolumeGradual(SOUNDHANDLE handle, float targetVol, float fadeTimeMs) {
    if(!this->isReady() || handle == 0 || !soloud->isValidVoiceHandle(handle)) return;

    soloud->setVolume(handle, 0.0f);

    if(cv::debug_snd.getBool()) debugLog("fading in to {:.2f}\n", targetVol);

    soloud->fadeVolume(handle, targetVol, fadeTimeMs / 1000.0f);
}

void SoLoudSoundEngine::updateOutputDevices(bool printInfo) {
    if(!this->isReady())  // soloud needs to be initialized first
        return;

    using namespace SoLoud;

    const auto currentDriver = getMAorSDLCV();

    // reset these, because if the backend changed, it might enumerate devices differently
    this->mSoloudDevices.clear();
    this->outputDevices.clear();
    this->outputDevices.push_back(
        OUTPUT_DEVICE{.isDefault = true, .driver = currentDriver});  // re-add dummy default device

    debugLog("SoundEngine: Using SoLoud backend: {:s}\n", cv::snd_soloud_backend.getString());
    DeviceInfo currentDevice{}, fallbackDevice{};

    result res = soloud->getCurrentDevice(&currentDevice);
    if(res == SO_NO_ERROR) {
        // in case we can't go through devices to find the real default, use the current one as the default
        fallbackDevice = currentDevice;
        debugLog("SoundEngine: Current device: {:s} (Default: {:s})\n", currentDevice.name,
                 currentDevice.isDefault ? "Yes" : "No");
    } else {
        fallbackDevice = {.name = "Unavailable", .identifier = "", .isDefault = true, .nativeDeviceInfo = nullptr};
        this->mSoloudDevices[-1] = fallbackDevice;
    }

    DeviceInfo *devicearray{};
    unsigned int deviceCount = 0;
    res = soloud->enumerateDevices(&devicearray, &deviceCount);

    if(res == SO_NO_ERROR) {
        // sort to keep them in the same order for each query
        std::vector<DeviceInfo> devices{devicearray, devicearray + deviceCount};
        std::ranges::stable_sort(
            devices, [](const char *a, const char *b) -> bool { return strcasecmp(a, b) < 0; },
            [](const DeviceInfo &di) -> const char * { return di.name; });

        for(int d = 0; d < devices.size(); d++) {
            if(printInfo) {
                debugLog("SoundEngine: Device {}: {:s} (Default: {:s})\n", d, devices[d].name,
                         devices[d].isDefault ? "Yes" : "No");
            }

            UString originalDeviceName{&devices[d].name[0]};
            OUTPUT_DEVICE soundDevice;
            soundDevice.id = d;
            soundDevice.name = originalDeviceName;
            soundDevice.enabled = true;
            soundDevice.isDefault = devices[d].isDefault;
            soundDevice.driver = currentDriver;

            // avoid duplicate names
            int duplicateNameCounter = 2;
            while(true) {
                bool foundDuplicateName = false;
                for(const auto &existingDevice : this->outputDevices) {
                    if(existingDevice.name == soundDevice.name) {
                        foundDuplicateName = true;
                        soundDevice.name = originalDeviceName;
                        soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));
                        duplicateNameCounter++;
                        break;
                    }
                }

                if(!foundDuplicateName) break;
            }
            if(cv::debug_snd.getBool()) {
                debugLog("added device id {} name {} iteration (d) {}\n", soundDevice.id, soundDevice.name, d);
            }

            // SDL3 backend has a special "default device", replace the engine default with that one and don't add it
            if(soundDevice.isDefault && soundDevice.name.findIgnoreCase("default") != -1) {
                soundDevice.id = -1;
                this->outputDevices[0] = soundDevice;
                this->mSoloudDevices[-1] = {devices[d]};
            } else {
                // otherwise add it as a new device with a real id
                this->outputDevices.push_back(soundDevice);
                if(soundDevice.isDefault) {
                    this->mSoloudDevices[-1] = {devices[d]};
                }
                this->mSoloudDevices[d] = {devices[d]};
            }
        }
    } else {
        // failed enumeration, use fallback
        this->mSoloudDevices[-1] = fallbackDevice;
    }
}

bool SoLoudSoundEngine::initializeOutputDevice(const OUTPUT_DEVICE &device) {
    // run callbacks pt. 1
    if(this->restartCBs[0] != nullptr) this->restartCBs[0]();
    debugLog("id {} name {}\n", device.id, device.name);

    // cleanup potential previous device
    if(this->isReady()) {
        soloud->deinit();
        this->bReady = false;
    }

    // basic flags
    // roundoff clipping alters/"damages" the waveform, but it sounds weird without it
    auto flags = SoLoud::Soloud::CLIP_ROUNDOFF; /* | SoLoud::Soloud::NO_FPU_REGISTER_CHANGE; */
    auto backend =
        (getMAorSDLCV() == SoundEngine::OutputDriver::SOLOUD_MA) ? SoLoud::Soloud::MINIAUDIO : SoLoud::Soloud::SDL3;

    unsigned int sampleRate =
        (cv::snd_freq.getVal<unsigned int>() == static_cast<unsigned int>(cv::snd_freq.getDefaultFloat())
             ? (unsigned int)SoLoud::Soloud::AUTO
             : cv::snd_freq.getVal<unsigned int>());
    if(sampleRate < 22500 || sampleRate > 192000) sampleRate = SoLoud::Soloud::AUTO;

    unsigned int bufferSize = (cv::snd_soloud_buffer.getVal<unsigned int>() ==
                                       static_cast<unsigned int>(cv::snd_soloud_buffer.getDefaultFloat())
                                   ? (unsigned int)SoLoud::Soloud::AUTO
                                   : cv::snd_soloud_buffer.getVal<unsigned int>());
    if(bufferSize > 2048) bufferSize = SoLoud::Soloud::AUTO;

    // use stereo output
    constexpr unsigned int channels = 2;

    // setup some SDL hints in case the SDL backend is used
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, PACKAGE_NAME, SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE, "game", SDL_HINT_OVERRIDE);

    // initialize a new soloud instance
    SoLoud::result result = soloud->init(flags, backend, sampleRate, bufferSize, channels);

    if(result != SoLoud::SO_NO_ERROR) {
        this->bReady = false;
        this->bWasBackendEverReady = false;
        engine->showMessageError("Sound Error", UString::format("SoLoud::Soloud::init() failed (%i)!", result));
        return false;
    }

    this->bReady = true;
    this->bWasBackendEverReady = true;

    {
        // populate devices array and set the desired output
        this->updateOutputDevices(true);

        if(device.id != this->currentOutputDevice.id && this->outputDevices.size() > 1 &&
           strcmp(this->mSoloudDevices[device.id].identifier,
                  this->mSoloudDevices[this->currentOutputDevice.id].identifier) != 0) {
            if(soloud->setDevice(&this->mSoloudDevices[device.id].identifier[0]) != SoLoud::SO_NO_ERROR) {
                // reset to default
                this->currentOutputDevice = this->outputDevices[0];
            }
        }

        this->currentOutputDevice = device;
        if(UString{&this->mSoloudDevices[device.id].name[0]}.findIgnoreCase("default") != -1) {
            // replace engine default device name (e.g. Default Playback Device, for SDL)
            this->currentOutputDevice.name = {&this->mSoloudDevices[device.id].name[0]};
        }

        if(this->currentOutputDevice.isDefault && this->currentOutputDevice.id == -1) {
            // update "fake" default convar string (avoid saving to configs)
            cv::snd_output_device.setDefaultString(this->currentOutputDevice.name);
        }

        cv::snd_output_device.setValue(this->currentOutputDevice.name, false);
    }

    // it's 0.95 by default, for some reason
    soloud->setPostClipScaler(1.0f);

    cv::snd_freq.setValue(soloud->getBackendSamplerate(),
                          false);  // set the cvar to match the actual output sample rate (without running callbacks)
    cv::snd_soloud_backend.setValue(soloud->getBackendString(), false);  // ditto
    if(cv::snd_soloud_buffer.getFloat() != cv::snd_soloud_buffer.getDefaultFloat())
        cv::snd_soloud_buffer.setValue(soloud->getBackendBufferSize(),
                                       false);  // ditto (but only if explicitly non-default was requested already)

    this->onMaxActiveChange(cv::snd_sanity_simultaneous_limit.getFloat());

    debugLog(
        "SoundEngine: Initialized SoLoud with output device = \"{:s}\" flags: 0x{:x}, backend: {:s}, sampleRate: {}, "
        "bufferSize: {}, channels: {}, "
        "maxActiveVoiceCount: {}\n",
        this->currentOutputDevice.name.toUtf8(), static_cast<unsigned int>(flags), soloud->getBackendString(),
        soloud->getBackendSamplerate(), soloud->getBackendBufferSize(), soloud->getBackendChannels(),
        this->iMaxActiveVoices);

    // init global volume
    this->setMasterVolume(this->fMasterVolume);

    // run callbacks pt. 2
    if(this->restartCBs[1] != nullptr) {
        this->restartCBs[1]();
    }
    return true;
}

void SoLoudSoundEngine::onMaxActiveChange(float newMax) {
    if(!soloud || !this->isReady()) return;
    const auto desired = std::clamp<unsigned int>(static_cast<unsigned int>(newMax), 64, 255);
    if(std::cmp_not_equal(soloud->getMaxActiveVoiceCount(), desired)) {
        SoLoud::result res = soloud->setMaxActiveVoiceCount(desired);
        if(res != SoLoud::SO_NO_ERROR) debugLog("SoundEngine WARNING: failed to setMaxActiveVoiceCount ({})\n", res);
    }
    this->iMaxActiveVoices = static_cast<int>(soloud->getMaxActiveVoiceCount());
    cv::snd_sanity_simultaneous_limit.setValue(this->iMaxActiveVoices, false);  // no infinite callback loop
}

#endif  // MCENGINE_FEATURE_SOLOUD
