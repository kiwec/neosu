#pragma once
#include "CBaseUISlider.h"

class UISlider : public CBaseUISlider {
   public:
    UISlider(float xPos, float yPos, float xSize, float ySize, UString name);

    void draw() override;
};
