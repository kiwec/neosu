// Copyright (c) 2025, WH, All rights reserved.
#include "config.h"

// WASM uses #embed-based generated files instead of inline assembly
#ifndef MCENGINE_PLATFORM_WASM
#include "shaders.h"

ALL_SHADER_BINARIES(INCBIN_C)
#endif
