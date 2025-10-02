// Copyright (c) 2025, WH, All rights reserved.
#include "dynutils.h"
#include "Environment.h"
#include "Logging.h"

#include <SDL3/SDL_loadso.h>

#ifdef MCENGINE_PLATFORM_WINDOWS
#define LPREFIX ""
#define LSUFFIX ".dll"
#else
#define LPREFIX "lib"
#define LSUFFIX ".so"
#endif

#define LNAMESTR(lib) fmt::format(LPREFIX "{:s}" LSUFFIX, (lib))

namespace dynutils {
namespace detail {

void *load_func_impl(lib_obj *lib, const char *func_name) {
    void *retfunc = nullptr;
    if(lib) {
        retfunc = reinterpret_cast<void *>(SDL_LoadFunction(reinterpret_cast<SDL_SharedObject *>(lib), func_name));
    }
    if(!retfunc) {
        debugLog("Failed to load function {:s} from lib ({:p}): {:s}", func_name, static_cast<const void *>(lib),
                 get_error());
    }
    return retfunc;
}

}  // namespace detail

void unload_lib(lib_obj *&lib) {
    if(lib) {
        SDL_UnloadObject(reinterpret_cast<SDL_SharedObject *>(lib));
    }
    lib = nullptr;
}

// example usage: load_lib("bass"), load_lib("libbass.so"), load_lib("bass.dll"), load_lib("bass", "lib/")
// you get the point
lib_obj *load_lib(const char *c_lib_name, const char *c_search_dir) {
    std::string lib_name{c_lib_name};
    std::string search_dir{c_search_dir};
    lib_obj *ret = nullptr;
    if(!lib_name.empty()) {
        if(Environment::getFileExtensionFromFilePath(lib_name).empty()) {
            lib_name = LNAMESTR(lib_name);
        }
        if(!search_dir.empty()) {
            if(!search_dir.ends_with('/') && search_dir.ends_with('\\')) {
                search_dir.push_back('/');
            }
            lib_name = search_dir + lib_name;
        }
        ret = reinterpret_cast<lib_obj *>(SDL_LoadObject(lib_name.c_str()));
    }
    if(!ret) {
        if(!lib_name.empty() && !lib_name.contains('/')) {
            // try to fall back to relative local paths first before giving up entirely
            for(const auto &path : std::array{"./lib", "."}) {
                std::string temp_relative = fmt::format("{}/{}", path, lib_name);
                if((ret = reinterpret_cast<lib_obj *>(SDL_LoadObject(temp_relative.c_str())))) {
                    // found
                    break;
                }
            }
        }
        if(!ret) debugLog("Failed to load library {:s}: {:s}", lib_name, get_error());
    }
    return ret;
}

const char *get_error() {
    const char *err = SDL_GetError();
    return err ? err : "<no error>";
}

#undef LNAME
#undef LNAMESTR
#undef LPREFIX
#undef LSUFFIX

}  // namespace dynutils
