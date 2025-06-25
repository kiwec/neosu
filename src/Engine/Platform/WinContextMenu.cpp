//================ Copyright (c) 2012, PG, All rights reserved. =================//
//
// Purpose:		windows context menu interface
//
// $NoKeywords: $wincmenu
//===============================================================================//

// TODO: DEPRECATED

#ifdef _WIN32

#include "WinContextMenu.h"

#include "Engine.h"
#include "WinEnvironment.h"

WinContextMenu::WinContextMenu() { this->menu = NULL; }

WinContextMenu::~WinContextMenu() {}

void WinContextMenu::begin() { this->menu = CreatePopupMenu(); }

void WinContextMenu::addItem(UString text, int returnValue) {
#ifdef _UNICODE
    InsertMenu(this->menu, 0, MF_BYPOSITION | MF_STRING, returnValue, text.wc_str());
#else
    InsertMenu(this->menu, 0, MF_BYPOSITION | MF_STRING, returnValue, text.toUtf8());
#endif
}

void WinContextMenu::addSeparator() {
    MENUITEMINFO mySep;

    mySep.cbSize = sizeof(MENUITEMINFO);
    mySep.fMask = MIIM_TYPE;
    mySep.fType = MFT_SEPARATOR;

    InsertMenuItem(this->menu, 0, 1, &mySep);
}

int WinContextMenu::end() {
    engine->focus();

    POINT p;
    GetCursorPos(&p);

    return TrackPopupMenu(this->menu, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD, p.x, p.y, 0,
                          ((WinEnvironment*)env)->getHwnd(), NULL);
}

#endif
