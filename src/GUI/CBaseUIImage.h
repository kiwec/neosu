#pragma once
#include "CBaseUIElement.h"

class CBaseUIImage : public CBaseUIElement {
   public:
    CBaseUIImage(std::string imageResourceName = "", float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0,
                 UString name = "");
    ~CBaseUIImage() override { ; }

    void draw(Graphics *g) override;

    void setImage(Image *img);

    CBaseUIImage *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUIImage *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }

    CBaseUIImage *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUIImage *setColor(Color color) {
        this->color = color;
        return this;
    }
    CBaseUIImage *setAlpha(float alpha) {
        this->color &= 0x00ffffff;
        this->color |= ((int)(255.0f * alpha)) << 24;
        return this;
    }
    CBaseUIImage *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }

    CBaseUIImage *setRotationDeg(float rotation) {
        this->fRot = rotation;
        return this;
    }
    CBaseUIImage *setScale(float xScale, float yScale) {
        this->vScale.x = xScale;
        this->vScale.y = yScale;
        return this;
    }
    CBaseUIImage *setScale(Vector2 scale) {
        this->vScale.x = scale.x;
        this->vScale.y = scale.y;
        return this;
    }
    CBaseUIImage *setScaleToFit(bool scaleToFit) {
        this->bScaleToFit = scaleToFit;
        return this;
    }

    inline float getRotationDeg() const { return this->fRot; }
    inline Vector2 getScale() const { return this->vScale; }
    inline Image *getImage() const { return this->image; }

   private:
    Image *image;

    Color frameColor;
    Color backgroundColor;
    Color color;

    bool bDrawFrame;
    bool bDrawBackground;
    bool bScaleToFit;

    float fRot;
    Vector2 vScale;
};
