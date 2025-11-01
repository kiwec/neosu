// Copyright (c) 2013, PG, All rights reserved.
#include "CBaseUIElement.h"

#include "Engine.h"
#include "Logging.h"
#include "ConVar.h"
#include "Mouse.h"

bool CBaseUIElement::isVisibleOnScreen(const McRect &rect) { return engine->getScreenRect().intersects(rect); }

void CBaseUIElement::stealFocus() {
    this->mouseInsideCheck = (u8)(this->bHandleLeftMouse << 1) | (u8)this->bHandleRightMouse;
    this->bActive = false;
    this->onFocusStolen();
}

void CBaseUIElement::mouse_update(bool *propagate_clicks) {
    if(unlikely(cv::debug_ui.getBool())) this->dumpElem();
    if(!this->bVisible || !this->bEnabled) return;

    // check if mouse is inside element
    if(this->getRect().contains(mouse->getPos())) {
        if(!this->bMouseInside) {
            this->bMouseInside = true;
            if(this->bVisible && this->bEnabled) this->onMouseInside();
        }
    } else {
        if(this->bMouseInside) {
            this->bMouseInside = false;
            if(this->bVisible && this->bEnabled) this->onMouseOutside();
        }
    }

    const u8 buttonMask =
        (u8)((this->bHandleLeftMouse && mouse->isLeftDown()) << 1) | (u8)(this->bHandleRightMouse && mouse->isRightDown());

    if(buttonMask && *propagate_clicks) {
        this->mouseUpCheck |= buttonMask;
        if(this->bMouseInside) {
            *propagate_clicks = !this->grabs_clicks;
        }

        // onMouseDownOutside
        if(!this->bMouseInside && !(this->mouseInsideCheck & buttonMask)) {
            this->mouseInsideCheck |= buttonMask;
            this->onMouseDownOutside((buttonMask & 0b10), (buttonMask & 0b01));
        }

        // onMouseDownInside
        if(this->bMouseInside && !(this->mouseInsideCheck & buttonMask)) {
            this->bActive = true;
            this->mouseInsideCheck |= buttonMask;
            this->onMouseDownInside((buttonMask & 0b10), (buttonMask & 0b01));
        }
    }

    // detect which buttons were released for mouse up events
    const u8 releasedButtons = this->mouseUpCheck & ~buttonMask;
    if(releasedButtons && this->bActive) {
        if(this->bMouseInside)
            this->onMouseUpInside((releasedButtons & 0b10), (releasedButtons & 0b01));
        else
            this->onMouseUpOutside((releasedButtons & 0b10), (releasedButtons & 0b01));

        if(!this->bKeepActive) this->bActive = false;
    }

    // remove released buttons from mouseUpCheck
    this->mouseUpCheck &= buttonMask;

    // reset mouseInsideCheck if all buttons are released
    if(!buttonMask) {
        this->mouseInsideCheck = 0b00;
    }
}

void CBaseUIElement::dumpElem() const {
    static size_t lastUpdateFrame = 0;
    static int updateCount = 0;
    size_t currentFrame = engine->getFrameCount();

    if(currentFrame != lastUpdateFrame) {
        if(lastUpdateFrame > 0) {
            Logger::logRaw(R"(frame: {}
updated {} times last frame
sName: {}
bVisible: {}
bActive: {}
bBusy: {}
bEnabled: {}
bKeepActive: {}
bMouseInside: {}
bHandleLeftMouse: {}
bHandleRightMouse: {}
vPos: {}
vmPos: {},
mouseInsideCheck: {:02b},
mouseUpCheck: {:02b})",
                           currentFrame, updateCount, this->sName, this->bVisible, this->bActive, this->bBusy,
                           this->bEnabled, this->bKeepActive, this->bMouseInside, this->bHandleLeftMouse,
                           this->bHandleRightMouse, this->rect, this->relRect, this->mouseInsideCheck,
                           this->mouseUpCheck);
        }
        lastUpdateFrame = currentFrame;
        updateCount = 0;
    }
    updateCount++;
}
