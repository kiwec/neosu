#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUIContainer.h"

class KeyboardEvent;

class UIOverlay : public CBaseUIContainer {
   public:
    UIOverlay() { this->bVisible = false; }

    virtual void onResolutionChange(vec2 newResolution) { (void)newResolution; }
};
