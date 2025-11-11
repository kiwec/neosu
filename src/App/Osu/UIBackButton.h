#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUIButton.h"

class UIBackButton final : public CBaseUIButton {
   public:
    UIBackButton(float xPos, float yPos, float xSize, float ySize, UString name)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), "") {}

    void draw() override;
    void mouse_update(bool* propagate_clicks) override;

    void onMouseDownInside(bool left = true, bool right = false) override;
    void onMouseInside() override;
    void onMouseOutside() override;

    void resetAnimation();

   private:
    float fAnimation{0.f};
};
