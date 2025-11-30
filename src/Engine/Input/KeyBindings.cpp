// Copyright (c) 2016, PG, All rights reserved.
#include "KeyBindings.h"

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
