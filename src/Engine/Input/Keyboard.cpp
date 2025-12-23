// Copyright (c) 2015, PG, All rights reserved.
#include "Keyboard.h"

#include "Engine.h"
#include "UString.h"

void Keyboard::update() {
    // std::vector<char16_t> charEventsConsumed;
    while(!this->charEventQueue.empty()) {
        auto event = this->charEventQueue.front();
        this->charEventQueue.pop();
        this->onChar_internal(event);
        // if(event.isConsumed()) {
        //     charEventsConsumed.push_back(event.getCharCode());
        // }
    }

    while(!this->keyEventQueue.empty()) {
        auto fullEvent = this->keyEventQueue.front();
        this->keyEventQueue.pop();
        switch(fullEvent.type) {
            case Type::KEYDOWN:
                // If we got a char event and something handled it, don't let an associated keyDown event go through.
                // This isn't very clean, but it avoids issues like having some key bound to "X"
                // and trying to type "X" into a textbox. Depending on what action "X" is bound to,
                // something weird might happen instead of just entering the "X" character.
                // I'm not sure if this is way of correlating key events with char events is stable across keyboard layouts
                // and locales, but hopefully it's better than nothing.

                // TODO: actually, this needs more thought... not every char listener consumes events consistently,
                // and some of them (like PauseMenu) consume char events unconditionally as long as it's visible for some reason?

                /*
                if(auto it = std::ranges::find(charEventsConsumed, fullEvent.orig.getCharCode());
                   it != charEventsConsumed.end()) {
                    // remove it from consumed events (we might have more than 1 or unmatched pairs)
                    charEventsConsumed.erase(it);
                } else
                */

                this->onKeyDown_internal(fullEvent.orig);
                break;
            case Type::KEYUP:
                // Just let all key up events go through, we shouldn't have received a char event for these anyways.
                this->onKeyUp_internal(fullEvent.orig);
                break;
        }
    }
}

void Keyboard::onKeyDown(KeyboardEvent event) { this->keyEventQueue.emplace(event, Type::KEYDOWN); }
void Keyboard::onKeyUp(KeyboardEvent event) { this->keyEventQueue.emplace(event, Type::KEYUP); }
void Keyboard::onChar(KeyboardEvent event) { this->charEventQueue.emplace(event); }

void Keyboard::reset() {
    this->controlDown = 0;
    this->altDown = 0;
    this->shiftDown = 0;
    this->superDown = 0;
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

void Keyboard::onKeyDown_internal(KeyboardEvent &event) {
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

void Keyboard::onKeyUp_internal(KeyboardEvent &event) {
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

void Keyboard::onChar_internal(KeyboardEvent &event) {
    for(auto *listener : this->listeners) {
        listener->onChar(event);
        if(event.isConsumed()) {
            break;
        }
    }
}
