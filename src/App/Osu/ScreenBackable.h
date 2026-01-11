#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "UIOverlay.h"

#include <memory>

class UIBackButton;

class ScreenBackable : public UIOverlay {
    NOCOPY_NOMOVE(ScreenBackable)
   public:
    ScreenBackable();
    ~ScreenBackable() override;

    void draw() override;
    void update() override;
    void onKeyDown(KeyboardEvent &e) override;
    void onResolutionChange(vec2 newResolution) override;
    virtual void onBack() = 0;
    virtual void updateLayout();

    std::unique_ptr<UIBackButton> backButton;
};
