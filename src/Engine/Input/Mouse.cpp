// Copyright (c) 2015, PG & 2025, WH, All rights reserved.
#include "Mouse.h"

#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "ResourceManager.h"

Mouse::Mouse() : InputDevice(), vPos(env->getMousePos()), vPosWithoutOffsets(this->vPos), vActualPos(this->vPos) {
    this->fSensitivity = cv::mouse_sensitivity.getFloat();
    this->bIsRawInputDesired = cv::mouse_raw_input.getBool();
    cv::mouse_raw_input.setCallback(SA::MakeDelegate<&Mouse::onRawInputChanged>(this));
    cv::mouse_sensitivity.setCallback(SA::MakeDelegate<&Mouse::onSensitivityChanged>(this));
}

void Mouse::draw() {
    if(!cv::debug_mouse.getBool()) return;

    this->drawDebug();

    // green rect = virtual cursor pos
    g->setColor(0xff00ff00);
    float size = 20.0f;
    g->drawRect(this->vActualPos.x - size / 2, this->vActualPos.y - size / 2, size, size);

    // red rect = real cursor pos
    g->setColor(0xffff0000);
    vec2 envPos = env->getMousePos();
    g->drawRect(envPos.x - size / 2, envPos.y - size / 2, size, size);

    // red = cursor clip
    if(env->isCursorClipped()) {
        McRect cursorClip = env->getCursorClip();
        g->drawRect(cursorClip.getMinX(), cursorClip.getMinY(), cursorClip.getWidth() - 1, cursorClip.getHeight() - 1);
    }

    // green = scaled & offset virtual area
    const vec2 scaledOffset = this->vOffset;
    const vec2 scaledEngineScreenSize = engine->getScreenSize() * this->vScale;
    g->setColor(0xff00ff00);
    g->drawRect(-scaledOffset.x, -scaledOffset.y, scaledEngineScreenSize.x, scaledEngineScreenSize.y);
}

void Mouse::drawDebug() {
    vec2 pos = this->getPos();

    g->setColor(0xff000000);
    g->drawLine(pos.x - 1, pos.y - 1, 0 - 1, pos.y - 1);
    g->drawLine(pos.x - 1, pos.y - 1, engine->getScreenWidth() - 1, pos.y - 1);
    g->drawLine(pos.x - 1, pos.y - 1, pos.x - 1, 0 - 1);
    g->drawLine(pos.x - 1, pos.y - 1, pos.x - 1, engine->getScreenHeight() - 1);

    g->setColor(0xffffffff);
    g->drawLine(pos.x, pos.y, 0, pos.y);
    g->drawLine(pos.x, pos.y, engine->getScreenWidth(), pos.y);
    g->drawLine(pos.x, pos.y, pos.x, 0);
    g->drawLine(pos.x, pos.y, pos.x, engine->getScreenHeight());

    float rectSizePercent = 0.05f;
    float aspectRatio = (float)engine->getScreenWidth() / (float)engine->getScreenHeight();
    vec2 rectSize = vec2(engine->getScreenWidth(), engine->getScreenHeight() * aspectRatio) * rectSizePercent;

    g->setColor(0xff000000);
    g->drawRect(pos.x - rectSize.x / 2.0f - 1, pos.y - rectSize.y / 2.0f - 1, rectSize.x, rectSize.y);

    g->setColor(0xffffffff);
    g->drawRect(pos.x - rectSize.x / 2.0f, pos.y - rectSize.y / 2.0f, rectSize.x, rectSize.y);

    McFont *posFont = resourceManager->getFont("FONT_DEFAULT");
    UString posString = UString::format("[%i, %i]", (int)pos.x, (int)pos.y);
    float stringWidth = posFont->getStringWidth(posString);
    float stringHeight = posFont->getHeight();
    vec2 textOffset = vec2(
        pos.x + rectSize.x / 2.0f + stringWidth + 5 > engine->getScreenWidth() ? -rectSize.x / 2.0f - stringWidth - 5
                                                                               : rectSize.x / 2.0f + 5,
        (pos.y + rectSize.y / 2.0f + stringHeight > engine->getScreenHeight()) ? -rectSize.y / 2.0f - stringHeight
                                                                               : rectSize.y / 2.0f + stringHeight);

    g->pushTransform();
    g->translate(pos.x + textOffset.x, pos.y + textOffset.y);
    g->drawString(posFont, UString::format("[%i, %i]", (int)pos.x, (int)pos.y));
    g->popTransform();
}

void Mouse::update() {
    this->resetWheelDelta();
    this->vDelta = {0.f,0.f};
    this->vRawDelta = {0.f,0.f};

    // <rel, abs> pair
    const auto envCachedMotion{env->consumeMousePositionCache()};
    vec2 newRel{envCachedMotion.first};
    if(vec::length(newRel) <= 0.f) return;  // early return for no motion

    vec2 newAbs{envCachedMotion.second};

    // vRawDelta doesn't include sensitivity or clipping, which is useful for fposu
    this->vRawDelta = newRel;

    if(env->isOSMouseInputRaw()) {
        // only relative input (raw) can have sensitivity
        newRel *= this->fSensitivity;
        // we only base the absolute position off of the relative motion for raw input
        newAbs = this->vPosWithoutOffsets + newRel;

        // apply clipping manually for rawinput, because it only clips the absolute position
        // which is decoupled from the relative position in relative mode
        // check this after applying sensitivity
        if(env->isCursorClipped()) {
            const McRect &clipRect{env->getCursorClip()};
            if(!clipRect.contains(newAbs)) {
                // re-calculate clamped cursor position
                newAbs = vec2{std::clamp<float>(newAbs.x, clipRect.getMinX(), clipRect.getMaxX()),
                                 std::clamp<float>(newAbs.y, clipRect.getMinY(), clipRect.getMaxY())};
                newRel = this->vPosWithoutOffsets - newAbs;
                if(vec::length(newRel) == 0) {
                    return;  // early return for the trivial case (like if we're confined in a corner)
                }
            }
        }
    }

    // if we got here, we have a motion delta to apply to the virtual cursor

    // vDelta includes transformations
    this->vDelta = newRel;

    // vPosWithoutOffsets should always match the post-transformation newAbs
    this->vPosWithoutOffsets = newAbs;

    this->onPosChange(this->vPosWithoutOffsets);
}

void Mouse::resetWheelDelta() {
    this->iWheelDeltaVertical = this->iWheelDeltaVerticalActual;
    this->iWheelDeltaVerticalActual = 0;

    this->iWheelDeltaHorizontal = this->iWheelDeltaHorizontalActual;
    this->iWheelDeltaHorizontalActual = 0;
}

void Mouse::onPosChange(vec2 pos) {
    this->vPosWithoutOffsets = pos;
    this->vPos = (this->vOffset + pos);
    this->vActualPos = this->vPos;

    // notify environment of the virtual cursor position
    env->updateCachedMousePos(this->vPosWithoutOffsets);
}

void Mouse::onWheelVertical(int delta) {
    this->iWheelDeltaVerticalActual += delta;

    for(auto &listener : this->listeners) {
        listener->onWheelVertical(delta);
    }
}

void Mouse::onWheelHorizontal(int delta) {
    this->iWheelDeltaHorizontalActual += delta;

    for(auto &listener : this->listeners) {
        listener->onWheelHorizontal(delta);
    }
}

void Mouse::onButtonChange(ButtonIndex button, bool down) {
    using enum ButtonIndex;
    if(button == BUTTON_NONE || button >= BUTTON_COUNT) return;

    this->bMouseButtonDownArray[static_cast<size_t>(button)] = down;

    // notify listeners
    for(auto &listener : this->listeners) {
        listener->onButtonChange(button, down);
    }
}

void Mouse::setPos(vec2 newPos) {
    this->vPos = newPos;
    env->setMousePos(newPos.x, newPos.y);
}

void Mouse::setOffset(vec2 offset) {
    vec2 oldOffset = this->vOffset;
    this->vOffset = offset;

    // update position to maintain visual position after offset change
    vec2 posAdjustment = this->vOffset - oldOffset;
    this->vPos += posAdjustment;
    this->vActualPos += posAdjustment;
}

void Mouse::addListener(MouseListener *mouseListener, bool insertOnTop) {
    if(mouseListener == nullptr) {
        engine->showMessageError("Mouse Error", "addListener(NULL)!");
        return;
    }

    if(insertOnTop)
        this->listeners.insert(this->listeners.begin(), mouseListener);
    else
        this->listeners.push_back(mouseListener);
}

void Mouse::removeListener(MouseListener *mouseListener) {
    for(size_t i = 0; i < this->listeners.size(); i++) {
        if(this->listeners[i] == mouseListener) {
            this->listeners.erase(this->listeners.begin() + i);
            i--;
        }
    }
}

void Mouse::onRawInputChanged(float newval) {
    this->bIsRawInputDesired = !!static_cast<int>(newval);
    env->setRawInput(this->bIsRawInputDesired);  // request environment to change the real OS cursor state (may or may
                                          // not take effect immediately)

    // non-rawinput with sensitivity != 1 is unsupported
    if(!this->bIsRawInputDesired && (this->fSensitivity < 0.999f || this->fSensitivity > 1.001f)) {
        debugLog("forced sensitivity to 1.0 due to raw input being disabled\n");
        cv::mouse_sensitivity.setValue(1.0f);
    }
}

void Mouse::onSensitivityChanged(float newSens) {
    this->fSensitivity = newSens;

    // non-rawinput with sensitivity != 1 is unsupported
    if(!this->bIsRawInputDesired && (this->fSensitivity < 0.999f || this->fSensitivity > 1.001f)) {
        debugLog("forced raw input enabled due to sensitivity != 1.0\n");
        cv::mouse_raw_input.setValue(true);
    }
}
