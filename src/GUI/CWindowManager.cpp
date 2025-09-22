// Copyright (c) 2014, PG, All rights reserved.
#include "CWindowManager.h"

#include "CBaseUIWindow.h"
#include "Engine.h"
// #include "ResourceManager.h"
// #include "Logging.h"

CWindowManager::CWindowManager() {
    this->bVisible = true;
    this->bEnabled = true;
    this->iLastEnabledWindow = 0;
    this->iCurrentEnabledWindow = 0;
}

CWindowManager::~CWindowManager() {
    for(auto &window : this->windows) {
        delete window;
    }
}

void CWindowManager::draw() {
    if(!this->bVisible) return;

    for(int i = this->windows.size() - 1; i >= 0; i--) {
        this->windows[i]->draw();
    }

    /*
    for (size_t i=0; i<m_windows.size(); i++)
    {
            UString text = UString::format(" -%i-  ", i);
            text.append(this->windows[i]->getName());
            g->pushTransform();
                    g->translate(20, i*40+350);
                    g->drawString(resourceManager->getFont("FONT_DEFAULT"), text);
            g->popTransform();
    }

    g->pushTransform();
    g->translate(20, 500);
    g->drawString(resourceManager->getFont("FONT_DEFAULT"), UString::format("currentEnabled = %i,
    lastEnabled = %i, topIndex = %i", this->iCurrentEnabledWindow, this->iLastEnabledWindow, getTopMouseWindowIndex()));
    g->popTransform();
    */
}

void CWindowManager::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible || this->windows.size() == 0) return;

    // update all windows, detect depth changes
    for(auto &window : this->windows) {
        window->mouse_update(propagate_clicks);
        if(!*propagate_clicks) break;
    }
    int topMouseWindowIndex = this->getTopMouseWindowIndex();

    // don't event think about switching if we are disabled
    if(!this->bEnabled) return;

    // TODO: the logic here is fucked
    // if the topmost window isn't doing anything, we may be switching
    if(!this->windows[0]->isBusy()) {
        if(topMouseWindowIndex < this->windows.size()) this->iCurrentEnabledWindow = topMouseWindowIndex;

        // enable the closest window under the cursor on a change, disable the others
        if(this->iCurrentEnabledWindow != this->iLastEnabledWindow) {
            this->iLastEnabledWindow = this->iCurrentEnabledWindow;
            if(this->iLastEnabledWindow < this->windows.size()) {
                this->windows[this->iLastEnabledWindow]->setEnabled(true);
                // debugLog("enabled {:s} @{:f}\n", this->windows[m_iLastEnabledWindow]->getName().toUtf8(),
                // engine->getTime());
                for(size_t i = 0; i < this->windows.size(); i++) {
                    if(i != this->iLastEnabledWindow) this->windows[i]->setEnabled(false);
                }
            }
        }

        // now check if we have to switch any windows
        bool topSwitch = false;
        int newTop = 0;
        for(size_t i = 0; i < this->windows.size(); i++) {
            if(this->windows[i]->isBusy() && !topSwitch) {
                topSwitch = true;
                newTop = i;
            }
        }

        // switch top window to m_windows[0], all others get pushed down
        if(topSwitch && newTop != 0) {
            // debugLog("top switch @{:f}\n", engine->getTime());
            if(this->windows.size() > 1) {
                CBaseUIWindow *newTopWindow = this->windows[newTop];
                newTopWindow->setEnabled(true);

                for(int i = newTop - 1; i >= 0; i--) {
                    this->windows[i + 1] = this->windows[i];
                }

                this->windows[0] = newTopWindow;
                this->iCurrentEnabledWindow = this->iLastEnabledWindow = 0;
            }
        }
    }
}

int CWindowManager::getTopMouseWindowIndex() {
    int tempEnabledWindow = this->windows.size() - 1;
    for(size_t i = 0; i < this->windows.size(); i++) {
        if(this->windows[i]->isMouseInside() && i < tempEnabledWindow) tempEnabledWindow = i;
    }
    return tempEnabledWindow;
}

void CWindowManager::addWindow(CBaseUIWindow *window) {
    if(window == nullptr) engine->showMessageError("Window Manager Error", "addWindow(NULL), you maggot!");

    this->windows.insert(this->windows.begin(), window);

    // disable all other windows
    for(size_t i = 1; i < (int)(this->windows.size() - 1); i++) {
        this->windows[i]->setEnabled(false);
    }
}

void CWindowManager::onResolutionChange(vec2 newResolution) {
    for(auto &window : this->windows) {
        window->onResolutionChange(newResolution);
    }
}

void CWindowManager::setFocus(CBaseUIWindow *window) {
    if(window == nullptr) engine->showMessageError("Window Manager Error", "setFocus(NULL), you noodle!");

    for(int i = 0; i < this->windows.size(); i++) {
        if(this->windows[i] == window && i != 0) {
            CBaseUIWindow *newTopWindow = this->windows[i];
            for(int i2 = i - 1; i2 >= 0; i2--) {
                this->windows[i2 + 1] = this->windows[i2];
            }
            this->windows[0] = newTopWindow;
            break;
        }
    }
}

void CWindowManager::openAll() {
    for(auto &window : this->windows) {
        window->open();
    }
}

void CWindowManager::closeAll() {
    for(auto &window : this->windows) {
        window->close();
    }
}

bool CWindowManager::isMouseInside() {
    for(auto &window : this->windows) {
        if(window->isMouseInside()) return true;
    }
    return false;
}

bool CWindowManager::isVisible() {
    if(!this->bVisible) return false;

    for(auto &window : this->windows) {
        if(window->isVisible()) return true;
    }
    return false;
}

bool CWindowManager::isActive() {
    if(!this->bVisible) return false;
    for(auto &window : this->windows) {
        if(window->isActive() || window->isBusy())  // TODO: is this correct? (busy)
            return true;
    }
    return false;
}

void CWindowManager::onKeyDown(KeyboardEvent &e) {
    if(!this->bVisible) return;

    for(auto &window : this->windows) {
        window->onKeyDown(e);
    }
}

void CWindowManager::onKeyUp(KeyboardEvent &e) {
    if(!this->bVisible) return;

    for(auto &window : this->windows) {
        window->onKeyUp(e);
    }
}

void CWindowManager::onChar(KeyboardEvent &e) {
    if(!this->bVisible) return;

    for(auto &window : this->windows) {
        window->onChar(e);
    }
}

void CWindowManager::setEnabled(bool enabled) {
    this->bEnabled = enabled;
    if(!this->bEnabled) {
        for(auto &window : this->windows) {
            window->setEnabled(false);
        }
    } else
        this->windows[this->iCurrentEnabledWindow]->setEnabled(true);
}
