// Copyright (c) 2025, WH, All rights reserved.
// global (engine-wide) configuration (constants etc.)
#pragma once

#include "config.h"

#ifndef MCENGINE_DATA_DIR

#ifndef MCENGINE_DATA_ROOT
#define MCENGINE_DATA_ROOT "."
#endif

#define MCENGINE_DATA_DIR MCENGINE_DATA_ROOT "/"

#endif

/* *INDENT-OFF* */  // clang-format off

#define MCENGINE_IMAGES_PATH	MCENGINE_DATA_DIR "materials"
#define MCENGINE_FONTS_PATH		MCENGINE_DATA_DIR "fonts"
#define MCENGINE_SOUNDS_PATH	MCENGINE_DATA_DIR "sounds"
#define MCENGINE_SHADERS_PATH	MCENGINE_DATA_DIR "shaders"
#define MCENGINE_CFG_PATH		MCENGINE_DATA_DIR "cfg"

/* *INDENT-ON* */  // clang-format on
