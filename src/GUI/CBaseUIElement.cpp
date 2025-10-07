// Copyright (c) 2013, PG, All rights reserved.
#include "CBaseUIElement.h"

#include "Engine.h"
#include "Logging.h"
#include "ConVar.h"
#include "Mouse.h"

bool CBaseUIElement::isVisibleOnScreen(const McRect &rect) { return engine->getScreenRect().intersects(rect); }

void CBaseUIElement::stealFocus() {
    this->mouseInsideCheck = static_cast<size_t>(static_cast<size_t>(this->bHandleLeftMouse) |
                                                 (static_cast<size_t>((this->bHandleRightMouse) << 1)));
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

    const std::bitset<2> buttonMask{static_cast<size_t>((this->bHandleLeftMouse && mouse->isLeftDown()) |
                                                        ((this->bHandleRightMouse && mouse->isRightDown()) << 1))};

    if(buttonMask.any() && *propagate_clicks) {
        this->mouseUpCheck |= buttonMask;
        if(this->bMouseInside) {
            *propagate_clicks = !this->grabs_clicks;
        }

        // onMouseDownOutside
        if(!this->bMouseInside && (this->mouseInsideCheck & buttonMask).none()) {
            this->mouseInsideCheck |= buttonMask;
            this->onMouseDownOutside(buttonMask[0], buttonMask[1]);
        }

        // onMouseDownInside
        if(this->bMouseInside && (this->mouseInsideCheck & buttonMask).none()) {
            this->bActive = true;
            this->mouseInsideCheck |= buttonMask;
            this->onMouseDownInside(buttonMask[0], buttonMask[1]);
        }
    }

    // detect which buttons were released for mouse up events
    const std::bitset<2> releasedButtons{this->mouseUpCheck & (~buttonMask)};
    if(releasedButtons.any() && this->bActive) {
        if(this->bMouseInside)
            this->onMouseUpInside(releasedButtons[0], releasedButtons[1]);
        else
            this->onMouseUpOutside(releasedButtons[0], releasedButtons[1]);

        if(!this->bKeepActive) this->bActive = false;
    }

    // remove released buttons from mouseUpCheck
    this->mouseUpCheck &= buttonMask;

    // reset mouseInsideCheck if all buttons are released
    if(buttonMask.none()) {
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
vmPos: {})",
                           currentFrame, updateCount, this->sName, this->bVisible, this->bActive, this->bBusy,
                           this->bEnabled, this->bKeepActive, this->bMouseInside, this->bHandleLeftMouse,
                           this->bHandleRightMouse, this->rect, this->relRect);
        }
        lastUpdateFrame = currentFrame;
        updateCount = 0;
    }
    updateCount++;
}
