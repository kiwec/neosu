// Copyright (c) 2016, PG, All rights reserved.
#include "UIModSelectorModButton.h"

#include <utility>

#include "ConVar.h"
#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "Engine.h"
#include "ModSelector.h"
#include "Osu.h"
#include "RichPresence.h"
#include "RoomScreen.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)
#include "OpenGLHeaders.h"
#endif

UIModSelectorModButton::UIModSelectorModButton(ModSelector *osuModSelector, float xPos, float yPos, float xSize,
                                               float ySize, UString name)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), "") {
    this->osuModSelector = osuModSelector;
    this->iState = 0;
    this->vScale = this->vBaseScale = vec2(1, 1);
    this->fRot = 0.0f;

    this->fEnabledScaleMultiplier = 1.25f;
    this->fEnabledRotationDeg = 6.0f;
    this->bAvailable = true;
    this->bOn = false;

    this->getActiveImageFunc = nullptr;

    this->bFocusStolenDelay = false;
}

void UIModSelectorModButton::draw() {
    if(!this->bVisible) return;

    if(this->getActiveImageFunc != nullptr && this->getActiveImageFunc()) {
        g->pushTransform();
        {
            g->scale(this->vScale.x, this->vScale.y);

            if(this->fRot != 0.0f) g->rotate(this->fRot);

            g->setColor(0xffffffff);

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)
            // HACK: For "Actual Flashlight" mod, I'm too lazy to add a new skin element
            bool draw_inverted_colors = this->getActiveModName() == UString("afl");

            if(draw_inverted_colors) {
                glEnable(GL_COLOR_LOGIC_OP);
                glLogicOp(GL_COPY_INVERTED);
            }
#endif

            this->getActiveImageFunc()->draw(this->vPos + this->vSize / 2.f);

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_GLES32)
            if(draw_inverted_colors) {
                glDisable(GL_COLOR_LOGIC_OP);
            }
#else
            MC_MESSAGE("inverted colors are not implemented for this renderer!");
#endif
        }
        g->popTransform();
    }

    if(!this->bAvailable) {
        const int size = this->vSize.x > this->vSize.y ? this->vSize.x : this->vSize.y;

        g->setColor(0xff000000);
        g->drawLine(this->vPos.x + 1, this->vPos.y, this->vPos.x + size + 1, this->vPos.y + size);
        g->setColor(0xffffffff);
        g->drawLine(this->vPos.x, this->vPos.y, this->vPos.x + size, this->vPos.y + size);
        g->setColor(0xff000000);
        g->drawLine(this->vPos.x + size + 1, this->vPos.y, this->vPos.x + 1, this->vPos.y + size);
        g->setColor(0xffffffff);
        g->drawLine(this->vPos.x + size, this->vPos.y, this->vPos.x, this->vPos.y + size);
    }
}

void UIModSelectorModButton::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUIButton::mouse_update(propagate_clicks);

    // handle tooltips
    if(this->isMouseInside() && this->bAvailable && this->states.size() > 0 && !this->bFocusStolenDelay) {
        osu->getTooltipOverlay()->begin();
        for(const auto &tooltipTextLine : this->states[this->iState].tooltipTextLines) {
            osu->getTooltipOverlay()->addLine(tooltipTextLine);
        }
        osu->getTooltipOverlay()->end();
    }

    this->bFocusStolenDelay = false;
}

void UIModSelectorModButton::resetState() {
    this->setOn(false, true);
    this->setState(0);
}

void UIModSelectorModButton::onClicked(bool /*left*/, bool /*right*/) {
    if(!this->bAvailable) return;

    // increase state, wrap around, switch on and off
    if(this->bOn) {
        this->iState = (this->iState + 1) % this->states.size();

        // HACK: In multi, skip "Actual Flashlight" mod
        if(BanchoState::is_in_a_multi_room() && this->states[0].modName == UString("fl")) {
            this->iState = this->iState % this->states.size() - 1;
        }

        if(this->iState == 0) {
            this->setOn(false);
        } else {
            this->setOn(true);
        }
    } else {
        this->setOn(true);
    }

    // set new state
    this->setState(this->iState);

    if(BanchoState::is_in_a_multi_room()) {
        auto mod_flags = this->osuModSelector->getModFlags();
        for(auto &slot : BanchoState::room.slots) {
            if(slot.player_id != BanchoState::get_uid()) continue;
            slot.mods = mod_flags;
            if(BanchoState::room.is_host()) {
                BanchoState::room.mods = mod_flags;
                if(BanchoState::room.freemods) {
                    // When freemod is enabled, we only want to force DT, HT, or Target.
                    BanchoState::room.mods &= LegacyFlags::DoubleTime | LegacyFlags::HalfTime | LegacyFlags::Target;
                }
            }
        }

        Packet packet;
        packet.id = MATCH_CHANGE_MODS;
        packet.write<u32>(mod_flags);
        BANCHO::Net::send_packet(packet);

        // Don't wait on server response to update UI
        osu->getRoom()->on_room_updated(BanchoState::room);
    }

    if(BanchoState::is_online()) {
        RichPresence::updateBanchoMods();
    }
}

void UIModSelectorModButton::onFocusStolen() {
    CBaseUIButton::onFocusStolen();

    this->bMouseInside = false;
    this->bFocusStolenDelay = true;
}

void UIModSelectorModButton::setBaseScale(float xScale, float yScale) {
    this->vBaseScale.x = xScale;
    this->vBaseScale.y = yScale;
    this->vScale = this->vBaseScale;

    if(this->bOn) {
        this->vScale = this->vBaseScale * this->fEnabledScaleMultiplier;
        this->fRot = this->fEnabledRotationDeg;
    }
}

void UIModSelectorModButton::setOn(bool on, bool silent) {
    if(!this->bAvailable) return;

    bool prevState = this->bOn;
    this->bOn = on;
    float animationDuration = 0.05f;
    if(silent) {
        animationDuration = 0.f;
    }

    // Disable all states except current
    for(int i = 0; i < this->states.size(); i++) {
        if(i == this->iState) {
            if(this->states[i].cvar->getBool() != on) {
                this->states[i].cvar->setValue(on);
            }
        } else {
            if(this->states[i].cvar->getBool()) {
                this->states[i].cvar->setValue(false);
            }
        }
    }
    if(!silent) {
        osu->updateMods();
    }

    if(this->bOn) {
        if(prevState) {
            // swap effect
            float swapDurationMultiplier = 0.65f;
            anim->moveLinear(&this->fRot, 0.0f, animationDuration * swapDurationMultiplier, true);
            anim->moveLinear(&this->vScale.x, this->vBaseScale.x, animationDuration * swapDurationMultiplier, true);
            anim->moveLinear(&this->vScale.y, this->vBaseScale.y, animationDuration * swapDurationMultiplier, true);

            anim->moveLinear(&this->fRot, this->fEnabledRotationDeg, animationDuration * swapDurationMultiplier,
                             animationDuration * swapDurationMultiplier, false);
            anim->moveLinear(&this->vScale.x, this->vBaseScale.x * this->fEnabledScaleMultiplier,
                             animationDuration * swapDurationMultiplier, animationDuration * swapDurationMultiplier,
                             false);
            anim->moveLinear(&this->vScale.y, this->vBaseScale.y * this->fEnabledScaleMultiplier,
                             animationDuration * swapDurationMultiplier, animationDuration * swapDurationMultiplier,
                             false);
        } else {
            anim->moveLinear(&this->fRot, this->fEnabledRotationDeg, animationDuration, true);
            anim->moveLinear(&this->vScale.x, this->vBaseScale.x * this->fEnabledScaleMultiplier, animationDuration,
                             true);
            anim->moveLinear(&this->vScale.y, this->vBaseScale.y * this->fEnabledScaleMultiplier, animationDuration,
                             true);
        }

        if(!silent) {
            soundEngine->play(osu->getSkin()->getCheckOn());
        }
    } else {
        anim->moveLinear(&this->fRot, 0.0f, animationDuration, true);
        anim->moveLinear(&this->vScale.x, this->vBaseScale.x, animationDuration, true);
        anim->moveLinear(&this->vScale.y, this->vBaseScale.y, animationDuration, true);

        if(prevState && !this->bOn && !silent) {
            // only play sound on specific change
            soundEngine->play(osu->getSkin()->getCheckOff());
        }
    }
}

void UIModSelectorModButton::setState(int state) {
    this->iState = state;

    // update image
    if(this->iState < this->states.size() && this->states[this->iState].getImageFunc != nullptr) {
        this->getActiveImageFunc = this->states[this->iState].getImageFunc;
    }
}

void UIModSelectorModButton::setState(unsigned int state, bool initialState, ConVar *cvar, UString modName,
                                      const UString &tooltipText, std::function<SkinImage *()> getImageFunc) {
    // dynamically add new state
    while(this->states.size() < state + 1) {
        STATE t{};
        t.getImageFunc = nullptr;
        this->states.push_back(t);
    }
    this->states[state].cvar = cvar;
    this->states[state].modName = std::move(modName);
    this->states[state].tooltipTextLines = tooltipText.split("\n");
    this->states[state].getImageFunc = std::move(getImageFunc);

    // set initial state image
    if(this->states.size() == 1)
        this->getActiveImageFunc = this->states[0].getImageFunc;
    else if(this->iState > -1 && this->iState < this->states.size())  // update current state image
        this->getActiveImageFunc = this->states[this->iState].getImageFunc;

    // set initial state on (but without firing callbacks)
    if(initialState) {
        this->setState(state);
        this->setOn(true, true);
    }
}

UString UIModSelectorModButton::getActiveModName() {
    if(this->states.size() > 0 && this->iState < this->states.size())
        return this->states[this->iState].modName;
    else
        return "";
}
