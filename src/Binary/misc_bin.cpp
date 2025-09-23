// Copyright (c) 2025, WH, All rights reserved.
#include "misc_bin.h"

#ifndef MISC_INCDIR
#define MISC_INCDIR
#endif

#define INCBIN_C_MISC(var, filename) INCBIN_C(var, MISC_INCDIR filename)

// in assets/misc
INCBIN_C_MISC(convar_template, "convar_template.html")
