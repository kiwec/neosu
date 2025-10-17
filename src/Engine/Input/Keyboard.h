#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "InputDevice.h"
#include "KeyboardEvent.h"
#include "KeyBindings.h"
#include "KeyboardListener.h"

class Keyboard final : public InputDevice {
    NOCOPY_NOMOVE(Keyboard)

   public:
    Keyboard();
    ~Keyboard() override = default;

    void draw() override {}
    void update() override {}

    void addListener(KeyboardListener *keyboardListener, bool insertOnTop = false);
    void removeListener(KeyboardListener *keyboardListener);
    void reset();

    void onKeyDown(KEYCODE keyCode);
    void onKeyUp(KEYCODE keyCode);
    void onChar(char16_t charCode);

    [[nodiscard]] inline bool isControlDown() const { return this->bControlDown; }
    [[nodiscard]] inline bool isAltDown() const { return this->bAltDown; }
    [[nodiscard]] inline bool isShiftDown() const { return this->bShiftDown; }
    [[nodiscard]] inline bool isSuperDown() const { return this->bSuperDown; }

   private:
    bool bControlDown;
    bool bAltDown;
    bool bShiftDown;
    bool bSuperDown;

    std::vector<KeyboardListener *> listeners;
};
