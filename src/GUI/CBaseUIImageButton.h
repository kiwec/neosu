#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "CBaseUIButton.h"

class CBaseUIImageButton : public CBaseUIButton {
   public:
    CBaseUIImageButton(std::string imageResourceName = "", float xPos = 0, float yPos = 0, float xSize = 0,
                       float ySize = 0, UString name = {});
    ~CBaseUIImageButton() override { ; }

    void draw() override;

    void onResized() override;

    CBaseUIImageButton *setImageResourceName(std::string imageResourceName);
    CBaseUIImageButton *setRotationDeg(float deg) {
        this->fRot = deg;
        return this;
    }
    CBaseUIImageButton *setScale(float xScale, float yScale) {
        this->vScale.x = xScale;
        this->vScale.y = yScale;
        return this;
    }
    CBaseUIImageButton *setScaleToFit(bool scaleToFit) {
        this->bScaleToFit = scaleToFit;
        return this;
    }
    CBaseUIImageButton *setKeepAspectRatio(bool keepAspectRatio) {
        this->bKeepAspectRatio = keepAspectRatio;
        return this;
    }

    [[nodiscard]] inline std::string getImageResourceName() const { return this->sImageResourceName; }
    [[nodiscard]] inline vec2 getScale() const { return this->vScale; }

   protected:
    std::string sImageResourceName;
    vec2 vScale{0.f};

    float fRot;

    bool bScaleToFit;
    bool bKeepAspectRatio;
};
