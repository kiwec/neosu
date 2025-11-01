// Copyright (c) 2015, PG, All rights reserved.
#ifndef MOUSELISTENER_H
#define MOUSELISTENER_H

#include "noinclude.h"

#include <cstdint>
// clang-format off
enum class MouseButtonFlags : uint8_t {
    MF_NONE =   0u,
    MF_LEFT =   1u << 0,
    MF_MIDDLE = 1u << 1,
    MF_RIGHT =  1u << 2,
    MF_X1 =     1u << 3,
    MF_X2 =     1u << 4,
    MF_COUNT =  1u << 5
};
// clang-format on

MAKE_FLAG_ENUM(MouseButtonFlags)

struct ButtonEvent {
    uint64_t timestamp;
    MouseButtonFlags btn;
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
