// Copyright (c) 2015, PG, All rights reserved.
#ifndef KEYBOARDEVENT_H
#define KEYBOARDEVENT_H

#include "noinclude.h"

#include <cstdint>

using KEYCODE = uint16_t;

class KeyboardEvent {
   public:
    KeyboardEvent(KEYCODE keyCode, uint64_t timestamp) : timestamp(timestamp), keyCode(keyCode) {}

    constexpr forceinline void consume() { this->bConsumed = true; }

    [[nodiscard]] constexpr forceinline bool isConsumed() const { return this->bConsumed; }
    [[nodiscard]] constexpr forceinline KEYCODE getKeyCode() const { return this->keyCode; }
    [[nodiscard]] constexpr forceinline char16_t getCharCode() const { return this->keyCode; }  // these are equivalent

    inline bool operator==(const KEYCODE &rhs) const { return this->keyCode == rhs; }
    inline bool operator!=(const KEYCODE &rhs) const { return this->keyCode != rhs; }

    explicit operator KEYCODE() const { return this->keyCode; }

   private:
    uint64_t timestamp;
    KEYCODE keyCode;
    bool bConsumed{false};
};

#endif
