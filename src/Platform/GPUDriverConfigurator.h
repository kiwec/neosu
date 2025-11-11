// Copyright (c) 2025, WH, All rights reserved.
#pragma once

#include "noinclude.h"

#include <string_view>

// configures GPU driver settings through vendor-specific APIs (currently NVAPI for NVIDIA on Windows)
class GPUDriverConfigurator {
    NOCOPY_NOMOVE(GPUDriverConfigurator);

   public:
    GPUDriverConfigurator() noexcept;
    ~GPUDriverConfigurator() noexcept;

    // if non-empty, contains initialization information (errors)
    // to defer showing errors until we know whether it's relevant to (e.g. what GPU is in use)
    [[nodiscard]] std::string_view getInitInfo() const noexcept;

   private:
    // convar callback
    void onDisableDrvThrdOptsChange(float newVal);
    bool currently_disabled;
};
