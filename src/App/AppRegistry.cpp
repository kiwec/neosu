// Copyright (c) 2026, WH, All rights reserved.
#include "AppDescriptor.h"

#include "Osu.h"
#include "BaseFrameworkTest.h"
#include "NeosuEnvInterop.h"

#include <array>

namespace Mc {

static constexpr std::array sDescriptors{
    AppDescriptor{"neosu", [] -> App * { return new Osu(); }, neosu::createInterop, neosu::handleExistingWindow},
    AppDescriptor{"BaseFrameworkTest", [] -> App * { return new Mc::Tests::BaseFrameworkTest(); }},
};

std::span<const AppDescriptor> getAllAppDescriptors() { return sDescriptors; }
const AppDescriptor &getDefaultAppDescriptor() { return sDescriptors[0]; }

}  // namespace Mc
