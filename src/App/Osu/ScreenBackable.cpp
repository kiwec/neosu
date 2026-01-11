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

ScreenBackable::ScreenBackable() : UIOverlay(), backButton(std::make_unique<UIBackButton>(-1.f, 0.f, 0.f, 0.f, "")) {
    this->backButton->setClickCallback(SA::MakeDelegate<&ScreenBackable::onBack>(this));

    this->updateLayout();
}

ScreenBackable::~ScreenBackable() = default;

void ScreenBackable::draw() {
    if(!this->bVisible) return;
    UIOverlay::draw();
    this->backButton->draw();
}

void ScreenBackable::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    this->backButton->mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;
    UIOverlay::mouse_update(propagate_clicks);
}

void ScreenBackable::onKeyDown(KeyboardEvent &e) {
    UIOverlay::onKeyDown(e);
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
