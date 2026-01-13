// Copyright (c) 2014, PG, All rights reserved.
#ifndef CWINDOWMANAGER_H
#define CWINDOWMANAGER_H

#include "KeyboardListener.h"
#include "Vectors.h"

#include <vector>

class CBaseUIWindow;
struct CBaseUIEventCtx;

class CWindowManager : public KeyboardListener {
   public:
    CWindowManager();
    ~CWindowManager() override;

    void draw();
    virtual void update(CBaseUIEventCtx &c);

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onResolutionChange(vec2 newResolution);

    void openAll();
    void closeAll();

    void addWindow(CBaseUIWindow *window);

    void setVisible(bool visible) { this->bVisible = visible; }
    void setEnabled(bool enabled);
    void setFocus(CBaseUIWindow *window);

    bool isMouseInside();
    bool isVisible();
    bool isActive();

    std::vector<CBaseUIWindow *> *getAllWindowsPointer() { return &this->windows; }

   private:
    int getTopMouseWindowIndex();

    bool bVisible;
    bool bEnabled;

    int iLastEnabledWindow;
    int iCurrentEnabledWindow;

    std::vector<CBaseUIWindow *> windows;
};

#endif
