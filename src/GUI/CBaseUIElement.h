#pragma once
// Copyright (c) 2013, PG, All rights reserved.

#include <utility>

#include "KeyboardListener.h"
#include "cbase.h"

// Guidelines for avoiding hair pulling:
// - Don't use m_vmSize
// - When an element is standalone, use getPos/setPos
// - In a container or in a scrollview, use getRelPos/setRelPos and call update_pos() on the container

enum class TEXT_JUSTIFICATION : uint8_t { LEFT, CENTERED, RIGHT };

class CBaseUIElement : public KeyboardListener {
    NOCOPY_NOMOVE(CBaseUIElement)
   public:
    CBaseUIElement(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "")
        : sName(std::move(name)),
          rect(xPos, yPos, xSize, ySize),
          relRect(this->rect),
          vPos(const_cast<vec2 &>(this->rect.getPos())),
          vSize(const_cast<vec2 &>(this->rect.getSize())),
          vmPos(const_cast<vec2 &>(this->relRect.getPos())),
          vmSize(const_cast<vec2 &>(this->relRect.getSize())) {}
    ~CBaseUIElement() override { ; }

    // main
    virtual void draw() = 0;
    virtual void mouse_update(bool *propagate_clicks);
    bool grabs_clicks = false;

    // keyboard input
    void onKeyUp(KeyboardEvent &e) override { (void)e; }
    void onKeyDown(KeyboardEvent &e) override { (void)e; }
    void onChar(KeyboardEvent &e) override { (void)e; }

    // getters
    [[nodiscard]] constexpr const UString &getName() const { return this->sName; }

    [[nodiscard]] constexpr const McRect &getRect() const { return this->rect; }

    [[nodiscard]] constexpr const vec2 &getPos() const { return this->vPos; }
    [[nodiscard]] constexpr const vec2 &getSize() const { return this->vSize; }

    [[nodiscard]] constexpr const McRect &getRelRect() const { return this->relRect; }

    [[nodiscard]] constexpr const vec2 &getRelPos() const { return this->vmPos; }
    [[nodiscard]] constexpr const vec2 &getRelSize() const { return this->vmSize; }

    virtual bool isActive() { return this->bActive || this->isBusy(); }
    virtual bool isVisible() { return this->bVisible; }

    // engine rectangle contains rect
    static bool isVisibleOnScreen(const McRect &rect);
    [[nodiscard]] static forceinline bool isVisibleOnScreen(CBaseUIElement *elem) {
        return CBaseUIElement::isVisibleOnScreen(elem->getRect());
    }

    [[nodiscard]] forceinline bool isVisibleOnScreen() const {
        return CBaseUIElement::isVisibleOnScreen(this->getRect());
    }

    virtual bool isEnabled() { return this->bEnabled; }
    virtual bool isBusy() { return this->bBusy && this->isVisible(); }
    virtual bool isMouseInside() { return this->bMouseInside && this->isVisible(); }

    inline CBaseUIElement *setPos(vec2 newPos) {
        if(newPos != this->vPos) {
            this->vPos = newPos;
            this->onMoved();
        }
        return this;
    }
    inline CBaseUIElement *setPos(float xPos, float yPos) { return this->setPos({xPos, yPos}); }
    inline CBaseUIElement *setPosX(float xPos) { return this->setPos({xPos, this->vPos.y}); }
    inline CBaseUIElement *setPosY(float yPos) { return this->setPos({this->vPos.x, yPos}); }
    inline CBaseUIElement *setRelPos(vec2 newRelPos) {
        this->vmPos = newRelPos;
        return this;
    }
    inline CBaseUIElement *setRelPos(float xPos, float yPos) { return this->setRelPos({xPos, yPos}); }
    inline CBaseUIElement *setRelPosX(float xPos) {
        this->vmPos.x = xPos;
        return this;
    }
    inline CBaseUIElement *setRelPosY(float yPos) {
        this->vmPos.y = yPos;
        return this;
    }

    inline CBaseUIElement *setSize(vec2 newSize) {
        if(newSize != this->vSize) {
            this->vSize = newSize;
            this->onResized();
            this->onMoved();
        }
        return this;
    }
    inline CBaseUIElement *setSize(float xSize, float ySize) { return this->setSize({xSize, ySize}); }
    inline CBaseUIElement *setSizeX(float xSize) { return this->setSize({xSize, this->vSize.y}); }
    inline CBaseUIElement *setSizeY(float ySize) { return this->setSize({this->vSize.x, ySize}); }

    inline CBaseUIElement *setRect(McRect rect) {
        this->rect = rect;
        return this;
    }

    inline CBaseUIElement *setRelRect(McRect rect) {
        this->relRect = rect;
        return this;
    }

    virtual CBaseUIElement *setVisible(bool visible) {
        this->bVisible = visible;
        return this;
    }
    virtual CBaseUIElement *setActive(bool active) {
        this->bActive = active;
        return this;
    }
    virtual CBaseUIElement *setKeepActive(bool keepActive) {
        this->bKeepActive = keepActive;
        return this;
    }
    virtual CBaseUIElement *setEnabled(bool enabled) {
        if(enabled != this->bEnabled) {
            this->bEnabled = enabled;
            if(this->bEnabled) {
                this->onEnabled();
            } else {
                this->onDisabled();
            }
        }
        return this;
    }
    virtual CBaseUIElement *setBusy(bool busy) {
        this->bBusy = busy;
        return this;
    }
    virtual CBaseUIElement *setName(UString name) {
        this->sName = std::move(name);
        return this;
    }
    virtual CBaseUIElement *setHandleLeftMouse(bool handle) {
        this->bHandleLeftMouse = handle;
        return this;
    }
    virtual CBaseUIElement *setHandleRightMouse(bool handle) {
        this->bHandleRightMouse = handle;
        return this;
    }
    // actions
    virtual void stealFocus();

   protected:
    // events
    virtual void onResized() { ; }
    virtual void onMoved() { ; }

    virtual void onFocusStolen() { ; }
    virtual void onEnabled() { ; }
    virtual void onDisabled() { ; }

    virtual void onMouseInside() { ; }
    virtual void onMouseOutside() { ; }
    virtual void onMouseDownInside(bool /*left*/ = true, bool /*right*/ = false) { ; }
    virtual void onMouseDownOutside(bool /*left*/ = true, bool /*right*/ = false) { ; }
    virtual void onMouseUpInside(bool /*left*/ = true, bool /*right*/ = false) { ; }
    virtual void onMouseUpOutside(bool /*left*/ = true, bool /*right*/ = false) { ; }

    // vars
    UString sName;

    // attributes
    bool bVisible = true;
    bool bActive = false;  // we are doing something, e.g. textbox is blinking and ready to receive input
    bool bBusy = false;    // we demand the focus to be kept on us, e.g. click-drag scrolling in a scrollview
    bool bEnabled = true;

    bool bKeepActive = false;  // once clicked, don't lose m_bActive, we have to manually release it (e.g. textbox)
    bool bMouseInside = false;

    bool bHandleLeftMouse = true;
    bool bHandleRightMouse = false;

    // position and size
    McRect rect;
    McRect relRect;

    vec2 &vPos;    // reference to rect.vMin
    vec2 &vSize;   // reference to rect.vSize
    vec2 &vmPos;   // reference to relRect.vMin
    vec2 &vmSize;  // reference to relRect.vSize

   private:
    void dumpElem() const;  // debug
    u8 mouseInsideCheck{0};
    u8 mouseUpCheck{0};
};
