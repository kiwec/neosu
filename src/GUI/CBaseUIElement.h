#pragma once

#include <utility>

#include "KeyboardListener.h"
#include "cbase.h"
#include <bitset>

// Guidelines for avoiding hair pulling:
// - Don't use m_vmSize
// - When an element is standalone, use getPos/setPos
// - In a container or in a scrollview, use getRelPos/setRelPos and call update_pos() on the container

class CBaseUIElement : public KeyboardListener {
   public:
    CBaseUIElement(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "")
        : sName(std::move(name)),
          rect(xPos, yPos, xSize, ySize),
          relRect(this->rect),
          vPos(const_cast<Vector2 &>(this->rect.getPos())),
          vSize(const_cast<Vector2 &>(this->rect.getSize())),
          vmPos(const_cast<Vector2 &>(this->relRect.getPos())),
          vmSize(const_cast<Vector2 &>(this->relRect.getSize())) {}
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

    [[nodiscard]] constexpr const Vector2 &getPos() const { return this->vPos; }
    [[nodiscard]] constexpr const Vector2 &getSize() const { return this->vSize; }

    [[nodiscard]] constexpr const McRect &getRelRect() const { return this->relRect; }

    [[nodiscard]] constexpr const Vector2 &getRelPos() const { return this->vmPos; }
    [[nodiscard]] constexpr const Vector2 &getRelSize() const { return this->vmSize; }

    virtual bool isActive() { return this->bActive || this->isBusy(); }
    virtual bool isVisible() { return this->bVisible; }

    // engine rectangle contains rect
    static bool isVisibleOnScreen(const McRect &rect);
    [[nodiscard]] static constexpr forceinline bool isVisibleOnScreen(CBaseUIElement *elem) {
        return CBaseUIElement::isVisibleOnScreen(elem->getRect());
    }

    [[nodiscard]] constexpr forceinline bool isVisibleOnScreen() const {
        return CBaseUIElement::isVisibleOnScreen(this->getRect());
    }

    virtual bool isEnabled() { return this->bEnabled; }
    virtual bool isBusy() { return this->bBusy && this->isVisible(); }
    virtual bool isMouseInside() { return this->bMouseInside && this->isVisible(); }

    virtual CBaseUIElement *setPos(float xPos, float yPos) {
        Vector2 newPos{xPos, yPos};
        if(newPos != this->vPos) {
            this->vPos = newPos;
            this->onMoved();
        }
        return this;
    }
    virtual CBaseUIElement *setPosX(float xPos) {
        if(this->vPos.x != xPos) {
            this->vPos.x = xPos;
            this->onMoved();
        }
        return this;
    }
    virtual CBaseUIElement *setPosY(float yPos) {
        if(this->vPos.y != yPos) {
            this->vPos.y = yPos;
            this->onMoved();
        }
        return this;
    }
    virtual CBaseUIElement *setPos(Vector2 position) { return this->setPos(position.x, position.y); }

    virtual CBaseUIElement *setRelPos(float xPos, float yPos) {
        this->vmPos.x = xPos;
        this->vmPos.y = yPos;
        return this;
    }
    virtual CBaseUIElement *setRelPosX(float xPos) {
        this->vmPos.x = xPos;
        return this;
    }
    virtual CBaseUIElement *setRelPosY(float yPos) {
        this->vmPos.y = yPos;
        return this;
    }
    virtual CBaseUIElement *setRelPos(Vector2 position) { return this->setRelPos(position.x, position.y); }

    virtual CBaseUIElement *setSize(float xSize, float ySize) {
        Vector2 newSize{xSize, ySize};
        if(newSize != this->vSize) {
            this->vSize = newSize;
            this->onResized();
            this->onMoved();
        }
        return this;
    }
    virtual CBaseUIElement *setSizeX(float xSize) {
        if(this->vSize.x != xSize) {
            this->vSize.x = xSize;
            this->onResized();
            this->onMoved();
        }
        return this;
    }
    virtual CBaseUIElement *setSizeY(float ySize) {
        if(this->vSize.y != ySize) {
            this->vSize.y = ySize;
            this->onResized();
            this->onMoved();
        }
        return this;
    }
    virtual CBaseUIElement *setSize(Vector2 size) { return this->setSize(size.x, size.y); }

    virtual CBaseUIElement *setRect(McRect rect) {
        this->rect = rect;
        return this;
    }

    virtual CBaseUIElement *setRelRect(McRect rect) {
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
    virtual CBaseUIElement *setEnabled(bool enabled, const char *reason = NULL) {
        if(enabled != this->bEnabled) {
            this->bEnabled = enabled;
            if(this->bEnabled) {
                this->onEnabled();
            } else {
                this->disabled_reason = reason;
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
    void stealFocus();

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

    Vector2 &vPos;    // reference to rect.vMin
    Vector2 &vSize;   // reference to rect.vSize
    Vector2 &vmPos;   // reference to relRect.vMin
    Vector2 &vmSize;  // reference to relRect.vSize

    const char *disabled_reason = NULL;

   private:
    std::bitset<2> mouseInsideCheck{0};
    std::bitset<2> mouseUpCheck{0};
};
