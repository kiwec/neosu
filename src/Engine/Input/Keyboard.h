#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "InputDevice.h"
#include "KeyboardEvent.h"
#include "KeyBindings.h"
#include "KeyboardListener.h"

#include <vector>

class Keyboard final : public InputDevice {
    NOCOPY_NOMOVE(Keyboard)

   public:
    Keyboard() = default;
    ~Keyboard() override = default;

    void reset();
    void draw() override {}
    void update() override {}

    void addListener(KeyboardListener *keyboardListener, bool insertOnTop = false);
    void removeListener(KeyboardListener *keyboardListener);

    void onKeyDown(KeyboardEvent event);
    void onKeyUp(KeyboardEvent event);
    void onChar(KeyboardEvent event);

    [[nodiscard]] constexpr forceinline bool isControlDown() const { return this->controlDown > 0; }
    [[nodiscard]] constexpr forceinline bool isAltDown() const { return this->altDown > 0; }
    [[nodiscard]] constexpr forceinline bool isShiftDown() const { return this->shiftDown > 0; }
    [[nodiscard]] constexpr forceinline bool isSuperDown() const { return this->superDown > 0; }

   private:
    std::vector<KeyboardListener *> listeners;

    // each holds 2 bits of state to handle left/right
    // first bit is right ctrl/alt/shift/super, second bit is left
    u8 controlDown{0};
    u8 altDown{0};
    u8 shiftDown{0};
    u8 superDown{0};
};
