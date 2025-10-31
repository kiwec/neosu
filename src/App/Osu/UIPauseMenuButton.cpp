// Copyright (c) 2018, PG, All rights reserved.
#include "UIPauseMenuButton.h"

#include <utility>

#include "AnimationHandler.h"
#include "Engine.h"
#include "Osu.h"
#include "Skin.h"
#include "SoundEngine.h"

UIPauseMenuButton::UIPauseMenuButton(std::function<Image *()> getImageFunc, float xPos, float yPos, float xSize,
                                     float ySize, UString name)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name)) {
    this->getImageFunc = std::move(getImageFunc);

    this->vScale = vec2(1, 1);
    this->fScaleMultiplier = 1.1f;

    this->fAlpha = 1.0f;
}

void UIPauseMenuButton::draw() {
    if(!this->bVisible) return;

    // draw image
    Image *image = this->getImageFunc();
    if(image != nullptr) {
        g->setColor(argb(this->fAlpha, 1.0f, 1.0f, 1.0f));
        g->pushTransform();
        {
            // scale
            g->scale(this->vScale.x, this->vScale.y);

            // center and draw
            g->translate(this->vPos.x + (int)(this->vSize.x / 2), this->vPos.y + (int)(this->vSize.y / 2));
            g->drawImage(image);
        }
        g->popTransform();
    }
}

void UIPauseMenuButton::setBaseScale(float xScale, float yScale) {
    this->vBaseScale.x = xScale;
    this->vBaseScale.y = yScale;

    this->vScale = this->vBaseScale;
}

void UIPauseMenuButton::onMouseInside() {
    CBaseUIButton::onMouseInside();

    if(env->winFocused()) soundEngine->play(osu->getSkin()->s_menu_hover);

    const float animationDuration = 0.09f;
    anim->moveLinear(&this->vScale.x, this->vBaseScale.x * this->fScaleMultiplier, animationDuration, true);
    anim->moveLinear(&this->vScale.y, this->vBaseScale.y * this->fScaleMultiplier, animationDuration, true);

    if(this->getName() == UString("Resume")) {
        soundEngine->play(osu->getSkin()->s_hover_pause_continue);
    } else if(this->getName() == UString("Retry")) {
        soundEngine->play(osu->getSkin()->s_hover_pause_retry);
    } else if(this->getName() == UString("Quit")) {
        soundEngine->play(osu->getSkin()->s_hover_pause_back);
    }
}

void UIPauseMenuButton::onMouseOutside() {
    CBaseUIButton::onMouseOutside();

    const float animationDuration = 0.09f;
    anim->moveLinear(&this->vScale.x, this->vBaseScale.x, animationDuration, true);
    anim->moveLinear(&this->vScale.y, this->vBaseScale.y, animationDuration, true);
}
