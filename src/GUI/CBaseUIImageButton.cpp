// Copyright (c) 2015, PG, All rights reserved.
#include "CBaseUIImageButton.h"

#include <utility>

#include "Engine.h"
#include "ResourceManager.h"
#include "Image.h"
#include "Graphics.h"

CBaseUIImageButton::CBaseUIImageButton(std::string imageResourceName, float xPos, float yPos, float xSize, float ySize,
                                       UString name)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), "") {
    this->setImageResourceName(std::move(imageResourceName));

    this->fRot = 0.0f;
    this->vScale = vec2(1, 1);
    this->bScaleToFit = false;
    this->bKeepAspectRatio = true;
}

void CBaseUIImageButton::draw() {
    if(!this->bVisible) return;

    // draw image
    Image *image = resourceManager->getImage(this->sImageResourceName);
    if(image != nullptr) {
        g->setColor(0xffffffff);
        g->pushTransform();

        // scale
        g->scale(this->vScale.x, this->vScale.y);

        // rotate
        if(this->fRot != 0.0f) g->rotate(this->fRot);

        // center and draw
        g->translate(this->getPos().x + (int)(this->getSize().x / 2), this->getPos().y + (int)(this->getSize().y / 2));
        g->drawImage(image);

        g->popTransform();
    }
}

CBaseUIImageButton *CBaseUIImageButton::setImageResourceName(std::string imageResourceName) {
    this->sImageResourceName = std::move(imageResourceName);

    Image *image = resourceManager->getImage(this->sImageResourceName);
    if(image != nullptr) this->setSize(vec2(image->getWidth(), image->getHeight()));

    return this;
}

void CBaseUIImageButton::onResized() {
    CBaseUIButton::onResized();

    Image *image = resourceManager->getImage(this->sImageResourceName);
    if(this->bScaleToFit && image != nullptr) {
        if(!this->bKeepAspectRatio) {
            this->vScale = vec2(this->getSize().x / image->getWidth(), this->getSize().y / image->getHeight());
            this->rect.setSize(
                vec2{(int)(image->getWidth() * this->vScale.x), (int)(image->getHeight() * this->vScale.y)});
        } else {
            float scaleFactor = this->getSize().x / image->getWidth() < this->getSize().y / image->getHeight()
                                    ? this->getSize().x / image->getWidth()
                                    : this->getSize().y / image->getHeight();
            this->vScale = vec2(scaleFactor, scaleFactor);
            this->rect.setSize(
                vec2{(int)(image->getWidth() * this->vScale.x), (int)(image->getHeight() * this->vScale.y)});
        }
    }
}
