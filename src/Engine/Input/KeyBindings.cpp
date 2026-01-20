// Copyright (c) 2016, PG, All rights reserved.
#include "KeyBindings.h"
#include <SDL3/SDL_scancode.h>

// clang-format off
#if defined(_WIN32)
#include "WinDebloatDefs.h"
#include <windows.h>
#elif defined __linux__
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>
#endif
namespace KeyBindings {
i32 old_keycode_to_sdl_keycode(i32 key) {
    switch(key) {
#if defined(_WIN32)
        case 0x41: return KEY_A;
        case 0x42: return KEY_B;
        case 0x43: return KEY_C;
        case 0x44: return KEY_D;
        case 0x45: return KEY_E;
        case 0x46: return KEY_F;
        case 0x47: return KEY_G;
        case 0x48: return KEY_H;
        case 0x49: return KEY_I;
        case 0x4A: return KEY_J;
        case 0x4B: return KEY_K;
        case 0x4C: return KEY_L;
        case 0x4D: return KEY_M;
        case 0x4E: return KEY_N;
        case 0x4F: return KEY_O;
        case 0x50: return KEY_P;
        case 0x51: return KEY_Q;
        case 0x52: return KEY_R;
        case 0x53: return KEY_S;
        case 0x54: return KEY_T;
        case 0x55: return KEY_U;
        case 0x56: return KEY_V;
        case 0x57: return KEY_W;
        case 0x58: return KEY_X;
        case 0x59: return KEY_Y;
        case 0x5A: return KEY_Z;
        case 0x30: return KEY_0;
        case 0x31: return KEY_1;
        case 0x32: return KEY_2;
        case 0x33: return KEY_3;
        case 0x34: return KEY_4;
        case 0x35: return KEY_5;
        case 0x36: return KEY_6;
        case 0x37: return KEY_7;
        case 0x38: return KEY_8;
        case 0x39: return KEY_9;
        case VK_NUMPAD0: return KEY_NUMPAD0;
        case VK_NUMPAD1: return KEY_NUMPAD1;
        case VK_NUMPAD2: return KEY_NUMPAD2;
        case VK_NUMPAD3: return KEY_NUMPAD3;
        case VK_NUMPAD4: return KEY_NUMPAD4;
        case VK_NUMPAD5: return KEY_NUMPAD5;
        case VK_NUMPAD6: return KEY_NUMPAD6;
        case VK_NUMPAD7: return KEY_NUMPAD7;
        case VK_NUMPAD8: return KEY_NUMPAD8;
        case VK_NUMPAD9: return KEY_NUMPAD9;
        case VK_MULTIPLY: return KEY_MULTIPLY;
        case VK_ADD: return KEY_ADD;
        case VK_SEPARATOR: return KEY_SEPARATOR;
        case VK_SUBTRACT: return KEY_SUBTRACT;
        case VK_DECIMAL: return KEY_DECIMAL;
        case VK_DIVIDE: return KEY_DIVIDE;
        case VK_F1: return KEY_F1;
        case VK_F2: return KEY_F2;
        case VK_F3: return KEY_F3;
        case VK_F4: return KEY_F4;
        case VK_F5: return KEY_F5;
        case VK_F6: return KEY_F6;
        case VK_F7: return KEY_F7;
        case VK_F8: return KEY_F8;
        case VK_F9: return KEY_F9;
        case VK_F10: return KEY_F10;
        case VK_F11: return KEY_F11;
        case VK_F12: return KEY_F12;
        case VK_LEFT: return KEY_LEFT;
        case VK_UP: return KEY_UP;
        case VK_RIGHT: return KEY_RIGHT;
        case VK_DOWN: return KEY_DOWN;
        case VK_TAB: return KEY_TAB;
        case VK_RETURN: return KEY_ENTER;
        case VK_LSHIFT: return KEY_LSHIFT;
        case VK_RSHIFT: return KEY_RSHIFT;
        case VK_LCONTROL: return KEY_LCONTROL;
        case VK_RCONTROL: return KEY_RCONTROL;
        case VK_LMENU: return KEY_LALT;
        case VK_RMENU: return KEY_RALT;
        case VK_LWIN: return KEY_LSUPER;
        case VK_RWIN: return KEY_RSUPER;
        case VK_ESCAPE: return KEY_ESCAPE;
        case VK_SPACE: return KEY_SPACE;
        case VK_BACK: return KEY_BACKSPACE;
        case VK_END: return KEY_END;
        case VK_INSERT: return KEY_INSERT;
        case VK_DELETE: return KEY_DELETE;
        case VK_HELP: return KEY_HELP;
        case VK_HOME: return KEY_HOME;
        case VK_PRIOR: return KEY_PAGEUP;
        case VK_NEXT: return KEY_PAGEDOWN;
#elif defined __linux__
        case XK_A: return KEY_A;
        case XK_B: return KEY_B;
        case XK_C: return KEY_C;
        case XK_D: return KEY_D;
        case XK_E: return KEY_E;
        case XK_F: return KEY_F;
        case XK_G: return KEY_G;
        case XK_H: return KEY_H;
        case XK_I: return KEY_I;
        case XK_J: return KEY_J;
        case XK_K: return KEY_K;
        case XK_L: return KEY_L;
        case XK_M: return KEY_M;
        case XK_N: return KEY_N;
        case XK_O: return KEY_O;
        case XK_P: return KEY_P;
        case XK_Q: return KEY_Q;
        case XK_R: return KEY_R;
        case XK_S: return KEY_S;
        case XK_T: return KEY_T;
        case XK_U: return KEY_U;
        case XK_V: return KEY_V;
        case XK_W: return KEY_W;
        case XK_X: return KEY_X;
        case XK_Y: return KEY_Y;
        case XK_Z: return KEY_Z;
        case XK_equal: return KEY_0;
        case XK_exclam: return KEY_1;
        case XK_quotedbl: return KEY_2;
        case XK_section: return KEY_3;
        case XK_dollar: return KEY_4;
        case XK_percent: return KEY_5;
        case XK_ampersand: return KEY_6;
        case XK_slash: return KEY_7;
        case XK_quoteright: return KEY_8;
        case XK_parenleft: return KEY_9;
        case XK_KP_0: return KEY_NUMPAD0;
        case XK_KP_1: return KEY_NUMPAD1;
        case XK_KP_2: return KEY_NUMPAD2;
        case XK_KP_3: return KEY_NUMPAD3;
        case XK_KP_4: return KEY_NUMPAD4;
        case XK_KP_5: return KEY_NUMPAD5;
        case XK_KP_6: return KEY_NUMPAD6;
        case XK_KP_7: return KEY_NUMPAD7;
        case XK_KP_8: return KEY_NUMPAD8;
        case XK_KP_9: return KEY_NUMPAD9;
        case XK_KP_Multiply: return KEY_MULTIPLY;
        case XK_KP_Add: return KEY_ADD;
        case XK_KP_Separator: return KEY_SEPARATOR;
        case XK_KP_Subtract: return KEY_SUBTRACT;
        case XK_KP_Decimal: return KEY_DECIMAL;
        case XK_KP_Divide: return KEY_DIVIDE;
        case XK_F1: return KEY_F1;
        case XK_F2: return KEY_F2;
        case XK_F3: return KEY_F3;
        case XK_F4: return KEY_F4;
        case XK_F5: return KEY_F5;
        case XK_F6: return KEY_F6;
        case XK_F7: return KEY_F7;
        case XK_F8: return KEY_F8;
        case XK_F9: return KEY_F9;
        case XK_F10: return KEY_F10;
        case XK_F11: return KEY_F11;
        case XK_F12: return KEY_F12;
        case XK_Left: return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        case XK_Up: return KEY_UP;
        case XK_Down: return KEY_DOWN;
        case XK_Tab: return KEY_TAB;
        case XK_Return: return KEY_ENTER;
        case XK_KP_Enter: return KEY_NUMPAD_ENTER;
        case XK_Shift_L: return KEY_LSHIFT;
        case XK_Shift_R: return KEY_RSHIFT;
        case XK_Control_L: return KEY_LCONTROL;
        case XK_Control_R: return KEY_RCONTROL;
        case XK_Alt_L: return KEY_LALT;
        case XK_Alt_R: return KEY_RALT;
        case XK_Super_R: return KEY_LSUPER;
        case XK_Super_L: return KEY_RSUPER;
        case XK_Escape: return KEY_ESCAPE;
        case XK_space: return KEY_SPACE;
        case XK_BackSpace: return KEY_BACKSPACE;
        case XK_End: return KEY_END;
        case XK_Insert: return KEY_INSERT;
        case XK_Delete: return KEY_DELETE;
        case XK_Help: return KEY_HELP;
        case XK_Home: return KEY_HOME;
        case XK_Prior: return KEY_PAGEUP;
        case XK_Next: return KEY_PAGEDOWN;
#endif
    }

    return 0;
}
}

// clang-format on

// just a sanity check to make sure these stay in sync

// alphabet
static_assert((u32)KEY_A == SDL_SCANCODE_A);
static_assert((u32)KEY_B == SDL_SCANCODE_B);
static_assert((u32)KEY_C == SDL_SCANCODE_C);
static_assert((u32)KEY_D == SDL_SCANCODE_D);
static_assert((u32)KEY_E == SDL_SCANCODE_E);
static_assert((u32)KEY_F == SDL_SCANCODE_F);
static_assert((u32)KEY_G == SDL_SCANCODE_G);
static_assert((u32)KEY_H == SDL_SCANCODE_H);
static_assert((u32)KEY_I == SDL_SCANCODE_I);
static_assert((u32)KEY_J == SDL_SCANCODE_J);
static_assert((u32)KEY_K == SDL_SCANCODE_K);
static_assert((u32)KEY_L == SDL_SCANCODE_L);
static_assert((u32)KEY_M == SDL_SCANCODE_M);
static_assert((u32)KEY_N == SDL_SCANCODE_N);
static_assert((u32)KEY_O == SDL_SCANCODE_O);
static_assert((u32)KEY_P == SDL_SCANCODE_P);
static_assert((u32)KEY_Q == SDL_SCANCODE_Q);
static_assert((u32)KEY_R == SDL_SCANCODE_R);
static_assert((u32)KEY_S == SDL_SCANCODE_S);
static_assert((u32)KEY_T == SDL_SCANCODE_T);
static_assert((u32)KEY_U == SDL_SCANCODE_U);
static_assert((u32)KEY_V == SDL_SCANCODE_V);
static_assert((u32)KEY_W == SDL_SCANCODE_W);
static_assert((u32)KEY_X == SDL_SCANCODE_X);
static_assert((u32)KEY_Y == SDL_SCANCODE_Y);
static_assert((u32)KEY_Z == SDL_SCANCODE_Z);

// numbers
static_assert((u32)KEY_0 == SDL_SCANCODE_0);
static_assert((u32)KEY_1 == SDL_SCANCODE_1);
static_assert((u32)KEY_2 == SDL_SCANCODE_2);
static_assert((u32)KEY_3 == SDL_SCANCODE_3);
static_assert((u32)KEY_4 == SDL_SCANCODE_4);
static_assert((u32)KEY_5 == SDL_SCANCODE_5);
static_assert((u32)KEY_6 == SDL_SCANCODE_6);
static_assert((u32)KEY_7 == SDL_SCANCODE_7);
static_assert((u32)KEY_8 == SDL_SCANCODE_8);
static_assert((u32)KEY_9 == SDL_SCANCODE_9);

// numpad
static_assert((u32)KEY_NUMPAD0 == SDL_SCANCODE_KP_0);
static_assert((u32)KEY_NUMPAD1 == SDL_SCANCODE_KP_1);
static_assert((u32)KEY_NUMPAD2 == SDL_SCANCODE_KP_2);
static_assert((u32)KEY_NUMPAD3 == SDL_SCANCODE_KP_3);
static_assert((u32)KEY_NUMPAD4 == SDL_SCANCODE_KP_4);
static_assert((u32)KEY_NUMPAD5 == SDL_SCANCODE_KP_5);
static_assert((u32)KEY_NUMPAD6 == SDL_SCANCODE_KP_6);
static_assert((u32)KEY_NUMPAD7 == SDL_SCANCODE_KP_7);
static_assert((u32)KEY_NUMPAD8 == SDL_SCANCODE_KP_8);
static_assert((u32)KEY_NUMPAD9 == SDL_SCANCODE_KP_9);
static_assert((u32)KEY_MULTIPLY == SDL_SCANCODE_KP_MULTIPLY);
static_assert((u32)KEY_ADD == SDL_SCANCODE_KP_PLUS);
static_assert((u32)KEY_SEPARATOR == SDL_SCANCODE_KP_EQUALS);
static_assert((u32)KEY_SUBTRACT == SDL_SCANCODE_KP_MINUS);
static_assert((u32)KEY_DECIMAL == SDL_SCANCODE_KP_DECIMAL);
static_assert((u32)KEY_DIVIDE == SDL_SCANCODE_KP_DIVIDE);

// function keys
static_assert((u32)KEY_F1 == SDL_SCANCODE_F1);
static_assert((u32)KEY_F2 == SDL_SCANCODE_F2);
static_assert((u32)KEY_F3 == SDL_SCANCODE_F3);
static_assert((u32)KEY_F4 == SDL_SCANCODE_F4);
static_assert((u32)KEY_F5 == SDL_SCANCODE_F5);
static_assert((u32)KEY_F6 == SDL_SCANCODE_F6);
static_assert((u32)KEY_F7 == SDL_SCANCODE_F7);
static_assert((u32)KEY_F8 == SDL_SCANCODE_F8);
static_assert((u32)KEY_F9 == SDL_SCANCODE_F9);
static_assert((u32)KEY_F10 == SDL_SCANCODE_F10);
static_assert((u32)KEY_F11 == SDL_SCANCODE_F11);
static_assert((u32)KEY_F12 == SDL_SCANCODE_F12);

// arrow keys
static_assert((u32)KEY_LEFT == SDL_SCANCODE_LEFT);
static_assert((u32)KEY_RIGHT == SDL_SCANCODE_RIGHT);
static_assert((u32)KEY_UP == SDL_SCANCODE_UP);
static_assert((u32)KEY_DOWN == SDL_SCANCODE_DOWN);

// special keys
static_assert((u32)KEY_TAB == SDL_SCANCODE_TAB);
static_assert((u32)KEY_NUMPAD_ENTER == SDL_SCANCODE_KP_ENTER);
static_assert((u32)KEY_ENTER == SDL_SCANCODE_RETURN);
static_assert((u32)KEY_LSHIFT == SDL_SCANCODE_LSHIFT);
static_assert((u32)KEY_RSHIFT == SDL_SCANCODE_RSHIFT);
static_assert((u32)KEY_LCONTROL == SDL_SCANCODE_LCTRL);
static_assert((u32)KEY_RCONTROL == SDL_SCANCODE_RCTRL);
static_assert((u32)KEY_LALT == SDL_SCANCODE_LALT);
static_assert((u32)KEY_RALT == SDL_SCANCODE_RALT);
static_assert((u32)KEY_ESCAPE == SDL_SCANCODE_ESCAPE);
static_assert((u32)KEY_TILDE == SDL_SCANCODE_GRAVE);
static_assert((u32)KEY_SPACE == SDL_SCANCODE_SPACE);
static_assert((u32)KEY_BACKSPACE == SDL_SCANCODE_BACKSPACE);
static_assert((u32)KEY_END == SDL_SCANCODE_END);
static_assert((u32)KEY_INSERT == SDL_SCANCODE_INSERT);
static_assert((u32)KEY_DELETE == SDL_SCANCODE_DELETE);
static_assert((u32)KEY_HELP == SDL_SCANCODE_HELP);
static_assert((u32)KEY_HOME == SDL_SCANCODE_HOME);
static_assert((u32)KEY_LSUPER == SDL_SCANCODE_LGUI);
static_assert((u32)KEY_RSUPER == SDL_SCANCODE_RGUI);
static_assert((u32)KEY_PAGEUP == SDL_SCANCODE_PAGEUP);
static_assert((u32)KEY_PAGEDOWN == SDL_SCANCODE_PAGEDOWN);

// media keys
static_assert((u32)KEY_PLAY == SDL_SCANCODE_MEDIA_PLAY);
static_assert((u32)KEY_PAUSE == SDL_SCANCODE_MEDIA_PAUSE);
static_assert((u32)KEY_PLAYPAUSE == SDL_SCANCODE_MEDIA_PLAY_PAUSE);
static_assert((u32)KEY_STOP == SDL_SCANCODE_MEDIA_STOP);
static_assert((u32)KEY_PREV == SDL_SCANCODE_MEDIA_PREVIOUS_TRACK);
static_assert((u32)KEY_NEXT == SDL_SCANCODE_MEDIA_NEXT_TRACK);
static_assert((u32)KEY_MUTE == SDL_SCANCODE_MUTE);
static_assert((u32)KEY_VOLUMEDOWN == SDL_SCANCODE_VOLUMEDOWN);
static_assert((u32)KEY_VOLUMEUP == SDL_SCANCODE_VOLUMEUP);
