// Copyright (c) 2016, PG, All rights reserved.
#include "UIBackButton.h"

#include <utility>

#include "AnimationHandler.h"
#include "ConVar.h"
#include "Engine.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SoundEngine.h"

UIBackButton::UIBackButton(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), "") {
    this->fAnimation = 0.0f;
}

void UIBackButton::draw() {
    if(!this->bVisible) return;

    // draw button image
    g->pushTransform();
    {
        g->setColor(0xffffffff);

        const SkinImage *backimg =
            this->bUseDefaultBack ? osu->getSkin()->i_menu_back2_DEFAULTSKIN : osu->getSkin()->i_menu_back2;
        backimg->draw(this->vPos + (backimg->getSize() / 2.f));
    }
    g->popTransform();

    // draw anim highlight overlay
    if(this->fAnimation > 0.0f) {
        g->pushTransform();
        {
            g->setColor(Color(0xffffffff).setA(this->fAnimation * 0.15f));

            g->translate(this->vPos.x + this->vSize.x / 2, this->vPos.y + this->vSize.y / 2);
            g->fillRect(-this->vSize.x / 2, -this->vSize.y / 2, this->vSize.x, this->vSize.y + 5);
        }
        g->popTransform();
    }
}

void UIBackButton::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUIButton::mouse_update(propagate_clicks);
}

void UIBackButton::onMouseDownInside(bool left, bool right) {
    CBaseUIButton::onMouseDownInside(left, right);

    soundEngine->play(osu->getSkin()->s_click_back_button);
}

void UIBackButton::onMouseInside() {
    CBaseUIButton::onMouseInside();

    anim->moveQuadOut(&this->fAnimation, 1.0f, 0.1f, 0.0f, true);
    soundEngine->play(osu->getSkin()->s_hover_back_button);
}

void UIBackButton::onMouseOutside() {
    CBaseUIButton::onMouseOutside();

    anim->moveQuadOut(&this->fAnimation, 0.0f, this->fAnimation * 0.1f, 0.0f, true);
}

void UIBackButton::updateLayout() {
    const SkinImage *backimg = osu->getSkin()->i_menu_back2;
    OptionsMenu *optmenu = osu->getOptionsMenu();

    if(optmenu && optmenu->isVisible() && backimg->getSize().y > (optmenu->getSize().y / 4) &&
       (osu->getSkin()->i_menu_back2_DEFAULTSKIN && osu->getSkin()->i_menu_back2_DEFAULTSKIN->isReady())) {
        // always show default back button when options menu is showing, if its height is > 1/4 the options menu height
        backimg = osu->getSkin()->i_menu_back2_DEFAULTSKIN;
        this->bUseDefaultBack = true;
    } else {
        this->bUseDefaultBack = false;
    }

    this->setSize(backimg->getSize());
}

void UIBackButton::resetAnimation() {
    anim->deleteExistingAnimation(&this->fAnimation);
    this->fAnimation = 0.0f;
}
