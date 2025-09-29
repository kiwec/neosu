#pragma once
// Copyright (c) 2018, PG, All rights reserved.
#include "CBaseUIButton.h"

class Image;

class UIPauseMenuButton final : public CBaseUIButton {
   public:
    UIPauseMenuButton(std::function<Image *()> getImageFunc, float xPos, float yPos, float xSize, float ySize,
                      UString name);

    void draw() override;

    void onMouseInside() override;
    void onMouseOutside() override;

    void setBaseScale(float xScale, float yScale);
    void setAlpha(float alpha) { this->fAlpha = alpha; }

    Image* getImage() { return this->getImageFunc != nullptr ? this->getImageFunc() : nullptr; }

   private:
    vec2 vScale{0.f};
    vec2 vBaseScale{0.f};
    float fScaleMultiplier;

    float fAlpha;

    std::function<Image *()> getImageFunc;
};
