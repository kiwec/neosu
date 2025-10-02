// Copyright (c) 2025, WH, All rights reserved.
// neosu-wide configuration (constants etc.)
#pragma once

#include "EngineConfig.h"

#ifndef NEOSU_DATA_DIR
#define NEOSU_DATA_DIR MCENGINE_DATA_DIR
#endif

/* *INDENT-OFF* */  // clang-format off

#define NEOSU_AVATARS_PATH		NEOSU_DATA_DIR "avatars"
#define NEOSU_CFG_PATH			NEOSU_DATA_DIR "cfg"
#define NEOSU_MAPS_PATH			NEOSU_DATA_DIR "maps"
#define NEOSU_REPLAYS_PATH		NEOSU_DATA_DIR "replays"
#define NEOSU_SCREENSHOTS_PATH	NEOSU_DATA_DIR "screenshots"
#define NEOSU_SKINS_PATH		NEOSU_DATA_DIR "skins"
#define NEOSU_DB_DIR			NEOSU_DATA_DIR // default is top-level, next to exe

CASSERT_STR_ENDSWITH(NEOSU_DATA_DIR, '/');
CASSERT_STR_ENDSWITH(NEOSU_DB_DIR, '/');

/* *INDENT-ON* */  // clang-format on
