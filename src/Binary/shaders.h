// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "config.h"
#include "incbin.h"

#ifndef SHADERS_INCDIR
#define SHADERS_INCDIR
#endif

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)

/* shader files located in assets/shaders */
#define GLSL_SHADER_NAMES(VorF, X)                                                           \
    X(GL_cursortrail_##VorF##sh, SHADERS_INCDIR "GL_cursortrail_" #VorF ".glsl")             \
    X(GL_slider_##VorF##sh, SHADERS_INCDIR "GL_slider_" #VorF ".glsl")                       \
    X(GL_flashlight_##VorF##sh, SHADERS_INCDIR "GL_flashlight_" #VorF ".glsl")               \
    X(GL_actual_flashlight_##VorF##sh, SHADERS_INCDIR "GL_actual_flashlight_" #VorF ".glsl") \
    X(GL_smoothclip_##VorF##sh, SHADERS_INCDIR "GL_smoothclip_" #VorF ".glsl")

#else

#define GLSL_SHADER_NAMES(...)

#endif

#if defined(MCENGINE_FEATURE_DIRECTX11)

#define HLSL_SHADER_NAMES(VorF, X)                                                               \
    X(DX11_cursortrail_##VorF##sh, SHADERS_INCDIR "DX11_cursortrail_" #VorF ".hlsl")             \
    X(DX11_slider_##VorF##sh, SHADERS_INCDIR "DX11_slider_" #VorF ".hlsl")                       \
    X(DX11_flashlight_##VorF##sh, SHADERS_INCDIR "DX11_flashlight_" #VorF ".hlsl")               \
    X(DX11_actual_flashlight_##VorF##sh, SHADERS_INCDIR "DX11_actual_flashlight_" #VorF ".hlsl") \
    X(DX11_smoothclip_##VorF##sh, SHADERS_INCDIR "DX11_smoothclip_" #VorF ".hlsl")               \
    X(DX11_default_##VorF##sh, SHADERS_INCDIR "DX11_default_" #VorF ".hlsl")

#else

#define HLSL_SHADER_NAMES(...)

#endif

#define ALL_SHADER_BINARIES(X) \
    GLSL_SHADER_NAMES(v, X)    \
    GLSL_SHADER_NAMES(f, X)    \
    HLSL_SHADER_NAMES(v, X)    \
    HLSL_SHADER_NAMES(f, X)

ALL_SHADER_BINARIES(INCBIN_H)
