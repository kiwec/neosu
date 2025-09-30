// Copyright (c) 2014, PG, All rights reserved.
#include "BassSoundEngine.h"
#include "SString.h"
#include "SoLoudSoundEngine.h"
#include "SoundEngine.h"

#include "Engine.h"
#include "Environment.h"
#include "ConVar.h"
#include "Logging.h"

SoundEngine *SoundEngine::initialize() {
#if !defined(MCENGINE_FEATURE_BASS) && !defined(MCENGINE_FEATURE_SOLOUD)
#error No sound backend available!
#endif
    SoundEngine *retBackend = nullptr;
    using Backend = enum SoundEngine::SndEngineType;

    auto args = env->getLaunchArgs();
    auto soundString = args["-sound"].value_or("soloud");  // default soloud
    SString::trim_inplace(soundString);
    SString::lower_inplace(soundString);

    std::vector<Backend> initOrderList;
    if(Env::cfg(AUD::SOLOUD) && Env::cfg(AUD::BASS)) {
        // built with both backends supported, only prefer bass if explicitly passed as a launch arg
        if(soundString.contains("bass")) {
            initOrderList = {Backend::BASS, Backend::SOLOUD};
        } else {
            initOrderList = {Backend::SOLOUD, Backend::BASS};
        }
    } else {
        // just try the one we actually support
        if(Env::cfg(AUD::SOLOUD)) {
            initOrderList = {Backend::SOLOUD};
        } else {  // must be bass
            initOrderList = {Backend::BASS};
        }
    }

    for(const auto type : initOrderList) {
#ifdef MCENGINE_FEATURE_BASS
        if(type == Backend::BASS) retBackend = new BassSoundEngine();
#endif
#ifdef MCENGINE_FEATURE_SOLOUD
        if(type == Backend::SOLOUD) retBackend = new SoLoudSoundEngine();
#endif
        if(!retBackend || !retBackend->succeeded()) {
            SAFE_DELETE(retBackend);
        } else {  // succeeded
            break;
        }
    }

    return retBackend;
}

std::vector<SoundEngine::OUTPUT_DEVICE> SoundEngine::getOutputDevices() {
    std::vector<SoundEngine::OUTPUT_DEVICE> outputDevices;

    for(auto &outputDevice : this->outputDevices) {
        if(outputDevice.enabled) {
            outputDevices.push_back(outputDevice);
        }
    }

    return outputDevices;
}

SoundEngine::OUTPUT_DEVICE SoundEngine::getWantedDevice() {
    auto wanted_name = cv::snd_output_device.getString();

    OUTPUT_DEVICE partial_match_fallback;
    bool fallback_found = false;

    for(auto device : this->outputDevices) {
        if(device.enabled && (device.name.utf8View() == wanted_name)) {
            return device;
        } else if(!fallback_found && wanted_name.length() > 2 &&
                  (SString::contains_ncase(wanted_name, device.name.toUtf8()) ||
                   SString::contains_ncase(device.name.toUtf8(), wanted_name))) {
            // accept the first partial match (both ways) (if any) as a fallback
            fallback_found = true;
            partial_match_fallback = device;
        }
    }

    if(fallback_found) {
        return partial_match_fallback;
    }

    debugLog("Could not find sound device '{:s}', initializing default one instead.", wanted_name);
    return this->getDefaultDevice();
}

SoundEngine::OUTPUT_DEVICE SoundEngine::getDefaultDevice() {
    for(auto device : this->outputDevices) {
        if(device.enabled && device.isDefault) {
            return device;
        }
    }

    debugLog("Could not find a working sound device!");
    return {
        .id = 0,
        .enabled = true,
        .isDefault = true,
        .name = "No sound",
        .driver = OutputDriver::NONE,
    };
}
