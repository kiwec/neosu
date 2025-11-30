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

    // 2 bits to handle left/right
    // right bit is right ctrl/alt/shift/super, left bit is left
    unsigned controlDown : 2 {0};
    unsigned altDown : 2 {0};
    unsigned shiftDown : 2 {0};
    unsigned superDown : 2 {0};
};
