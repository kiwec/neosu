// Copyright (c) 2016, PG, All rights reserved.
#include "UIButton.h"

#include <utility>

#include "AnimationHandler.h"
#include "Engine.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"

static float button_sound_cooldown = 0.f;

void UIButton::draw() {
    if(!this->bVisible || !this->bVisible2) return;

    Image *buttonLeft = this->bDefaultSkin ? osu->getSkin()->i_button_left_default : osu->getSkin()->i_button_left;
    Image *buttonMiddle = this->bDefaultSkin ? osu->getSkin()->i_button_mid_default : osu->getSkin()->i_button_mid;
    Image *buttonRight = this->bDefaultSkin ? osu->getSkin()->i_button_right_default : osu->getSkin()->i_button_right;

    float leftScale = osu->getImageScaleToFitResolution(buttonLeft, this->vSize);
    float leftWidth = buttonLeft->getWidth() * leftScale;

    float rightScale = osu->getImageScaleToFitResolution(buttonRight, this->vSize);
    float rightWidth = buttonRight->getWidth() * rightScale;

    float middleWidth = this->vSize.x - leftWidth - rightWidth;

    {
        auto color = this->is_loading ? rgba(0x33, 0x33, 0x33, 0xff) : this->color;

        float brightness = 1.f + (this->fHoverAnim * 0.2f);
        color = Colors::brighten(color, brightness);

        g->setColor(color);
    }

    buttonLeft->bind();
    {
        g->drawQuad((int)this->vPos.x, (int)this->vPos.y, (int)leftWidth, (int)this->vSize.y);
    }
    buttonLeft->unbind();

    buttonMiddle->bind();
    {
        g->drawQuad((int)this->vPos.x + (int)leftWidth, (int)this->vPos.y, (int)middleWidth, (int)this->vSize.y);
    }
    buttonMiddle->unbind();

    buttonRight->bind();
    {
        g->drawQuad((int)this->vPos.x + (int)leftWidth + (int)middleWidth, (int)this->vPos.y, (int)rightWidth,
                    (int)this->vSize.y);
    }
    buttonRight->unbind();

    if(this->is_loading) {
        const float scale = (this->vSize.y * 0.8) / osu->getSkin()->i_loading_spinner->getSize().y;
        g->setColor(0xffffffff);
        g->pushTransform();
        g->rotate(engine->getTime() * 180, 0, 0, 1);
        g->scale(scale, scale);
        g->translate(this->vPos.x + this->vSize.x / 2.0f, this->vPos.y + this->vSize.y / 2.0f);
        g->drawImage(osu->getSkin()->i_loading_spinner);
        g->popTransform();
    } else {
        this->drawText();
    }
}

void UIButton::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible || !this->bVisible2) return;
    CBaseUIButton::mouse_update(propagate_clicks);

    if(this->isMouseInside() && this->tooltipTextLines.size() > 0 && !this->bFocusStolenDelay) {
        osu->getTooltipOverlay()->begin();
        {
            for(const auto &tooltipTextLine : this->tooltipTextLines) {
                osu->getTooltipOverlay()->addLine(tooltipTextLine);
            }
        }
        osu->getTooltipOverlay()->end();
    }

    this->bFocusStolenDelay = false;
}

void UIButton::onMouseInside() {
    // There's actually no animation, it just goes to 1 instantly
    this->fHoverAnim = 1.f;

    if(button_sound_cooldown + 0.05f < engine->getTime()) {
        soundEngine->play(osu->getSkin()->s_hover_button);
        button_sound_cooldown = engine->getTime();
    }
}

void UIButton::onMouseOutside() { this->fHoverAnim = 0.f; }

void UIButton::onClicked(bool left, bool right) {
    CBaseUIButton::onClicked(left, right);

    if(this->is_loading) return;

    this->animateClickColor();

    soundEngine->play(osu->getSkin()->s_click_button);
}

void UIButton::onFocusStolen() {
    CBaseUIButton::onFocusStolen();

    this->bMouseInside = false;
    this->bFocusStolenDelay = true;
}

void UIButton::animateClickColor() {
    this->fClickAnim = 1.0f;
    anim->moveLinear(&this->fClickAnim, 0.0f, 0.5f, true);
}

void UIButton::setTooltipText(const UString &text) { this->tooltipTextLines = text.split("\n"); }
