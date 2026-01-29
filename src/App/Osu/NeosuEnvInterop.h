// Copyright (c) 2026, WH, All rights reserved.
#pragma once

#ifndef NEOSUENVINTEROP_H
#define NEOSUENVINTEROP_H

// TODO: maybe these should be static members of Osu:: ?
namespace neosu {
void *createInterop(void *envptr);
void handleExistingWindow(int argc, char *argv[]);
}  // namespace neosu

#endif
