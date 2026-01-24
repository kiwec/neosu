// Copyright (c) 2014, PG, All rights reserved.
#include "CBaseUIImage.h"

#include <utility>

#include "Engine.h"
#include "ResourceManager.h"
#include "Image.h"
#include "Graphics.h"

CBaseUIImage::CBaseUIImage(const std::string& imageResourceName, float xPos, float yPos, float xSize, float ySize,
                           UString name)
    : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
    this->bScaleToFit = true;  // must be up here because it's used in setImage()

    if(!imageResourceName.empty()) {
        this->setImage(resourceManager->getImage(imageResourceName));
    } else {
        this->image = nullptr;
    }

    this->fRot = 0.0f;
    this->vScale.x = 1.0f;
    this->vScale.y = 1.0f;

    // if our image is null, autosize to the element size
    if(this->image == nullptr) {
        this->rect.setSize(vec2{xSize, ySize});
    }

    this->frameColor = argb(255, 255, 255, 255);
    this->backgroundColor = argb(255, 0, 0, 0);
    this->color = 0xffffffff;

    this->bDrawFrame = false;
    this->bDrawBackground = false;
}

void CBaseUIImage::draw() {
    if(!this->bVisible) return;

    // draw background
    if(this->bDrawBackground) {
        g->setColor(this->backgroundColor);
        g->fillRect(this->getPos().x + 1, this->getPos().y + 1, this->getSize().x - 1, this->getSize().y - 1);
    }

    // draw image
    if(this->image != nullptr) {
        g->setColor(this->color);
        g->pushTransform();

        // scale
        if(this->bScaleToFit) g->scale(this->vScale.x, this->vScale.y);

        // rotate
        if(this->fRot != 0.0f) g->rotate(this->fRot);

        // center and draw
        g->translate(this->getPos().x + (this->getSize().x / 2) + (!this->bScaleToFit ? 1 : 0),
                     this->getPos().y + (this->getSize().y / 2) + (!this->bScaleToFit ? 1 : 0));
        g->drawImage(this->image);

        g->popTransform();
    }

    // draw frame
    if(this->bDrawFrame) {
        g->setColor(this->frameColor);
        g->drawRect(this->getPos(), this->getSize());
    }
}

void CBaseUIImage::setImage(const Image* img) {
    this->image = img;

    if(this->image != nullptr && this->image->isReady()) {
        if(this->bScaleToFit) {
            this->rect.setSize(vec2{this->image->getWidth(), this->image->getHeight()});
        }

        this->vScale.x = this->getSize().x / (float)this->image->getWidth();
        this->vScale.y = this->getSize().y / (float)this->image->getHeight();
    } else {
        this->vScale.x = 1;
        this->vScale.y = 1;
    }
}
