// Copyright (c) 2025, WH, All rights reserved.
#include "BassManager.h"

#if defined(MCENGINE_FEATURE_BASS)

#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "Logging.h"
#include "Environment.h"

#include <SDL3/SDL_loadso.h>

namespace BassManager {
namespace BassFuncs {
template <typename T>
T loadFunction(SDL_SharedObject *lib, const char *funcName) {
    T func = reinterpret_cast<T>(SDL_LoadFunction(lib, funcName));
    if(!func) debugLog("BassManager: Failed to load function {:s}: {:s}", funcName, SDL_GetError());
    return func;
}

#ifdef MCENGINE_PLATFORM_WINDOWS
#define LPREFIX ""
#define LSUFFIX ".dll"
#else
#define LPREFIX "lib"
#define LSUFFIX ".so"
#endif

#define LNAME(x) LPREFIX #x LSUFFIX

// define all the libraries with their properties
// (name, version_func, expected_version, func_group)
#ifdef MCENGINE_PLATFORM_WINDOWS
#define _BASS_WIN_LIBRARIES(X)                                                   \
    X(bassasio, BASS_ASIO_GetVersion, BASSASIOVERSION_REAL, BASS_ASIO_FUNCTIONS) \
    X(basswasapi, BASS_WASAPI_GetVersion, BASSWASAPIVERSION_REAL, BASS_WASAPI_FUNCTIONS)
#else
#define _BASS_WIN_LIBRARIES(X)
#endif

#define BASS_LIBRARIES(X)                                                            \
    X(bass, BASS_GetVersion, BASSVERSION_REAL, BASS_CORE_FUNCTIONS)                  \
    X(bass_fx, BASS_FX_GetVersion, BASSFXVERSION_REAL, BASS_FX_FUNCTIONS)            \
    X(bassmix, BASS_Mixer_GetVersion, BASSMIXVERSION_REAL, BASS_MIX_FUNCTIONS)       \
    X(bassloud, BASS_Loudness_GetVersion, BASSLOUDVERSION_REAL, BASS_LOUD_FUNCTIONS) \
    _BASS_WIN_LIBRARIES(X)

// setup the library handles and paths to check for them
#define DECLARE_LIB(name, ...)                              \
    static SDL_SharedObject *s_lib##name = nullptr;         \
    static constexpr std::initializer_list name##_paths = { \
        LNAME(name), "lib/" LNAME(name)};  // check under lib/ if it's not found in the default search path

BASS_LIBRARIES(DECLARE_LIB)

// generate function pointer definitions from the header
#define DEFINE_BASS_FUNCTION(name) name##_t name = nullptr;

ALL_BASS_FUNCTIONS(DEFINE_BASS_FUNCTION)

#define LOAD_FUNCTION(name) name = loadFunction<name##_t>(currentLib, #name);

#define GENERATE_LIBRARY_LOADER(libname, vfunc, ver, funcgroup)                                                       \
    static bool load_##libname() {                                                                                    \
        failedLoad = #libname;                                                                                        \
        for(auto &path : libname##_paths) {                                                                           \
            s_lib##libname = SDL_LoadObject(path);                                                                    \
            if(!s_lib##libname) continue;                                                                             \
            (vfunc) = loadFunction<vfunc##_t>(s_lib##libname, #vfunc);                                                \
            if(!(vfunc)) {                                                                                            \
                SDL_UnloadObject(s_lib##libname);                                                                     \
                s_lib##libname = nullptr;                                                                             \
                continue;                                                                                             \
            }                                                                                                         \
            uint64_t actualVersion = static_cast<uint64_t>(vfunc());                                                  \
            if(actualVersion >= (ver)) {                                                                              \
                SDL_SharedObject *currentLib = s_lib##libname;                                                        \
                funcgroup(LOAD_FUNCTION) return true;                                                                 \
            }                                                                                                         \
            debugLog("BassManager: version too old for {:s} (expected {:x}, got {:x})", path, ver, actualVersion); \
            SDL_UnloadObject(s_lib##libname);                                                                         \
            s_lib##libname = nullptr;                                                                                 \
            (vfunc) = nullptr;                                                                                        \
        }                                                                                                             \
        debugLog("BassManager: Failed to load " #libname " library: {:s}", SDL_GetError());                        \
        return false;                                                                                                 \
    }

static std::string failedLoad = "none";

BASS_LIBRARIES(GENERATE_LIBRARY_LOADER)

};  // namespace BassFuncs

namespace {  // static
HPLUGIN plBassFlac = 0;
bool loaded = false;

void unloadPlugins() {
    if(plBassFlac) {
        BASS_PluginEnable(plBassFlac, false);
        BASS_PluginFree(plBassFlac);
        plBassFlac = 0;
    }
}

HPLUGIN loadPlugin(const std::string &pluginname) {
#define LNAMESTR(lib) fmt::format(LPREFIX "{:s}" LSUFFIX, (lib))
    HPLUGIN ret = 0;
    // handle bassflac plugin separately
    std::string tryPath{LNAMESTR(pluginname)};

    if(!env->fileExists(tryPath)) tryPath = fmt::format("lib" PREF_PATHSEP "{}", LNAMESTR(pluginname));

    // make it a fully qualified path
    if(env->fileExists(tryPath))
        tryPath = fmt::format("{}{}", env->getFolderFromFilePath(tryPath), LNAMESTR(pluginname));
    else
        tryPath = LNAMESTR(pluginname);

    const UString pathUString{tryPath};

    ret = BASS_PluginLoad(
        (const char *)pathUString.plat_str(),
        Env::cfg(OS::WINDOWS) ? BASS_UNICODE : 0);  // ??? this wchar_t->char* cast is required for some reason?

    if(ret) {
        if(cv::debug_snd.getBool())
            debugLog("loaded {:s} version {:#x}", pluginname.c_str(), BASS_PluginGetInfo(ret)->version);
        BASS_PluginEnable(ret, true);
    } else {
        failedLoad = std::string(pluginname);
    }

    return ret;
#undef LNAMESTR
}

}  // namespace

bool isLoaded() { return loaded; }

bool init() {
    if(loaded) return true;

#define LOAD_LIBRARY(libname, ...) \
    if(!load_##libname()) {        \
        cleanup();                 \
        return false;              \
    }

    // load all the libraries here
    BASS_LIBRARIES(LOAD_LIBRARY)

    if(!loadPlugin("bassflac")) {
        cleanup();
        return false;
    }

    // if we got here, we loaded everything
    failedLoad = "none";

    return (loaded = true);
}

void cleanup() {
    // first clean up plugins
    unloadPlugins();

    // unload in reverse order
#define UNLOAD_LIB(name, ...)          \
    if(s_lib##name) {                  \
        SDL_UnloadObject(s_lib##name); \
        s_lib##name = nullptr;         \
    }

#ifdef MCENGINE_PLATFORM_WINDOWS
    UNLOAD_LIB(basswasapi)
    UNLOAD_LIB(bassasio)
#endif
    UNLOAD_LIB(bassloud)
    UNLOAD_LIB(bassmix)
    UNLOAD_LIB(bass_fx)
    UNLOAD_LIB(bass)

    // reset to null
#define RESET_FUNCTION(name) name = nullptr;
    ALL_BASS_FUNCTIONS(RESET_FUNCTION)

    loaded = false;
}

std::string getFailedLoad() { return failedLoad; }

static std::string getBassErrorStringFromCode(int code) {
    std::string errstr = fmt::format("Unknown (code {:d})", code);

    switch(code) {
        case BASS_OK:
            errstr = "No error.";
            break;
        case BASS_ERROR_MEM:
            errstr = "Memory error";
            break;
        case BASS_ERROR_FILEOPEN:
            errstr = "Can't open the file";
            break;
        case BASS_ERROR_DRIVER:
            errstr = "Can't find an available driver";
            break;
        case BASS_ERROR_BUFLOST:
            errstr = "The sample buffer was lost";
            break;
        case BASS_ERROR_HANDLE:
            errstr = "Invalid handle";
            break;
        case BASS_ERROR_FORMAT:
            errstr = "Unsupported sample format";
            break;
        case BASS_ERROR_POSITION:
            errstr = "Invalid position";
            break;
        case BASS_ERROR_INIT:
            errstr = "BASS_Init has not been successfully called";
            break;
        case BASS_ERROR_START:
            errstr = "BASS_Start has not been successfully called";
            break;
        case BASS_ERROR_SSL:
            errstr = "SSL/HTTPS support isn't available";
            break;
        case BASS_ERROR_REINIT:
            errstr = "Device needs to be reinitialized";
            break;
        case BASS_ERROR_ALREADY:
            errstr = "Already initialized";
            break;
        case BASS_ERROR_NOTAUDIO:
            errstr = "File does not contain audio";
            break;
        case BASS_ERROR_NOCHAN:
            errstr = "Can't get a free channel";
            break;
        case BASS_ERROR_ILLTYPE:
            errstr = "An illegal type was specified";
            break;
        case BASS_ERROR_ILLPARAM:
            errstr = "An illegal parameter was specified";
            break;
        case BASS_ERROR_NO3D:
            errstr = "No 3D support";
            break;
        case BASS_ERROR_NOEAX:
            errstr = "No EAX support";
            break;
        case BASS_ERROR_DEVICE:
            errstr = "Illegal device number";
            break;
        case BASS_ERROR_NOPLAY:
            errstr = "Not playing";
            break;
        case BASS_ERROR_FREQ:
            errstr = "Illegal sample rate";
            break;
        case BASS_ERROR_NOTFILE:
            errstr = "The stream is not a file stream";
            break;
        case BASS_ERROR_NOHW:
            errstr = "No hardware voices available";
            break;
        case BASS_ERROR_EMPTY:
            errstr = "The file has no sample data";
            break;
        case BASS_ERROR_NONET:
            errstr = "No internet connection could be opened";
            break;
        case BASS_ERROR_CREATE:
            errstr = "Couldn't create the file";
            break;
        case BASS_ERROR_NOFX:
            errstr = "Effects are not available";
            break;
        case BASS_ERROR_NOTAVAIL:
            errstr = "Requested data/action is not available";
            break;
        case BASS_ERROR_DECODE:
            errstr = "The channel is/isn't a decoding channel";
            break;
        case BASS_ERROR_DX:
            errstr = "A sufficient DirectX version is not installed";
            break;
        case BASS_ERROR_TIMEOUT:
            errstr = "Connection timeout";
            break;
        case BASS_ERROR_FILEFORM:
            errstr = "Unsupported file format";
            break;
        case BASS_ERROR_SPEAKER:
            errstr = "Unavailable speaker";
            break;
        case BASS_ERROR_VERSION:
            errstr = "Invalid BASS version";
            break;
        case BASS_ERROR_CODEC:
            errstr = "Codec is not available/supported";
            break;
        case BASS_ERROR_ENDED:
            errstr = "The channel/file has ended";
            break;
        case BASS_ERROR_BUSY:
            errstr = "The device is busy";
            break;
        case BASS_ERROR_UNSTREAMABLE:
            errstr = "Unstreamable file";
            break;
        case BASS_ERROR_PROTOCOL:
            errstr = "Unsupported protocol";
            break;
        case BASS_ERROR_DENIED:
            errstr = "Access Denied";
            break;
#ifdef MCENGINE_PLATFORM_WINDOWS
        case BASS_ERROR_WASAPI:
            errstr = "No WASAPI";
            break;
        case BASS_ERROR_WASAPI_BUFFER:
            errstr = "Invalid buffer size";
            break;
        case BASS_ERROR_WASAPI_CATEGORY:
            errstr = "Can't set category";
            break;
        case BASS_ERROR_WASAPI_DENIED:
            errstr = "Access denied";
            break;
#endif
        case BASS_ERROR_UNKNOWN:  // fallthrough
        default:
            break;
    }
    return errstr;
}

std::string printBassError(const std::string &context, int code) {
    std::string errstr{getBassErrorStringFromCode(code)};

    debugLog("{:s} error: {:s}", context, errstr);
    return fmt::format("{:s} error: {:s}", context, errstr);  // also return it
}

UString getErrorUString(int code) { return UString::fmt("BASS error: {:s}", getBassErrorStringFromCode(code == INT_MIN ? BASS_ErrorGetCode() : code)); }

}  // namespace BassManager

#endif
