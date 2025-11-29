// Copyright (c) 2015, PG, All rights reserved.
#include "Keyboard.h"

#include "Engine.h"
#include "UString.h"

void Keyboard::reset() {
    this->controlDown = 0b00;
    this->altDown = 0b00;
    this->shiftDown = 0b00;
    this->superDown = 0b00;
}

void Keyboard::addListener(KeyboardListener *keyboardListener, bool insertOnTop) {
    if(keyboardListener == nullptr) {
        engine->showMessageError(u"Keyboard Error", u"addListener(NULL)!");
        return;
    }

    if(insertOnTop)
        this->listeners.insert(this->listeners.begin(), keyboardListener);
    else
        this->listeners.push_back(keyboardListener);
}

void Keyboard::removeListener(KeyboardListener *keyboardListener) {
    std::erase_if(this->listeners,
                  [keyboardListener](const auto &listener) -> bool { return listener == keyboardListener; });
}

void Keyboard::onKeyDown(KeyboardEvent event) {
    switch(event.getScanCode()) {
        case KEY_LCONTROL:
            this->controlDown |= 0b10;
            break;
        case KEY_RCONTROL:
            this->controlDown |= 0b01;
            break;
        case KEY_LALT:
            this->altDown |= 0b10;
            break;
        case KEY_RALT:
            this->altDown |= 0b01;
            break;
        case KEY_LSHIFT:
            this->shiftDown |= 0b10;
            break;
        case KEY_RSHIFT:
            this->shiftDown |= 0b01;
            break;
        case KEY_LSUPER:
            this->superDown |= 0b10;
            break;
        case KEY_RSUPER:
            this->superDown |= 0b01;
            break;
        default:
            break;
    }

    for(auto *listener : this->listeners) {
        listener->onKeyDown(event);
        if(event.isConsumed()) {
            break;
        }
    }
}

void Keyboard::onKeyUp(KeyboardEvent event) {
    switch(event.getScanCode()) {
        case KEY_LCONTROL:
            this->controlDown &= 0b01;
            break;
        case KEY_RCONTROL:
            this->controlDown &= 0b10;
            break;
        case KEY_LALT:
            this->altDown &= 0b01;
            break;
        case KEY_RALT:
            this->altDown &= 0b10;
            break;
        case KEY_LSHIFT:
            this->shiftDown &= 0b01;
            break;
        case KEY_RSHIFT:
            this->shiftDown &= 0b10;
            break;
        case KEY_LSUPER:
            this->superDown &= 0b01;
            break;
        case KEY_RSUPER:
            this->superDown &= 0b10;
            break;
        default:
            break;
    }

    for(auto *listener : this->listeners) {
        listener->onKeyUp(event);
        if(event.isConsumed()) {
            break;
        }
    }
}

void Keyboard::onChar(KeyboardEvent event) {
    for(auto *listener : this->listeners) {
        listener->onChar(event);
        if(event.isConsumed()) {
            break;
        }
    }
}
