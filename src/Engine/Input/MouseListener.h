// Copyright (c) 2015, PG, All rights reserved.
#ifndef MOUSELISTENER_H
#define MOUSELISTENER_H

#include <cstdint>

namespace ButtonIndices {
enum idx : unsigned char {
 BUTTON_NONE = 0,
 BUTTON_LEFT = 1,
 BUTTON_MIDDLE = 2,
 BUTTON_RIGHT = 3,
 BUTTON_X1 = 4,
 BUTTON_X2 = 5,
 BUTTON_COUNT = 6,
};
}  // namespace ButtonIndices

using ButtonIndex = ButtonIndices::idx;

struct ButtonEvent {
    uint64_t timestamp;
    ButtonIndex btn;
    bool down;
};

class MouseListener {
   public:
    virtual ~MouseListener() = default;

    virtual void onButtonChange(ButtonEvent /*event*/) {}

    virtual void onWheelVertical(int /*delta*/) {}
    virtual void onWheelHorizontal(int /*delta*/) {}
};

#endif
