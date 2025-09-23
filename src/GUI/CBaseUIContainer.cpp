// Copyright (c) 2011, PG, All rights reserved.
#include "CBaseUIContainer.h"

#include <utility>

#include "Engine.h"
#include "Logging.h"

CBaseUIContainer::CBaseUIContainer(float Xpos, float Ypos, float Xsize, float Ysize, UString name)
    : CBaseUIElement(Xpos, Ypos, Xsize, Ysize, std::move(name)) {}

CBaseUIContainer::~CBaseUIContainer() { this->freeElements(); }

// free memory from children
void CBaseUIContainer::freeElements() {
    for(size_t i = 0; i < this->vElements.size(); i++) {
        delete this->vElements[i];
    }
    this->vElements.clear();
}

// invalidate children without freeing memory
void CBaseUIContainer::invalidate() { this->vElements.clear(); }

CBaseUIContainer *CBaseUIContainer::addBaseUIElement(CBaseUIElement *element, float xPos, float yPos) {
    if(element == nullptr) return this;

    element->setRelPos(xPos, yPos);
    element->setPos(this->vPos + element->getRelPos());
    this->vElements.push_back(element);

    return this;
}

CBaseUIContainer *CBaseUIContainer::addBaseUIElement(CBaseUIElement *element) {
    if(element == nullptr) return this;

    element->setRelPos(element->getPos().x, element->getPos().y);
    element->setPos(this->vPos + element->getRelPos());
    this->vElements.push_back(element);

    return this;
}

CBaseUIContainer *CBaseUIContainer::addBaseUIElementBack(CBaseUIElement *element, float xPos, float yPos) {
    if(element == nullptr) return this;

    element->setRelPos(xPos, yPos);
    element->setPos(this->vPos + element->getRelPos());
    this->vElements.insert(this->vElements.begin(), element);

    return this;
}

CBaseUIContainer *CBaseUIContainer::addBaseUIElementBack(CBaseUIElement *element) {
    if(element == nullptr) return this;

    element->setRelPos(element->getPos().x, element->getPos().y);
    element->setPos(this->vPos + element->getRelPos());
    this->vElements.insert(this->vElements.begin(), element);

    return this;
}

CBaseUIContainer *CBaseUIContainer::insertBaseUIElement(CBaseUIElement *element, CBaseUIElement *index) {
    if(element == nullptr || index == nullptr) return this;

    element->setRelPos(element->getPos().x, element->getPos().y);
    element->setPos(this->vPos + element->getRelPos());
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i] == index) {
            this->vElements.insert(this->vElements.begin() + std::clamp<int>(i, 0, this->vElements.size()), element);
            return this;
        }
    }

    debugLog("Warning: CBaseUIContainer::insertBaseUIElement() couldn't find index");

    return this;
}

CBaseUIContainer *CBaseUIContainer::insertBaseUIElementBack(CBaseUIElement *element, CBaseUIElement *index) {
    if(element == nullptr || index == nullptr) return this;

    element->setRelPos(element->getPos().x, element->getPos().y);
    element->setPos(this->vPos + element->getRelPos());
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i] == index) {
            this->vElements.insert(this->vElements.begin() + std::clamp<int>(i + 1, 0, this->vElements.size()),
                                   element);
            return this;
        }
    }

    debugLog("Warning: CBaseUIContainer::insertBaseUIElementBack() couldn't find index");

    return this;
}

CBaseUIContainer *CBaseUIContainer::removeBaseUIElement(CBaseUIElement *element) {
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i] == element) {
            this->vElements.erase(this->vElements.begin() + i);
            return this;
        }
    }

    debugLog("Warning: CBaseUIContainer::removeBaseUIElement() couldn't find element");

    return this;
}

CBaseUIContainer *CBaseUIContainer::deleteBaseUIElement(CBaseUIElement *element) {
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i] == element) {
            delete element;
            this->vElements.erase(this->vElements.begin() + i);
            return this;
        }
    }

    debugLog("Warning: CBaseUIContainer::deleteBaseUIElement() couldn't find element");

    return this;
}

CBaseUIElement *CBaseUIContainer::getBaseUIElement(const UString &name) {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i]->getName() == name) return this->vElements[i];
    }
    debugLog("CBaseUIContainer ERROR: GetBaseUIElement() \"{:s}\" does not exist!!!", name.toUtf8());
    return nullptr;
}

void CBaseUIContainer::draw() {
    if(!this->bVisible) return;

    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        const auto &e = this->vElements[i];
        if(e->isVisible()) {
            e->draw();
        }
    }
}

void CBaseUIContainer::draw_debug() {
    g->setColor(0xffffffff);
    g->drawLine(this->vPos.x, this->vPos.y, this->vPos.x + this->vSize.x, this->vPos.y);
    g->drawLine(this->vPos.x, this->vPos.y, this->vPos.x, this->vPos.y + this->vSize.y);
    g->drawLine(this->vPos.x, this->vPos.y + this->vSize.y, this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y);
    g->drawLine(this->vPos.x + this->vSize.x, this->vPos.y, this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y);

    g->setColor(0xff0000ff);
    for(size_t i = 0; i < this->vElements.size(); i++) {
        g->drawLine(this->vElements[i]->vPos.x, this->vElements[i]->vPos.y,
                    this->vElements[i]->vPos.x + this->vElements[i]->vSize.x, this->vElements[i]->vPos.y);
        g->drawLine(this->vElements[i]->vPos.x, this->vElements[i]->vPos.y, this->vElements[i]->vPos.x,
                    this->vElements[i]->vPos.y + this->vElements[i]->vSize.y);
        g->drawLine(this->vElements[i]->vPos.x, this->vElements[i]->vPos.y + this->vElements[i]->vSize.y,
                    this->vElements[i]->vPos.x + this->vElements[i]->vSize.x,
                    this->vElements[i]->vPos.y + this->vElements[i]->vSize.y);
        g->drawLine(this->vElements[i]->vPos.x + this->vElements[i]->vSize.x, this->vElements[i]->vPos.y,
                    this->vElements[i]->vPos.x + this->vElements[i]->vSize.x,
                    this->vElements[i]->vPos.y + this->vElements[i]->vSize.y);
    }
}

void CBaseUIContainer::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUIElement::mouse_update(propagate_clicks);

    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        this->vElements[i]->mouse_update(propagate_clicks);
    }
}

void CBaseUIContainer::update_pos() {
    const auto &thisPos = this->vPos;
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        const auto &e = this->vElements[i];
        e->setPos(thisPos + e->getRelPos());
    }
}

void CBaseUIContainer::onKeyUp(KeyboardEvent &e) {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i]->isVisible()) this->vElements[i]->onKeyUp(e);
    }
}
void CBaseUIContainer::onKeyDown(KeyboardEvent &e) {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i]->isVisible()) this->vElements[i]->onKeyDown(e);
    }
}

void CBaseUIContainer::onChar(KeyboardEvent &e) {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i]->isVisible()) this->vElements[i]->onChar(e);
    }
}

void CBaseUIContainer::onFocusStolen() {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        this->vElements[i]->stealFocus();
    }
}

void CBaseUIContainer::onEnabled() {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        this->vElements[i]->setEnabled(true);
    }
}

void CBaseUIContainer::onDisabled() {
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        this->vElements[i]->setEnabled(false);
    }
}

void CBaseUIContainer::onMouseDownOutside(bool /*left*/, bool /*right*/) { this->onFocusStolen(); }

bool CBaseUIContainer::isBusy() {
    if(!this->bVisible) return false;
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i]->isBusy()) return true;
    }

    return false;
}

bool CBaseUIContainer::isActive() {
    if(!this->bVisible) return false;
    MC_UNROLL
    for(size_t i = 0; i < this->vElements.size(); i++) {
        if(this->vElements[i]->isActive()) return true;
    }

    return false;
}
