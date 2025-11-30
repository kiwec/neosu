#pragma once
// Copyright (c) 2025, WH, All rights reserved.
#ifndef BASS_MANAGER_H
#define BASS_MANAGER_H

#include "config.h"

#if defined(MCENGINE_FEATURE_BASS)
// need to include before bass headers because namespace things
#include <cstdint>  // IWYU pragma: keep
#include <string>

// can't be namespaced
#ifdef MCENGINE_PLATFORM_WINDOWS

#include "WinDebloatDefs.h"

#ifdef WINAPI_FAMILY
#include <winapifamily.h>
#endif
#include <wtypes.h>
typedef unsigned __int64 QWORD;
#endif

// shove BASS declarations into their own namespace
namespace bass_EXTERN {
extern "C" {
#define NOBASSOVERLOADS
#include <bass.h>
#include <bass_fx.h>
#include <bassloud.h>
#include <bassmix.h>
#ifdef MCENGINE_PLATFORM_WINDOWS
#include <bassasio.h>
#include <basswasapi.h>
#endif
}
};  // namespace bass_EXTERN

#ifndef BASS_CONFIG_MP3_OLDGAPS
#define BASS_CONFIG_MP3_OLDGAPS 68
#endif
#ifndef BASS_CONFIG_DEV_TIMEOUT
#define BASS_CONFIG_DEV_TIMEOUT \
    70  // https://github.com/ppy/osu-framework/blob/eed788fd166540f7e219e1e48a36d0bf64f07cc4/osu.Framework/Audio/AudioManager.cs#L419
#endif
#ifndef BASS_CONFIG_VISTA_TRUEPOS
#define BASS_CONFIG_VISTA_TRUEPOS 30
#endif

#ifdef MCENGINE_PLATFORM_WINDOWS
#define BASSVERSION_REAL 0x2041129
#define BASSFXVERSION_REAL 0x2040c10
#define BASSMIXVERSION_REAL 0x2040c04
#define BASSASIOVERSION_REAL 0x1040208
#define BASSWASAPIVERSION_REAL 0x2040403
#else
#define BASSVERSION_REAL 0x2041118
#define BASSFXVERSION_REAL 0x2040c10
#define BASSMIXVERSION_REAL 0x2040c04
#endif

#define BASSLOUDVERSION_REAL 0x00000001  // TODO: lazy

namespace BassManager {
namespace BassFuncs {
// imported enums/defines
using bass_EXTERN::QWORD;
#ifndef MCENGINE_PLATFORM_WINDOWS
using bass_EXTERN::BOOL;
using bass_EXTERN::BYTE;
using bass_EXTERN::DWORD;
using bass_EXTERN::WORD;
#endif

using bass_EXTERN::BASS_3DVECTOR;
using bass_EXTERN::BASS_DEVICEINFO;
using bass_EXTERN::BASS_INFO;
using bass_EXTERN::HCHANNEL;
using bass_EXTERN::HPLUGIN;
using bass_EXTERN::HSAMPLE;
using bass_EXTERN::HSTREAM;
using bass_EXTERN::HSYNC;
using bass_EXTERN::SYNCPROC;

// bassfx enums
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_AA_FILTER_LENGTH;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_OLDPOS;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_PREVENT_CLICK;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_SEEKWINDOW_MS;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_USE_AA_FILTER;
using bass_EXTERN::BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO;

using bass_EXTERN::BASS_ATTRIB_TEMPO;
using bass_EXTERN::BASS_ATTRIB_TEMPO_FREQ;
using bass_EXTERN::BASS_ATTRIB_TEMPO_PITCH;

#ifdef MCENGINE_PLATFORM_WINDOWS
using bass_EXTERN::BASS_ASIO_INFO;
using bass_EXTERN::BASS_ASIO_DEVICEINFO;

using bass_EXTERN::BASS_WASAPI_INFO;
using bass_EXTERN::BASS_WASAPI_DEVICEINFO;
using bass_EXTERN::WASAPIPROC;
#endif

// define all BASS functions we'll need from the libs
#define BASS_CORE_FUNCTIONS(X)   \
    X(BASS_GetVersion)           \
    X(BASS_SetConfig)            \
    X(BASS_GetConfig)            \
    X(BASS_Init)                 \
    X(BASS_Start)                \
    X(BASS_Free)                 \
    X(BASS_GetDeviceInfo)        \
    X(BASS_SetDevice)            \
    X(BASS_ErrorGetCode)         \
    X(BASS_StreamCreateFile)     \
    X(BASS_SampleLoad)           \
    X(BASS_SampleFree)           \
    X(BASS_SampleStop)           \
    X(BASS_SampleGetChannel)     \
    X(BASS_ChannelPlay)          \
    X(BASS_ChannelPause)         \
    X(BASS_ChannelFree)          \
    X(BASS_ChannelStop)          \
    X(BASS_ChannelSetSync)       \
    X(BASS_ChannelRemoveSync)    \
    X(BASS_ChannelSetAttribute)  \
    X(BASS_ChannelGetAttribute)  \
    X(BASS_ChannelSetPosition)   \
    X(BASS_ChannelGetPosition)   \
    X(BASS_ChannelGetLength)     \
    X(BASS_ChannelGetData)       \
    X(BASS_ChannelFlags)         \
    X(BASS_ChannelIsActive)      \
    X(BASS_ChannelBytes2Seconds) \
    X(BASS_ChannelSeconds2Bytes) \
    X(BASS_ChannelSet3DPosition) \
    X(BASS_Set3DPosition)        \
    X(BASS_Apply3D)              \
    X(BASS_StreamFree)           \
    X(BASS_PluginLoad)           \
    X(BASS_PluginEnable)         \
    X(BASS_PluginFree)           \
    X(BASS_PluginGetInfo)

#define BASS_FX_FUNCTIONS(X) \
    X(BASS_FX_GetVersion)    \
    X(BASS_FX_TempoCreate)

#define BASS_MIX_FUNCTIONS(X)        \
    X(BASS_Mixer_GetVersion)         \
    X(BASS_Mixer_ChannelFlags)       \
    X(BASS_Mixer_ChannelGetMixer)    \
    X(BASS_Mixer_ChannelRemove)      \
    X(BASS_Mixer_ChannelGetPosition) \
    X(BASS_Mixer_ChannelSetPosition) \
    X(BASS_Mixer_StreamCreate)       \
    X(BASS_Mixer_StreamAddChannel)

#define BASS_LOUD_FUNCTIONS(X)  \
    X(BASS_Loudness_GetVersion) \
    X(BASS_Loudness_Start)      \
    X(BASS_Loudness_GetLevel)   \
    X(BASS_Loudness_Stop)

#ifdef MCENGINE_PLATFORM_WINDOWS
#define BASS_ASIO_FUNCTIONS(X)     \
    X(BASS_ASIO_GetVersion)        \
    X(BASS_ASIO_Init)              \
    X(BASS_ASIO_ControlPanel)      \
    X(BASS_ASIO_GetInfo)           \
    X(BASS_ASIO_GetRate)           \
    X(BASS_ASIO_GetDeviceInfo)     \
    X(BASS_ASIO_ChannelEnableBASS) \
    X(BASS_ASIO_Start)             \
    X(BASS_ASIO_GetLatency)        \
    X(BASS_ASIO_Free)              \
    X(BASS_ASIO_ChannelSetVolume)

#define BASS_WASAPI_FUNCTIONS(X) \
    X(BASS_WASAPI_GetVersion)    \
    X(BASS_WASAPI_GetInfo)       \
    X(BASS_WASAPI_GetDeviceInfo) \
    X(BASS_WASAPI_Init)          \
    X(BASS_WASAPI_Start)         \
    X(BASS_WASAPI_Free)          \
    X(BASS_WASAPI_SetVolume)
#else
#define BASS_ASIO_FUNCTIONS(X)
#define BASS_WASAPI_FUNCTIONS(X)
#endif

#define ALL_BASS_FUNCTIONS(X) \
    BASS_CORE_FUNCTIONS(X)    \
    BASS_FX_FUNCTIONS(X) BASS_MIX_FUNCTIONS(X) BASS_LOUD_FUNCTIONS(X) BASS_ASIO_FUNCTIONS(X) BASS_WASAPI_FUNCTIONS(X)

// generate the type definitions and declarations
#define DECLARE_BASS_FUNCTION(name)                \
    using name##_t = decltype(&bass_EXTERN::name); \
    extern name##_t name;
// clang-format off

	ALL_BASS_FUNCTIONS(DECLARE_BASS_FUNCTION)
};
    // open the libraries and populate the function pointers
    bool init();
	// close the libraries (BassSoundEngine destructor)
	void cleanup();
    bool isLoaded();

	std::string getFailedLoad();

	std::string printBassError(const std::string &context, int code);
    std::string getErrorString(int code = (-0x7fffffff - 1)); // anything it takes to avoid including another header within a header
//clang-format on
}; // namespace BassManager

using namespace BassManager::BassFuncs;

#endif

#endif // BASS_MANAGER_H
