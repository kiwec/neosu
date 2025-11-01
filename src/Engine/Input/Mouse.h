// Copyright (c) 2015, PG & 2025, WH, All rights reserved.
#pragma once
#ifndef MOUSE_H
#define MOUSE_H

#include "InputDevice.h"
#include "MouseListener.h"

class Mouse final : public InputDevice {
    NOCOPY_NOMOVE(Mouse)

   public:
    Mouse();
    ~Mouse() override = default;

    void draw() override;
    void update() override;

    void drawDebug();

    // event handling
    void addListener(MouseListener *mouseListener, bool insertOnTop = false);
    void removeListener(MouseListener *mouseListener);

    // input handling
    void onPosChange(vec2 pos);
    void onWheelVertical(int delta);
    void onWheelHorizontal(int delta);
    void onButtonChange(ButtonEvent ev);

    // position/coordinate handling
    void setPos(vec2 pos);  // NOT OS mouse pos, virtual mouse pos
    void setOffset(vec2 offset);
    inline void setScale(vec2 scale) { this->vScale = scale; }

    // state getters
    [[nodiscard]] inline vec2 getPos() const { return this->vPos; }
    [[nodiscard]] inline vec2 getRealPos() const { return this->vPosWithoutOffsets; }
    [[nodiscard]] inline vec2 getActualPos() const { return this->vActualPos; }
    [[nodiscard]] inline vec2 getDelta() const { return this->vDelta; }
    [[nodiscard]] inline vec2 getRawDelta() const { return this->vRawDelta; }

    [[nodiscard]] inline vec2 getOffset() const { return this->vOffset; }
    [[nodiscard]] inline vec2 getScale() const { return this->vScale; }
    [[nodiscard]] inline float getSensitivity() const { return this->fSensitivity; }

    // button state accessors
    [[nodiscard]] constexpr bool isLeftDown() const {
        return flags::has<MouseButtonFlags::MF_LEFT>(this->buttonsHeldMask);
    }
    [[nodiscard]] constexpr bool isMiddleDown() const {
        return flags::has<MouseButtonFlags::MF_MIDDLE>(this->buttonsHeldMask);
    }
    [[nodiscard]] constexpr bool isRightDown() const {
        return flags::has<MouseButtonFlags::MF_RIGHT>(this->buttonsHeldMask);
    }
    [[nodiscard]] constexpr bool isButton4Down() const {
        return flags::has<MouseButtonFlags::MF_X1>(this->buttonsHeldMask);
    }
    [[nodiscard]] constexpr bool isButton5Down() const {
        return flags::has<MouseButtonFlags::MF_X2>(this->buttonsHeldMask);
    }
    [[nodiscard]] constexpr MouseButtonFlags getHeldButtons() const { return this->buttonsHeldMask; }

    [[nodiscard]] inline int getWheelDeltaVertical() const { return this->iWheelDeltaVertical; }
    [[nodiscard]] inline int getWheelDeltaHorizontal() const { return this->iWheelDeltaHorizontal; }

    void resetWheelDelta();

    [[nodiscard]] inline bool isRawInputWanted() const {
        return this->bIsRawInputDesired;
    }  // "desired" rawinput state, NOT actual OS raw input state!

   private:
    // callbacks
    void onSensitivityChanged(float newSens);
    void onRawInputChanged(float newVal);

    // position state
    vec2 vPos{0.f};                // position with offset applied
    vec2 vPosWithoutOffsets{0.f};  // position without offset
    vec2 vDelta{0.f};              // movement delta in the current frame
    vec2 vRawDelta{0.f};   // movement delta in the current frame, without consideration for clipping or sensitivity
    vec2 vActualPos{0.f};  // final cursor position after all transformations

    // mode tracking
    bool bIsRawInputDesired{false};  // whether the user wants raw (relative) input
    float fSensitivity{1.0f};

    // button state (using our internal button index)
    MouseButtonFlags buttonsHeldMask{0};

    // wheel state
    int iWheelDeltaVertical{0};
    int iWheelDeltaHorizontal{0};
    int iWheelDeltaVerticalActual{0};
    int iWheelDeltaHorizontalActual{0};

    // listeners
    std::vector<MouseListener *> listeners;

    // transform parameters
    vec2 vOffset{0, 0};  // offset applied to coordinates
    vec2 vScale{1, 1};   // scale applied to coordinates
};

#endif
