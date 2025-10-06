// Copyright (c) 2018, PG, All rights reserved.
#ifndef OSUICONS_H
#define OSUICONS_H

#include <vector>

class Icons {
   public:
    static std::vector<char16_t> icons;

    static char16_t addIcon(char16_t character) {
        icons.push_back(character);
        return character;
    }

    static char16_t Z_UNKNOWN_CHAR;
    static char16_t Z_SPACE;

    static char16_t GEAR;
    static char16_t DESKTOP;
    static char16_t CIRCLE;
    static char16_t CUBE;
    static char16_t VOLUME_UP;
    static char16_t VOLUME_DOWN;
    static char16_t VOLUME_OFF;
    static char16_t PAINTBRUSH;
    static char16_t GAMEPAD;
    static char16_t WRENCH;
    static char16_t EYE;
    static char16_t ARROW_CIRCLE_UP;
    static char16_t TROPHY;
    static char16_t CARET_DOWN;
    static char16_t ARROW_DOWN;
    static char16_t GLOBE;
    static char16_t USER;
    static char16_t UNDO;
    static char16_t KEYBOARD;
    static char16_t LOCK;
    static char16_t UNLOCK;
};

#endif
