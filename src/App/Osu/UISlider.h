#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUISlider.h"

class UISlider final : public CBaseUISlider {
   public:
    UISlider(float xPos, float yPos, float xSize, float ySize, UString name);

    void draw() override;
};
