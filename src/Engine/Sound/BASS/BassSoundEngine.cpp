// Copyright (c) 2014, PG, All rights reserved.
#include "BassSoundEngine.h"

#ifdef MCENGINE_FEATURE_BASS

#include "Beatmap.h"
#include "CBaseUILabel.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"

#include <cmath>
#include <utility>
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/SongBrowser.h"
#include "Sound.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "CBaseUISlider.h"

DWORD BassSoundEngine::ASIO_clamp(BASS_ASIO_INFO info, DWORD buflen) {
    if(std::cmp_equal(buflen, -1)) return info.bufpref;
    if(buflen < info.bufmin) return info.bufmin;
    if(buflen > info.bufmax) return info.bufmax;
    if(info.bufgran == 0) return buflen;

    if(info.bufgran == -1) {
        // Buffer lengths are only allowed in powers of 2
        for(int oksize = info.bufmin; std::cmp_less_equal(oksize, info.bufmax); oksize *= 2) {
            if(std::cmp_equal(oksize, buflen)) {
                return buflen;
            } else if(std::cmp_greater(oksize, buflen)) {
                oksize /= 2;
                return oksize;
            }
        }

        // Unreachable
        return info.bufpref;
    } else {
        // Buffer lengths are only allowed in multiples of info.bufgran
        buflen -= info.bufmin;
        buflen = (buflen / info.bufgran) * info.bufgran;
        buflen += info.bufmin;
        return buflen;
    }
}
#endif

BassSoundEngine::BassSoundEngine() : SoundEngine() {
    if(!BassManager::init())  // this checks the library versions as well
    {
        engine->showMessageErrorFatal(
            "Fatal Sound Error", UString::fmt("Failed to load BASS feature: {:s} !", BassManager::getFailedLoad()));
        engine->shutdown();
        return;
    }

    BASS_SetConfig(BASS_CONFIG_BUFFER, 100);

    // all beatmaps timed to non-iTunesSMPB + 529 sample deletion offsets on old dlls pre 2015
    BASS_SetConfig(BASS_CONFIG_MP3_OLDGAPS, 1);

    // avoids lag/jitter in BASS_Mixer_ChannelGetPosition() shortly after a BASS_ChannelPlay() after loading/silence
    BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);

    // if set to 1, increases sample playback latency by 10 ms
    BASS_SetConfig(BASS_CONFIG_VISTA_TRUEPOS, 0);
    this->currentOutputDevice = {
        .id = 0,
        .enabled = true,
        .isDefault = true,
        .name = "No sound",
        .driver = OutputDriver::NONE,
    };
}

BassSoundEngine::~BassSoundEngine() { BassManager::cleanup(); }

void BassSoundEngine::updateOutputDevices(bool /*printInfo*/) {
    this->outputDevices.clear();

    BASS_DEVICEINFO deviceInfo;
    for(int d = 0; (BASS_GetDeviceInfo(d, &deviceInfo) == TRUE); d++) {
        const bool isEnabled = (deviceInfo.flags & BASS_DEVICE_ENABLED);
        const bool isDefault = (deviceInfo.flags & BASS_DEVICE_DEFAULT);

        SoundEngine::OUTPUT_DEVICE soundDevice;
        soundDevice.id = d;
        soundDevice.name = deviceInfo.name;
        soundDevice.enabled = isEnabled;
        soundDevice.isDefault = isDefault;

        // avoid duplicate names
        int duplicateNameCounter = 2;
        while(true) {
            bool foundDuplicateName = false;
            for(auto &outputDevice : this->outputDevices) {
                if(outputDevice.name == soundDevice.name) {
                    foundDuplicateName = true;

                    soundDevice.name = deviceInfo.name;
                    soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));

                    duplicateNameCounter++;

                    break;
                }
            }

            if(!foundDuplicateName) break;
        }

        soundDevice.driver = OutputDriver::BASS;
        this->outputDevices.push_back(soundDevice);

        debugLog("DSOUND: Device {:d} = \"{:s}\", enabled = {:d}, default = {:d}\n", d, deviceInfo.name, (int)isEnabled,
                 (int)isDefault);
    }

#ifdef _WIN32
    BASS_ASIO_DEVICEINFO asioDeviceInfo;
    for(int d = 0; (BASS_ASIO_GetDeviceInfo(d, &asioDeviceInfo) == TRUE); d++) {
        SoundEngine::OUTPUT_DEVICE soundDevice;
        soundDevice.id = d;
        soundDevice.name = asioDeviceInfo.name;
        soundDevice.enabled = true;
        soundDevice.isDefault = false;

        // avoid duplicate names
        int duplicateNameCounter = 2;
        while(true) {
            bool foundDuplicateName = false;
            for(size_t i = 0; i < this->outputDevices.size(); i++) {
                if(this->outputDevices[i].name == soundDevice.name) {
                    foundDuplicateName = true;

                    soundDevice.name = deviceInfo.name;
                    soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));

                    duplicateNameCounter++;

                    break;
                }
            }

            if(!foundDuplicateName) break;
        }

        soundDevice.driver = OutputDriver::BASS_ASIO;
        soundDevice.name.append(" [ASIO]");
        this->outputDevices.push_back(soundDevice);

        debugLog("ASIO: Device {:d} = \"{:s}\"\n", d, asioDeviceInfo.name);
    }

    BASS_WASAPI_DEVICEINFO wasapiDeviceInfo;
    for(int d = 0; (BASS_WASAPI_GetDeviceInfo(d, &wasapiDeviceInfo) == TRUE); d++) {
        const bool isEnabled = (wasapiDeviceInfo.flags & BASS_DEVICE_ENABLED);
        const bool isDefault = (wasapiDeviceInfo.flags & BASS_DEVICE_DEFAULT);
        const bool isInput = (wasapiDeviceInfo.flags & BASS_DEVICE_INPUT);
        if(isInput) continue;

        SoundEngine::OUTPUT_DEVICE soundDevice;
        soundDevice.id = d;
        soundDevice.name = wasapiDeviceInfo.name;
        soundDevice.enabled = isEnabled;
        soundDevice.isDefault = isDefault;

        // avoid duplicate names
        int duplicateNameCounter = 2;
        while(true) {
            bool foundDuplicateName = false;
            for(size_t i = 0; i < this->outputDevices.size(); i++) {
                if(this->outputDevices[i].name == soundDevice.name) {
                    foundDuplicateName = true;

                    soundDevice.name = wasapiDeviceInfo.name;
                    soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));

                    duplicateNameCounter++;

                    break;
                }
            }

            if(!foundDuplicateName) break;
        }

        soundDevice.driver = OutputDriver::BASS_WASAPI;
        soundDevice.name.append(" [WASAPI]");
        this->outputDevices.push_back(soundDevice);

        debugLog("WASAPI: Device {:d} = \"{:s}\", enabled = {:d}, default = {:d}\n", d, wasapiDeviceInfo.name,
                 (int)isEnabled, (int)isDefault);
    }
#endif
}

// The BASS mixer is used for every sound driver, but it's useful to be able to
// initialize it later on some drivers where we know the best available frequency.
bool BassSoundEngine::init_bass_mixer(const SoundEngine::OUTPUT_DEVICE &device) {
    auto bass_flags = BASS_DEVICE_STEREO | BASS_DEVICE_FREQ | BASS_DEVICE_NOSPEAKER;
    auto freq = cv::snd_freq.getInt();

    // We initialize a "No sound" device for measuring loudness and mixing sounds,
    // regardless of the device we'll use for actual output.
    if(!BASS_Init(0, freq, bass_flags | BASS_DEVICE_SOFTWARE, NULL, NULL)) {
        auto code = BASS_ErrorGetCode();
        if(code != BASS_ERROR_ALREADY) {
            this->ready_since = -1.0;
            debugLog("BASS_Init(0) failed.\n");
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }
    }

    if(device.driver == OutputDriver::BASS) {
        if(!BASS_Init(device.id, freq, bass_flags | BASS_DEVICE_SOFTWARE, NULL, NULL)) {
            this->ready_since = -1.0;
            debugLog("BASS_Init({:d}) errored out.\n", device.id);
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }
    }

    auto mixer_flags = BASS_SAMPLE_FLOAT | BASS_MIXER_NONSTOP | BASS_MIXER_RESUME;
    if(device.driver != OutputDriver::BASS) mixer_flags |= BASS_STREAM_DECODE | BASS_MIXER_POSEX;
    this->g_bassOutputMixer = BASS_Mixer_StreamCreate(freq, 2, mixer_flags);
    if(this->g_bassOutputMixer == 0) {
        this->ready_since = -1.0;
        debugLog("BASS_Mixer_StreamCreate() failed.\n");
        osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
        return false;
    }

    // Disable buffering to lower latency on regular BASS output
    // This has no effect on ASIO/WASAPI since for those the mixer is a decode stream
    BASS_ChannelSetAttribute(this->g_bassOutputMixer, BASS_ATTRIB_BUFFER, 0.f);

    // Switch to "No sound" device for all future sound processing
    // Only g_bassOutputMixer will be output to the actual device!
    BASS_SetDevice(0);
    return true;
}

bool BassSoundEngine::initializeOutputDevice(const SoundEngine::OUTPUT_DEVICE &device) {
    debugLog("BassSoundEngine: initializeOutputDevice( {:s} ) ...\n", device.name.toUtf8());

    this->shutdown();

    // We compensate for latency via BASS_ATTRIB_MIXER_LATENCY
    cv::universal_offset_hardcoded.setValue(0.f);

    if(device.driver == OutputDriver::NONE || (device.driver == OutputDriver::BASS && device.id == 0)) {
        this->ready_since = -1.0;
        this->currentOutputDevice = device;
        cv::snd_output_device.setValue(this->currentOutputDevice.name);
        debugLog("BassSoundEngine: Output Device = \"{:s}\"\n", this->currentOutputDevice.name.toUtf8());

        if(osu && osu->optionsMenu) {
            osu->optionsMenu->scheduleLayoutUpdate();
        }

        return true;
    }

    if(device.driver == OutputDriver::BASS) {
        // Normal output: render playback buffer every 5ms (buffer is 30ms large)
        BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 5);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 1);
    } else if(device.driver == OutputDriver::BASS_ASIO || device.driver == OutputDriver::BASS_WASAPI) {
        // ASIO/WASAPI: let driver decide when to render playback buffer
        BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
    }

    // allow users to override some defaults (but which may cause beatmap desyncs)
    // we only want to set these if their values have been explicitly modified (to avoid sideeffects in the default
    // case, and for my sanity)
    {
        if(cv::snd_updateperiod.getFloat() != cv::snd_updateperiod.getDefaultFloat())
            BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, cv::snd_updateperiod.getInt());

        if(cv::snd_dev_buffer.getFloat() != cv::snd_dev_buffer.getDefaultFloat())
            BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, cv::snd_dev_buffer.getInt());

        if(cv::snd_dev_period.getFloat() != cv::snd_dev_period.getDefaultFloat())
            BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, cv::snd_dev_period.getInt());
    }

    // When the driver is BASS, we can init the mixer immediately
    // On other drivers, we'd rather get the sound card's frequency first
    if(device.driver == OutputDriver::BASS) {
        if(!this->init_bass_mixer(device)) {
            return false;
        }
    }

#ifdef _WIN32
    if(device.driver == OutputDriver::BASS_ASIO) {
        if(!BASS_ASIO_Init(device.id, 0)) {
            ready_since = -1.0;
            debugLog("BASS_ASIO_Init() failed.\n");
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }

        double sample_rate = BASS_ASIO_GetRate();
        if(sample_rate == 0.0) {
            sample_rate = cv::snd_freq.getFloat();
            debugLog("ASIO: BASS_ASIO_GetRate() returned 0, using {:f} instead!\n", sample_rate);
        } else {
            cv::snd_freq.setValue(sample_rate);
        }
        if(!init_bass_mixer(device)) {
            return false;
        }

        BASS_ASIO_INFO info{};
        BASS_ASIO_GetInfo(&info);
        auto bufsize = cv::asio_buffer_size.getVal<unsigned long>();
        bufsize = ASIO_clamp(info, bufsize);

        if(osu && osu->optionsMenu) {
            auto slider = osu->optionsMenu->asioBufferSizeSlider;
            slider->setBounds(info.bufmin, info.bufmax);
            slider->setKeyDelta(info.bufgran == -1 ? info.bufmin : info.bufgran);
        }

        if(!BASS_ASIO_ChannelEnableBASS(false, 0, g_bassOutputMixer, true)) {
            ready_since = -1.0;
            debugLog("BASS_ASIO_ChannelEnableBASS() failed.\n");
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }

        if(!BASS_ASIO_Start(bufsize, 0)) {
            ready_since = -1.0;
            debugLog("BASS_ASIO_Start() failed.\n");
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }

        double wanted_latency = 1000.0 * cv::asio_buffer_size.getFloat() / sample_rate;
        double actual_latency = 1000.0 * (double)BASS_ASIO_GetLatency(false) / sample_rate;
        BASS_ChannelSetAttribute(g_bassOutputMixer, BASS_ATTRIB_MIXER_LATENCY, actual_latency / 1000.0);
        debugLog("ASIO: wanted {:f} ms, got {:f} ms latency. Sample rate: {:f} Hz\n", wanted_latency, actual_latency,
                 sample_rate);
    }

    if(device.driver == OutputDriver::BASS_WASAPI) {
        const float bufferSize =
            std::round(cv::win_snd_wasapi_buffer_size.getFloat() * 1000.0f) / 1000.0f;  // in seconds
        const float updatePeriod =
            std::round(cv::win_snd_wasapi_period_size.getFloat() * 1000.0f) / 1000.0f;  // in seconds

        BASS_WASAPI_DEVICEINFO info;
        if(!BASS_WASAPI_GetDeviceInfo(device.id, &info)) {
            debugLog("WASAPI: Failed to get device info\n");
            return false;
        }
        cv::snd_freq.setValue(info.mixfreq);
        if(!init_bass_mixer(device)) {
            return false;
        }

        // BASS_MIXER_NONSTOP prevents some sound cards from going to sleep when there is no output
        auto flags = BASS_MIXER_NONSTOP;

#ifdef _WIN64
        // BASS_WASAPI_RAW ignores sound "enhancements" that some sound cards offer (adds latency)
        // It is only available on Windows 8.1 or above
        flags |= BASS_WASAPI_RAW;
#endif

        if(cv::win_snd_wasapi_exclusive.getBool()) {
            // BASS_WASAPI_EXCLUSIVE makes neosu have exclusive output to the sound card
            // BASS_WASAPI_AUTOFORMAT chooses the best matching sample format, BASSWASAPI doesn't resample in exclusive
            // mode
            flags |= BASS_WASAPI_EXCLUSIVE | BASS_WASAPI_AUTOFORMAT;
        }
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#endif
        if(!BASS_WASAPI_Init(device.id, 0, 0, flags, bufferSize, updatePeriod, WASAPIPROC_BASS,
                             (void *)g_bassOutputMixer)) {
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
            ready_since = -1.0;
            debugLog("BASS_WASAPI_Init() failed.\n");
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }

        if(!BASS_WASAPI_Start()) {
            ready_since = -1.0;
            debugLog("BASS_WASAPI_Start() failed.\n");
            osu->notificationOverlay->addToast(BassManager::getErrorUString(), ERROR_TOAST);
            return false;
        }

        BASS_ChannelSetAttribute(g_bassOutputMixer, BASS_ATTRIB_MIXER_LATENCY,
                                 cv::win_snd_wasapi_buffer_size.getFloat());
    }
#endif

    this->ready_since = engine->getTime();
    this->currentOutputDevice = device;
    cv::snd_output_device.setValue(this->currentOutputDevice.name);
    debugLog("BassSoundEngine: Output Device = \"{:s}\"\n", this->currentOutputDevice.name.toUtf8());

    if(osu && osu->optionsMenu) {
        osu->optionsMenu->scheduleLayoutUpdate();
    }

    return true;
}

void BassSoundEngine::restart() { this->setOutputDevice(this->currentOutputDevice); }

void BassSoundEngine::shutdown() {
    VolNormalization::abort();

    if(this->currentOutputDevice.driver == OutputDriver::BASS) {
        BASS_SetDevice(this->currentOutputDevice.id);
        BASS_Free();
        BASS_SetDevice(0);
    }

#ifdef _WIN32
    if(this->currentOutputDevice.driver == OutputDriver::BASS_ASIO) {
        BASS_ASIO_Free();
    } else if(this->currentOutputDevice.driver == OutputDriver::BASS_WASAPI) {
        BASS_WASAPI_Free();
    }
#endif

    this->ready_since = -1.0;
    this->g_bassOutputMixer = 0;
    BASS_Free();  // free "No sound" device
}

bool BassSoundEngine::play(Sound *snd, float pan, float pitch) {
    auto [bassSound, channel] = GETHANDLE(BassSound, getChannel());
    if(!bassSound) {
        return false;
    }

    if(bassSound->isOverlayable() && cv::snd_restrict_play_frame.getBool()) {
        if(engine->getTime() <= bassSound->fLastPlayTime) {
            return false;
        }
    }

    if(!channel) {
        debugLog("BassSoundEngine::play() failed to get channel, errorcode {:d}\n", BASS_ErrorGetCode());
        return false;
    }

    pan = std::clamp<float>(pan, -1.0f, 1.0f);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, bassSound->fVolume);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_PAN, pan);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_NORAMP, bassSound->isStream() ? 0 : 1);
    if(pitch != 0.0f) {
        f32 freq = cv::snd_freq.getFloat();
        BASS_ChannelGetAttribute(channel, BASS_ATTRIB_FREQ, &freq);
        BASS_ChannelSetAttribute(channel, BASS_ATTRIB_FREQ, std::pow(2.0f, pitch) * freq);
    }

    BASS_ChannelFlags(channel, bassSound->isLooped() ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP);

    if(BASS_Mixer_ChannelGetMixer(channel) != 0) return false;

    auto flags = BASS_MIXER_DOWNMIX | BASS_MIXER_NORAMPIN | BASS_STREAM_AUTOFREE;
    if(!BASS_Mixer_StreamAddChannel(this->g_bassOutputMixer, channel, flags)) {
        debugLog("BASS_Mixer_StreamAddChannel() failed ({:d})!\n", BASS_ErrorGetCode());
        return false;
    }

    // Make sure the mixer is playing! Duh.
    if(this->currentOutputDevice.driver == OutputDriver::BASS) {
        if(BASS_ChannelIsActive(this->g_bassOutputMixer) != BASS_ACTIVE_PLAYING) {
            if(!BASS_ChannelPlay(this->g_bassOutputMixer, true)) {
                debugLog("BassSoundEngine::play() couldn't BASS_ChannelPlay(), errorcode {:d}\n", BASS_ErrorGetCode());
                return false;
            }
        }
    }

    bassSound->bStarted = true;
    bassSound->fChannelCreationTime = engine->getTime();
    if(bassSound->bPaused) {
        bassSound->bPaused = false;
        bassSound->fLastPlayTime = bassSound->fChannelCreationTime - (((f64)bassSound->paused_position_ms) / 1000.0);
    } else {
        bassSound->fLastPlayTime = bassSound->fChannelCreationTime;
    }

    if(cv::debug.getBool()) {
        debugLog("Playing {:s}\n", bassSound->getFilePath().c_str());
    }

    return true;
}

void BassSoundEngine::pause(Sound *snd) {
    auto [bassSound, stream] = GETHANDLE(BassSound, stream);
    if(!bassSound) {
        return;
    }

    if(!stream) {
        engine->showMessageError("Programmer Error", "Called pause on a sample!");
        return;
    }
    if(!bassSound->isPlaying()) return;

    auto pan = bassSound->getPan();
    auto pos = bassSound->getPositionMS();
    auto loop = bassSound->isLooped();
    auto speed = bassSound->getSpeed();

    // Calling BASS_Mixer_ChannelRemove automatically frees the stream due
    // to BASS_STREAM_AUTOFREE. We need to reinitialize it.
    resourceManager->reloadResource(bassSound);

    bassSound->setPositionMS(pos);
    bassSound->setSpeed(speed);
    bassSound->setPan(pan);
    bassSound->setLoop(loop);
    bassSound->bPaused = true;
    bassSound->paused_position_ms = pos;
}

void BassSoundEngine::stop(Sound *snd) {
    if(!this->isReady() || snd == NULL || !snd->isReady()) return;

    // This will stop all samples, then re-init to be ready for a play()
    resourceManager->reloadResource(snd);
}

bool BassSoundEngine::isReady() {
    if(this->ready_since == -1.0) return false;
    return this->ready_since + (double)cv::snd_ready_delay.getFloat() < engine->getTime();
}

bool BassSoundEngine::hasExclusiveOutput() {
    return this->isASIO() || (this->isWASAPI() && cv::win_snd_wasapi_exclusive.getBool());
}

void BassSoundEngine::setOutputDevice(const SoundEngine::OUTPUT_DEVICE &device) {
    bool was_playing = false;
    unsigned long prevMusicPositionMS = 0;
    if(osu->getSelectedBeatmap()->getMusic() != NULL) {
        was_playing = osu->getSelectedBeatmap()->getMusic()->isPlaying();
        prevMusicPositionMS = osu->getSelectedBeatmap()->getMusic()->getPositionMS();
    }

    // TODO: This is blocking main thread, can freeze for a long time on some sound cards
    auto previous = this->currentOutputDevice;
    if(!this->initializeOutputDevice(device)) {
        if((device.id == previous.id && device.driver == previous.driver) || !this->initializeOutputDevice(previous)) {
            // We failed to reinitialize the device, don't start an infinite loop, just give up
            this->currentOutputDevice = {
                .id = 0,
                .enabled = true,
                .isDefault = true,
                .name = "No sound",
                .driver = OutputDriver::NONE,
            };
        }
    }

    osu->optionsMenu->outputDeviceLabel->setText(this->getOutputDeviceName());
    osu->getSkin()->reloadSounds();
    osu->optionsMenu->onOutputDeviceResetUpdate();

    // start playing music again after audio device changed
    if(osu->getSelectedBeatmap()->getMusic() != NULL) {
        if(osu->isInPlayMode()) {
            osu->getSelectedBeatmap()->unloadMusic();
            osu->getSelectedBeatmap()->loadMusic(false);
            osu->getSelectedBeatmap()->getMusic()->setLoop(false);
            osu->getSelectedBeatmap()->getMusic()->setPositionMS(prevMusicPositionMS);
        } else {
            osu->getSelectedBeatmap()->unloadMusic();
            osu->getSelectedBeatmap()->select();
            osu->getSelectedBeatmap()->getMusic()->setPositionMS(prevMusicPositionMS);
        }
    }

    // resume loudness calc
    VolNormalization::start_calc(db->loudness_to_calc);

    if(was_playing) {
        osu->music_unpause_scheduled = true;
    }
}

void BassSoundEngine::setVolume(float volume) {
    if(!this->isReady()) return;

    this->fMasterVolume = std::clamp<float>(volume, 0.0f, 1.0f);
    if(this->currentOutputDevice.driver == OutputDriver::BASS_ASIO) {
#ifdef _WIN32
        BASS_ASIO_ChannelSetVolume(false, 0, this->fMasterVolume);
        BASS_ASIO_ChannelSetVolume(false, 1, this->fMasterVolume);
#endif
    } else if(this->currentOutputDevice.driver == OutputDriver::BASS_WASAPI) {
#ifdef _WIN32
        if(hasExclusiveOutput()) {
            // Device volume doesn't seem to work, so we'll use DSP instead
            BASS_ChannelSetAttribute(g_bassOutputMixer, BASS_ATTRIB_VOLDSP, this->fMasterVolume);
        } else {
            BASS_WASAPI_SetVolume(BASS_WASAPI_CURVE_WINDOWS | BASS_WASAPI_VOL_SESSION, this->fMasterVolume);
        }
#endif
    } else {
        // 0 (silent) - 10000 (full).
        BASS_SetConfig(BASS_CONFIG_GVOL_SAMPLE, (DWORD)(this->fMasterVolume * 10000));
        BASS_SetConfig(BASS_CONFIG_GVOL_STREAM, (DWORD)(this->fMasterVolume * 10000));
        BASS_SetConfig(BASS_CONFIG_GVOL_MUSIC, (DWORD)(this->fMasterVolume * 10000));
    }
}

void BassSoundEngine::onFreqChanged(float oldFreq, float newFreq) {
    if(!this->isReady() || oldFreq == newFreq) return;
    this->restart();
}

void BassSoundEngine::onParamChanged(float oldValue, float newValue) {
    const int oldValueMS = static_cast<int>(std::round(oldValue * 1000.0f));
    const int newValueMS = static_cast<int>(std::round(newValue * 1000.0f));

    if(oldValueMS != newValueMS) this->restart();
}

#endif
