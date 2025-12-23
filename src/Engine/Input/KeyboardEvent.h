// Copyright (c) 2015, PG, All rights reserved.
#ifndef KEYBOARDEVENT_H
#define KEYBOARDEVENT_H

#include "noinclude.h"

#include <cstdint>

using SCANCODE = uint16_t;
using KEYCODE = uint32_t;
using KEYCODES = KEYCODE;

class KeyboardEvent {
   public:
    KeyboardEvent(SCANCODE scanCode, char16_t charCode, uint64_t timestamp, bool repeat = false)
        : timestamp(timestamp), scanCode(scanCode), charCode(charCode), bRepeat(repeat) {}

    constexpr forceinline void consume() { this->bConsumed = true; }

    [[nodiscard]] constexpr forceinline bool isConsumed() const { return this->bConsumed; }
    [[nodiscard]] constexpr forceinline bool isRepeat() const { return this->bRepeat; }
    [[nodiscard]] constexpr forceinline SCANCODE getScanCode() const { return this->scanCode; }
    [[nodiscard]] constexpr forceinline char16_t getCharCode() const { return this->charCode; }
    [[nodiscard]] constexpr forceinline uint64_t getTimestamp() const { return this->timestamp; }

    inline bool operator==(const SCANCODE &rhs) const { return this->scanCode == rhs; }
    inline bool operator!=(const SCANCODE &rhs) const { return this->scanCode != rhs; }

    explicit operator SCANCODE() const { return this->scanCode; }

   private:
    uint64_t timestamp;
    SCANCODE scanCode;
    char16_t charCode;
    bool bRepeat;
    bool bConsumed{false};
};

#endif
