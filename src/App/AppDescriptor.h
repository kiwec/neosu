// Copyright (c) 2026, WH, All rights reserved.
#pragma once

#ifndef APPDESCRIPTOR_H
#define APPDESCRIPTOR_H

#include <span>

class App;
namespace Mc {
struct AppDescriptor {
    const char *name{nullptr};
    App *(*create)(){nullptr};
    // null = use base Environment::Interop (no-op)
    void *(*createInterop)(void *env){nullptr};
    // null = skip existing-window check
    void (*handleExistingWindow)(int argc, char *argv[]){nullptr};
};

// implemented in AppRegistry.cpp
std::span<const AppDescriptor> getAllAppDescriptors();
const AppDescriptor &getDefaultAppDescriptor();
}  // namespace Mc

#endif
