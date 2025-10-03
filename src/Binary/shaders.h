// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "config.h"
#include "incbin.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

#ifndef SHADERS_INCDIR
#define SHADERS_INCDIR
#endif

/* shader files located in assets/shaders */
#define SHADER_FILE_EXT ".glsl"
#define DEFAULT_SHADER(...)

#elif defined(MCENGINE_FEATURE_DIRECTX11)

#define SHADER_FILE_EXT ".hlsl"
#define DEFAULT_SHADER(VorF, X) X(default_##VorF##sh, SHADERS_INCDIR "default_" #VorF SHADER_FILE_EXT)

#endif

#define BASE_SHADER_NAMES(VorF, X)                                                             \
    X(cursortrail_##VorF##sh, SHADERS_INCDIR "cursortrail_" #VorF SHADER_FILE_EXT)             \
    X(slider_##VorF##sh, SHADERS_INCDIR "slider_" #VorF SHADER_FILE_EXT)                       \
    X(flashlight_##VorF##sh, SHADERS_INCDIR "flashlight_" #VorF SHADER_FILE_EXT)               \
    X(actual_flashlight_##VorF##sh, SHADERS_INCDIR "actual_flashlight_" #VorF SHADER_FILE_EXT) \
    X(smoothclip_##VorF##sh, SHADERS_INCDIR "smoothclip_" #VorF SHADER_FILE_EXT)               \
    DEFAULT_SHADER(VorF, X)

#define ALL_SHADER_BINARIES(X) \
    BASE_SHADER_NAMES(v, X)    \
    BASE_SHADER_NAMES(f, X)

ALL_SHADER_BINARIES(INCBIN_H)
