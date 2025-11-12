#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "OsuScreen.h"
#include "UIBackButton.h"

class ScreenBackable : public OsuScreen {
   public:
    ScreenBackable();
    ~ScreenBackable() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onResolutionChange(vec2 newResolution) override;
    virtual void onBack() = 0;
    virtual void updateLayout();

    UIBackButton *backButton;
};
