// Copyright (c) 2015, PG, All rights reserved.
#include "Keyboard.h"

#include "Engine.h"
#include "UString.h"

void Keyboard::reset() {
    this->bControlDown = false;
    this->bAltDown = false;
    this->bShiftDown = false;
    this->bSuperDown = false;
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
        case KEY_RCONTROL:
            this->bControlDown = true;
            break;
        case KEY_LALT:
        case KEY_RALT:
            this->bAltDown = true;
            break;

        case KEY_LSHIFT:
        case KEY_RSHIFT:
            this->bShiftDown = true;
            break;
        case KEY_LSUPER:
        case KEY_RSUPER:
            this->bSuperDown = true;
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
        case KEY_RCONTROL:
            this->bControlDown = false;
            break;
        case KEY_LALT:
        case KEY_RALT:
            this->bAltDown = false;
            break;

        case KEY_LSHIFT:
        case KEY_RSHIFT:
            this->bShiftDown = false;
            break;
        case KEY_LSUPER:
        case KEY_RSUPER:
            this->bSuperDown = false;
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
