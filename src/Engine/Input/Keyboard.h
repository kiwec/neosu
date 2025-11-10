#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "InputDevice.h"
#include "KeyboardEvent.h"
#include "KeyBindings.h"
#include "KeyboardListener.h"

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

    [[nodiscard]] constexpr forceinline bool isControlDown() const { return this->bControlDown; }
    [[nodiscard]] constexpr forceinline bool isAltDown() const { return this->bAltDown; }
    [[nodiscard]] constexpr forceinline bool isShiftDown() const { return this->bShiftDown; }
    [[nodiscard]] constexpr forceinline bool isSuperDown() const { return this->bSuperDown; }

   private:
    std::vector<KeyboardListener *> listeners;

    bool bControlDown{false};
    bool bAltDown{false};
    bool bShiftDown{false};
    bool bSuperDown{false};
};
