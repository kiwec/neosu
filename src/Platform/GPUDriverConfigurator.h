// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "noinclude.h"

class SDLMain;

// configures GPU driver settings through vendor-specific APIs (currently NVAPI for NVIDIA on Windows)
class GPUDriverConfigurator {
    NOCOPY_NOMOVE(GPUDriverConfigurator);

   public:
    GPUDriverConfigurator(SDLMain *mainp) noexcept;
    ~GPUDriverConfigurator() noexcept;

   private:
    // convar callback
    void onDisableDrvThrdOptsChange(float newVal);

    SDLMain *mainptr;

    bool currently_disabled;
};
