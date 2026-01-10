// Copyright (c) 2016, PG, All rights reserved.
#include "ScreenBackable.h"

#include "Engine.h"
#include "OsuConVars.h"
#include "KeyBindings.h"
#include "Osu.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "UIBackButton.h"
#include "MakeDelegateWrapper.h"

ScreenBackable::ScreenBackable() : OsuScreen() {
    this->backButton = new UIBackButton(-1, 0, 0, 0, "");
    this->backButton->setClickCallback(SA::MakeDelegate<&ScreenBackable::onBack>(this));

    this->updateLayout();
}

ScreenBackable::~ScreenBackable() { SAFE_DELETE(this->backButton); }

void ScreenBackable::draw() {
    if(!this->bVisible) return;
    OsuScreen::draw();
    this->backButton->draw();
}

void ScreenBackable::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    this->backButton->mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;
    OsuScreen::mouse_update(propagate_clicks);
}

void ScreenBackable::onKeyDown(KeyboardEvent &e) {
    OsuScreen::onKeyDown(e);
    if(!this->bVisible || e.isConsumed()) return;

    if(e == KEY_ESCAPE || e == cv::GAME_PAUSE.getVal<SCANCODE>()) {
        soundEngine->play(osu->getSkin()->s_menu_back);
        this->onBack();
        e.consume();
        return;
    }
}

void ScreenBackable::updateLayout() {
    this->backButton->updateLayout();
    this->backButton->setPosY(osu->getVirtScreenHeight() - this->backButton->getSize().y);
}

void ScreenBackable::onResolutionChange(vec2 /*newResolution*/) { this->updateLayout(); }
