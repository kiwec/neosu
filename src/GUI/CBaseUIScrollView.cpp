// Copyright (c) 2013, PG, All rights reserved.
// TODO: refactor the spaghetti parts, this can be done way more elegantly

#include "CBaseUIScrollView.h"

#include "AnimationHandler.h"
#include "CBaseUIContainer.h"
#include "ConVar.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Mouse.h"
// #include "ResourceManager.h"
// #include "Logging.h"

CBaseUIScrollView::CBaseUIScrollView(float xPos, float yPos, float xSize, float ySize, const UString &name)
    : CBaseUIElement(xPos, yPos, xSize, ySize, name) {
    this->grabs_clicks = true;

    this->iScrollResistance = cv::ui_scrollview_resistance.getInt();  // TODO: dpi handling

    this->container = new CBaseUIContainer(xPos, yPos, xSize, ySize, name);
}

CBaseUIScrollView::~CBaseUIScrollView() {
    this->freeElements();
    SAFE_DELETE(this->container);
}

void CBaseUIScrollView::freeElements() {
    this->container->freeElements();

    anim->deleteExistingAnimation(&this->vKineticAverage.x);
    anim->deleteExistingAnimation(&this->vKineticAverage.y);

    anim->deleteExistingAnimation(&this->vScrollPos.y);
    anim->deleteExistingAnimation(&this->vScrollPos.x);

    anim->deleteExistingAnimation(&this->vVelocity.x);
    anim->deleteExistingAnimation(&this->vVelocity.y);

    this->vScrollSize.x = this->vScrollSize.y = 0;
    this->vScrollPos.x = this->vScrollPos.y = 0;
    this->vVelocity.x = this->vVelocity.y = 0;

    this->container->setPos(this->vPos);  // TODO: wtf is this doing here
}

void CBaseUIScrollView::draw() {
    if(!this->bVisible) return;

    // draw background
    if(this->bDrawBackground) {
        g->setColor(this->backgroundColor);
        g->fillRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
    }

    // draw base frame
    if(this->bDrawFrame) {
        if(this->frameDarkColor != 0 || this->frameBrightColor != 0)
            g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y, this->frameDarkColor,
                        this->frameBrightColor, this->frameBrightColor, this->frameDarkColor);
        else {
            g->setColor(this->frameColor);
            g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
        }
    }

    // draw elements & scrollbars
    if(this->bHorizontalClipping || this->bVerticalClipping) {
        auto clip_rect =
            McRect(this->bHorizontalClipping ? this->vPos.x + 1 : 0, this->bVerticalClipping ? this->vPos.y + 2 : 0,
                   this->bHorizontalClipping ? this->vSize.x - 1 : engine->getScreenWidth(),
                   this->bVerticalClipping ? this->vSize.y - 1 : engine->getScreenHeight());
        g->pushClipRect(clip_rect);
    }

    this->container->draw();

    if(this->bDrawScrollbars) {
        // vertical
        if(this->bVerticalScrolling && this->vScrollSize.y > this->vSize.y) {
            g->setColor(this->scrollbarColor);
            if(((this->bScrollbarScrolling && this->bScrollbarIsVerticalScrolling) ||
                this->verticalScrollbar.contains(mouse->getPos())) &&
               !this->bScrolling)
                g->setAlpha(1.0f);

            g->fillRect(this->verticalScrollbar.getX(), this->verticalScrollbar.getY(),
                        this->verticalScrollbar.getWidth(), this->verticalScrollbar.getHeight());
            // g->fillRoundedRect(this->verticalScrollbar.getX(), this->verticalScrollbar.getY(),
            // m_verticalScrollbar.getWidth(), this->verticalScrollbar.getHeight(),
            // this->verticalScrollbar.getWidth()/2);
        }

        // horizontal
        if(this->bHorizontalScrolling && this->vScrollSize.x > this->vSize.x) {
            g->setColor(this->scrollbarColor);
            if(((this->bScrollbarScrolling && !this->bScrollbarIsVerticalScrolling) ||
                this->horizontalScrollbar.contains(mouse->getPos())) &&
               !this->bScrolling)
                g->setAlpha(1.0f);

            g->fillRect(this->horizontalScrollbar.getX(), this->horizontalScrollbar.getY(),
                        this->horizontalScrollbar.getWidth(), this->horizontalScrollbar.getHeight());
            // g->fillRoundedRect(this->horizontalScrollbar.getX(), this->horizontalScrollbar.getY(),
            // m_horizontalScrollbar.getWidth(), this->horizontalScrollbar.getHeight(),
            // m_horizontalScrollbar.getHeight()/2);
        }
    }

    if(this->bHorizontalClipping || this->bVerticalClipping) {
        g->popClipRect();
    }
}

void CBaseUIScrollView::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    this->container->mouse_update(propagate_clicks);
    CBaseUIElement::mouse_update(propagate_clicks);

    const bool wasContainerBusyBeforeUpdate = this->container->isBusy();
    if(this->bBusy) {
        const vec2 deltaToAdd = (mouse->getPos() - this->vMouseBackup2);
        // debugLog("+ ({})", deltaToAdd);

        anim->moveQuadOut(&this->vKineticAverage.x, deltaToAdd.x, cv::ui_scrollview_kinetic_approach_time.getFloat(),
                          true);
        anim->moveQuadOut(&this->vKineticAverage.y, deltaToAdd.y, cv::ui_scrollview_kinetic_approach_time.getFloat(),
                          true);

        this->vMouseBackup2 = mouse->getPos();
    }

    // scrolling logic
    if(this->bActive && !this->bBlockScrolling && (this->bVerticalScrolling || this->bHorizontalScrolling) &&
       this->bEnabled) {
        if(!this->bScrollResistanceCheck) {
            this->bScrollResistanceCheck = true;
            this->vMouseBackup3 = mouse->getPos();
        }

        // get pull strength
        int diff = std::abs(mouse->getPos().x - this->vMouseBackup3.x);
        if(std::abs(mouse->getPos().y - this->vMouseBackup3.y) > diff)
            diff = std::abs(mouse->getPos().y - this->vMouseBackup3.y);

        // if we are above our resistance, try to steal the focus and enable scrolling for us
        if(this->container->isActive() && diff > this->iScrollResistance && !this->container->isBusy())
            this->container->stealFocus();

        // handle scrollbar scrolling start
        if(this->verticalScrollbar.contains(mouse->getPos()) && !this->bScrollbarScrolling && !this->bScrolling) {
            // NOTE: scrollbar dragging always force steals focus
            if(!wasContainerBusyBeforeUpdate) {
                this->container->stealFocus();

                this->vMouseBackup.y = mouse->getPos().y - this->verticalScrollbar.getMaxY();
                this->bScrollbarScrolling = true;
                this->bScrollbarIsVerticalScrolling = true;
            }
        } else if(this->horizontalScrollbar.contains(mouse->getPos()) && !this->bScrollbarScrolling &&
                  !this->bScrolling) {
            // NOTE: scrollbar dragging always force steals focus
            if(!wasContainerBusyBeforeUpdate) {
                this->container->stealFocus();

                this->vMouseBackup.x = mouse->getPos().x - this->horizontalScrollbar.getMaxX();
                this->bScrollbarScrolling = true;
                this->bScrollbarIsVerticalScrolling = false;
            }
        } else if(!this->bScrolling && !this->bScrollbarScrolling && !this->container->isBusy() &&
                  !this->container->isActive()) {
            if(!this->container->isBusy()) {
                // if we have successfully stolen the focus or the container is no longer busy, start scrolling
                this->bScrollbarIsVerticalScrolling = false;

                this->vMouseBackup = mouse->getPos();
                this->vScrollPosBackup = this->vScrollPos + (mouse->getPos() - this->vMouseBackup3);
                this->bScrolling = true;
                this->bAutoScrollingX = false;
                this->bAutoScrollingY = false;

                anim->deleteExistingAnimation(&this->vScrollPos.x);
                anim->deleteExistingAnimation(&this->vScrollPos.y);

                anim->deleteExistingAnimation(&this->vVelocity.x);
                anim->deleteExistingAnimation(&this->vVelocity.y);
            }
        }
    } else if(this->bScrolling || this->bScrollbarScrolling)  // we were scrolling, stop it
    {
        this->bScrolling = false;
        this->bActive = false;

        vec2 delta = this->vKineticAverage;

        // calculate remaining kinetic energy
        if(!this->bScrollbarScrolling)
            this->vVelocity = cv::ui_scrollview_kinetic_energy_multiplier.getFloat() * delta *
                                  (engine->getFrameTime() != 0.0 ? 1.0 / engine->getFrameTime() : 60.0) / 60.0f +
                              this->vScrollPos;

        // debugLog("kinetic = ({}), velocity = ({:}), frametime = {:f}", delta, this->vVelocity, engine->getFrameTime());

        this->bScrollbarScrolling = false;
    } else
        this->bScrollResistanceCheck = false;

    // handle mouse wheel scrolling
    if(!keyboard->isAltDown() && this->bMouseInside && this->bEnabled) {
        if(mouse->getWheelDeltaVertical() != 0)
            this->scrollY(mouse->getWheelDeltaVertical() * this->fScrollMouseWheelMultiplier *
                          cv::ui_scrollview_mousewheel_multiplier.getFloat());
        if(mouse->getWheelDeltaHorizontal() != 0)
            this->scrollX(-mouse->getWheelDeltaHorizontal() * this->fScrollMouseWheelMultiplier *
                          cv::ui_scrollview_mousewheel_multiplier.getFloat());
    }

    // handle drag scrolling and rubber banding
    if(this->bScrolling && this->bActive) {
        if(this->bVerticalScrolling)
            this->vScrollPos.y = this->vScrollPosBackup.y + (mouse->getPos().y - this->vMouseBackup.y);
        if(this->bHorizontalScrolling)
            this->vScrollPos.x = this->vScrollPosBackup.x + (mouse->getPos().x - this->vMouseBackup.x);

        this->container->setPos(this->vPos + this->vScrollPos);
    } else  // no longer scrolling, smooth the remaining velocity
    {
        this->vKineticAverage = {0.f, 0.f};

        // rubber banding + kinetic scrolling

        // TODO: fix amount being dependent on fps due to double animation time indirection

        // y axis
        if(!this->bAutoScrollingY && this->bVerticalScrolling) {
            if(std::round(this->vScrollPos.y) > 1)  // rubber banding, top
            {
                // debugLog("y rubber banding, top");
                anim->moveQuadOut(&this->vVelocity.y, 1, 0.05f, 0.0f, true);
                anim->moveQuadOut(&this->vScrollPos.y, this->vVelocity.y, 0.2f, 0.0f, true);
            } else if(std::round(std::abs(this->vScrollPos.y) + this->vSize.y) > this->vScrollSize.y &&
                      std::round(this->vScrollPos.y) < 1)  // rubber banding, bottom
            {
                // debugLog("y rubber banding, bottom");
                anim->moveQuadOut(&this->vVelocity.y,
                                  (this->vScrollSize.y > this->vSize.y ? -this->vScrollSize.y : 1) +
                                      (this->vScrollSize.y > this->vSize.y ? this->vSize.y : 0),
                                  0.05f, 0.0f, true);

                anim->moveQuadOut(&this->vScrollPos.y, this->vVelocity.y, 0.2f, 0.0f, true);
            } else if(std::round(this->vVelocity.y) != 0 &&
                      std::round(this->vScrollPos.y) != std::round(this->vVelocity.y)) {  // kinetic scrolling
                // debugLog("y kinetic scrolling, velocity: {} scrollpos: {} thispos: {}", this->vVelocity.y, this->vScrollPos.y, this->vPos.y);
                anim->moveQuadOut(&this->vScrollPos.y, this->vVelocity.y, 0.35f, 0.0f, true);
            }
        }

        // x axis
        if(!this->bAutoScrollingX && this->bHorizontalScrolling) {
            if(std::round(this->vScrollPos.x) > 1)  // rubber banding, left
            {
                // debugLog("x rubber banding, left");
                anim->moveQuadOut(&this->vVelocity.x, 1, 0.05f, 0.0f, true);
                anim->moveQuadOut(&this->vScrollPos.x, this->vVelocity.x, 0.2f, 0.0f, true);
            } else if(std::round(std::abs(this->vScrollPos.x) + this->vSize.x) > this->vScrollSize.x &&
                      std::round(this->vScrollPos.x) < 1)  // rubber banding, right
            {
                // debugLog("x rubber banding, right");
                anim->moveQuadOut(&this->vVelocity.x,
                                  (this->vScrollSize.x > this->vSize.x ? -this->vScrollSize.x : 1) +
                                      (this->vScrollSize.x > this->vSize.x ? this->vSize.x : 0),
                                  0.05f, 0.0f, true);
                anim->moveQuadOut(&this->vScrollPos.x, this->vVelocity.x, 0.2f, 0.0f, true);
            } else if(std::round(this->vVelocity.x) != 0 &&
                      std::round(this->vScrollPos.x) != std::round(this->vVelocity.x)) {  // kinetic scrolling
                anim->moveQuadOut(&this->vScrollPos.x, this->vVelocity.x, 0.35f, 0.0f, true);
                // debugLog("x rubber banding, kinetic scrolling");
            }
        }
    }

    // handle scrollbar scrolling movement
    if(this->bScrollbarScrolling) {
        this->vVelocity.x = this->vVelocity.y = 0;
        if(this->bScrollbarIsVerticalScrolling) {
            // debugLog("scrollbar scrolling movement vertical");
            const float percent =
                std::clamp<float>((mouse->getPos().y - this->vPos.y - this->verticalScrollbar.getWidth() -
                                   this->verticalScrollbar.getHeight() - this->vMouseBackup.y - 1) /
                                      (this->vSize.y - 2 * this->verticalScrollbar.getWidth()),
                                  0.0f, 1.0f);
            this->scrollToYInt(-this->vScrollSize.y * percent, true, false);
        } else {
            // debugLog("scrollbar scrolling movement not vertical scrolling");

            const float percent =
                std::clamp<float>((mouse->getPos().x - this->vPos.x - this->horizontalScrollbar.getHeight() -
                                   this->horizontalScrollbar.getWidth() - this->vMouseBackup.x - 1) /
                                      (this->vSize.x - 2 * this->horizontalScrollbar.getHeight()),
                                  0.0f, 1.0f);
            this->scrollToXInt(-this->vScrollSize.x * percent, true, false);
        }
    }

    // position update during scrolling
    bool clippingDirty = (anim->isAnimating(&this->vScrollPos.y) || anim->isAnimating(&this->vScrollPos.x));
    if(clippingDirty) {
        // debugLog("hit first condition, frame: {}", engine->getFrameCount());
        this->container->setPos(this->vPos.x + std::round(this->vScrollPos.x),
                                this->vPos.y + std::round(this->vScrollPos.y));
    }

    // update scrollbars
    if((clippingDirty |= (this->bScrolling || this->bScrollbarScrolling))) {
        // debugLog("hit second condition, frame: {}", engine->getFrameCount());
        this->updateScrollbars();
    }

    // HACKHACK: if an animation was started and ended before any setpos could get fired, manually update the position
    if(this->container->getPos() !=
       (this->vPos + vec2(std::round(this->vScrollPos.x), std::round(this->vScrollPos.y)))) {
        this->container->setPos(this->vPos.x + std::round(this->vScrollPos.x),
                                this->vPos.y + std::round(this->vScrollPos.y));
        clippingDirty = true;
        // debugLog("hit third condition, frame: {}", engine->getFrameCount());
        this->updateScrollbars();
    }

    // only draw visible elements
    if(clippingDirty) this->updateClipping();
}

void CBaseUIScrollView::onKeyUp(KeyboardEvent &e) { this->container->onKeyUp(e); }

void CBaseUIScrollView::onKeyDown(KeyboardEvent &e) { this->container->onKeyDown(e); }

void CBaseUIScrollView::onChar(KeyboardEvent &e) { this->container->onChar(e); }

void CBaseUIScrollView::scrollY(int delta, bool animated) {
    if(!this->bVerticalScrolling || delta == 0 || this->bScrolling || this->vSize.y >= this->vScrollSize.y ||
       this->container->isBusy())
        return;

    const bool allowOverscrollBounce = cv::ui_scrollview_mousewheel_overscrollbounce.getBool();

    // keep velocity (partially animated/finished scrolls should not get lost, especially multiple scroll() calls in
    // quick succession)
    const float remainingVelocity = this->vScrollPos.y - this->vVelocity.y;
    if(animated && this->bAutoScrollingY) delta -= remainingVelocity;

    // calculate new target
    float target = this->vScrollPos.y + delta;
    this->bAutoScrollingY = animated;

    // clamp target
    {
        if(target > 1) {
            if(!allowOverscrollBounce) target = 1;

            this->bAutoScrollingY = !allowOverscrollBounce;
        }

        if(std::abs(target) + this->vSize.y > this->vScrollSize.y) {
            if(!allowOverscrollBounce)
                target = (this->vScrollSize.y > this->vSize.y ? -this->vScrollSize.y : this->vScrollSize.y) +
                         (this->vScrollSize.y > this->vSize.y ? this->vSize.y : 0);

            this->bAutoScrollingY = !allowOverscrollBounce;
        }
    }

    // TODO: fix very slow autoscroll when 1 scroll event goes to >= top or >= bottom
    // TODO: fix overscroll dampening user action when direction flips (while rubber banding)

    // apply target
    anim->deleteExistingAnimation(&this->vVelocity.y);
    if(animated) {
        anim->moveQuadOut(&this->vScrollPos.y, target, 0.15f, 0.0f, true);

        this->vVelocity.y = target;
    } else {
        anim->deleteExistingAnimation(&this->vScrollPos.y);

        this->vScrollPos.y = target;
        this->vVelocity.y = this->vScrollPos.y - remainingVelocity;
    }
}

void CBaseUIScrollView::scrollX(int delta, bool animated) {
    if(!this->bHorizontalScrolling || delta == 0 || this->bScrolling || this->vSize.x >= this->vScrollSize.x ||
       this->container->isBusy())
        return;

    // TODO: fix all of this shit with the code from scrollY() above

    // stop any movement
    if(animated) this->vVelocity.x = 0;

    // keep velocity
    if(this->bAutoScrollingX && animated)
        delta += (delta > 0 ? (this->iPrevScrollDeltaX < 0 ? 0 : std::abs(delta - this->iPrevScrollDeltaX))
                            : (this->iPrevScrollDeltaX > 0 ? 0 : -std::abs(delta - this->iPrevScrollDeltaX)));

    // calculate target respecting the boundaries
    float target = this->vScrollPos.x + delta;
    if(target > 1) target = 1;
    if(std::abs(target) + this->vSize.x > this->vScrollSize.x)
        target = (this->vScrollSize.x > this->vSize.x ? -this->vScrollSize.x : this->vScrollSize.x) +
                 (this->vScrollSize.x > this->vSize.x ? this->vSize.x : 0);

    this->bAutoScrollingX = animated;
    this->iPrevScrollDeltaX = delta;

    if(animated)
        anim->moveQuadOut(&this->vScrollPos.x, target, 0.15f, 0.0f, true);
    else {
        const float remainingVelocity = this->vScrollPos.x - this->vVelocity.x;

        this->vScrollPos.x += delta;
        this->vVelocity.x = this->vScrollPos.x - remainingVelocity;

        anim->deleteExistingAnimation(&this->vScrollPos.x);
    }
}

void CBaseUIScrollView::scrollToX(int scrollPosX, bool animated) { this->scrollToXInt(scrollPosX, animated); }

void CBaseUIScrollView::scrollToY(int scrollPosY, bool animated) { this->scrollToYInt(scrollPosY, animated); }

void CBaseUIScrollView::scrollToYInt(int scrollPosY, bool animated, bool slow) {
    if(!this->bVerticalScrolling || this->bScrolling) return;

    float upperBounds = 1;
    float lowerBounds = -this->vScrollSize.y + this->vSize.y;
    if(lowerBounds >= upperBounds) lowerBounds = upperBounds;

    const float targetY = std::clamp<float>(scrollPosY, lowerBounds, upperBounds);

    this->vVelocity.y = targetY;

    if(animated) {
        this->bAutoScrollingY = true;
        anim->moveQuadOut(&this->vScrollPos.y, targetY, (slow ? 0.15f : 0.035f), 0.0f, true);
    } else {
        anim->deleteExistingAnimation(&this->vVelocity.y);
        anim->deleteExistingAnimation(&this->vScrollPos.y);
        this->vScrollPos.y = targetY;
    }
}

void CBaseUIScrollView::scrollToXInt(int scrollPosX, bool animated, bool slow) {
    if(!this->bHorizontalScrolling || this->bScrolling) return;

    float upperBounds = 1;
    float lowerBounds = -this->vScrollSize.x + this->vSize.x;
    if(lowerBounds >= upperBounds) lowerBounds = upperBounds;

    const float targetX = std::clamp<float>(scrollPosX, lowerBounds, upperBounds);

    this->vVelocity.x = targetX;

    if(animated) {
        this->bAutoScrollingX = true;
        anim->moveQuadOut(&this->vScrollPos.x, targetX, (slow ? 0.15f : 0.035f), 0.0f, true);
    } else {
        anim->deleteExistingAnimation(&this->vScrollPos.x);
        this->vScrollPos.x = targetX;
    }
}

void CBaseUIScrollView::scrollToElement(CBaseUIElement *element, int /*xOffset*/, int yOffset, bool animated) {
    const std::vector<CBaseUIElement *> &elements = this->container->getElements();
    for(auto i : elements) {
        if(i == element) {
            this->scrollToY(-element->getRelPos().y + yOffset, animated);
            return;
        }
    }
}

void CBaseUIScrollView::updateClipping() {
    const std::vector<CBaseUIElement *> &elements = this->container->getElements();
    const McRect &me{this->getRect()};

    for(auto e : elements) {
        const McRect &eRect = e->getRect();  // heh
        const bool eVisible = e->isVisible();
        if(me.intersects(eRect)) {
            if(!eVisible) {
                e->setVisible(true);
            }
        } else if(eVisible) {
            e->setVisible(false);
        }
    }
}

void CBaseUIScrollView::updateScrollbars() {
    // update vertical scrollbar
    if(this->bVerticalScrolling && this->vScrollSize.y > this->vSize.y) {
        const float verticalBlockWidth = cv::ui_scrollview_scrollbarwidth.getInt();

        const float rawVerticalPercent = (this->vScrollPos.y > 0 ? -this->vScrollPos.y : std::abs(this->vScrollPos.y)) /
                                         (this->vScrollSize.y - this->vSize.y);
        float overscroll = 1.0f;
        if(rawVerticalPercent > 1.0f)
            overscroll = 1.0f - (rawVerticalPercent - 1.0f) * 0.95f;
        else if(rawVerticalPercent < 0.0f)
            overscroll = 1.0f - std::abs(rawVerticalPercent) * 0.95f;

        const float verticalPercent = std::clamp<float>(rawVerticalPercent, 0.0f, 1.0f);

        const float verticalHeightPercent = (this->vSize.y - (verticalBlockWidth * 2)) / this->vScrollSize.y;
        const float verticalBlockHeight =
            std::clamp<float>(std::max(verticalHeightPercent * this->vSize.y, verticalBlockWidth) * overscroll,
                              verticalBlockWidth, std::max(this->vSize.y, verticalBlockWidth));

        this->verticalScrollbar =
            McRect(this->vPos.x + this->vSize.x - (verticalBlockWidth * this->fScrollbarSizeMultiplier),
                   this->vPos.y + (verticalPercent * (this->vSize.y - (verticalBlockWidth * 2) - verticalBlockHeight) +
                                   verticalBlockWidth + 1),
                   (verticalBlockWidth * this->fScrollbarSizeMultiplier), verticalBlockHeight);
        if(this->bScrollbarOnLeft) {
            this->verticalScrollbar.setMinX(this->vPos.x);
            this->verticalScrollbar.setMaxX(verticalBlockWidth * this->fScrollbarSizeMultiplier);
        }
    }

    // update horizontal scrollbar
    if(this->bHorizontalScrolling && this->vScrollSize.x > this->vSize.x) {
        const float horizontalPercent =
            std::clamp<float>((this->vScrollPos.x > 0 ? -this->vScrollPos.x : std::abs(this->vScrollPos.x)) /
                                  (this->vScrollSize.x - this->vSize.x),
                              0.0f, 1.0f);
        const float horizontalBlockWidth = cv::ui_scrollview_scrollbarwidth.getInt();
        const float horizontalHeightPercent = (this->vSize.x - (horizontalBlockWidth * 2)) / this->vScrollSize.x;
        const float horizontalBlockHeight = std::max(horizontalHeightPercent * this->vSize.x, horizontalBlockWidth);

        this->horizontalScrollbar = McRect(
            this->vPos.x + (horizontalPercent * (this->vSize.x - (horizontalBlockWidth * 2) - horizontalBlockHeight) +
                            horizontalBlockWidth + 1),
            this->vPos.y + this->vSize.y - horizontalBlockWidth, horizontalBlockHeight, horizontalBlockWidth);
    }
}

CBaseUIScrollView *CBaseUIScrollView::setScrollSizeToContent(int border) {
    auto oldScrollPos = this->vScrollPos;
    bool wasAtBottom = (this->vSize.y - this->vScrollPos.y) >= this->vScrollSize.y;

    this->vScrollSize = {0.f, 0.f};

    const std::vector<CBaseUIElement *> &elements = this->container->getElements();
    for(auto e : elements) {
        const float x = e->getRelPos().x + e->getSize().x;
        const float y = e->getRelPos().y + e->getSize().y;

        if(x > this->vScrollSize.x) this->vScrollSize.x = x;
        if(y > this->vScrollSize.y) this->vScrollSize.y = y;
    }

    this->vScrollSize.x += border;
    this->vScrollSize.y += border;

    this->container->setSize(this->vScrollSize);

    // TODO: duplicate code, ref onResized(), but can't call onResized() due to possible endless recursion if
    // setScrollSizeToContent() within onResized() HACKHACK: shit code
    if(this->bVerticalScrolling && this->vScrollSize.y < this->vSize.y && this->vScrollPos.y != 1) this->scrollToY(1);
    if(this->bHorizontalScrolling && this->vScrollSize.x < this->vSize.x && this->vScrollPos.x != 1) this->scrollToX(1);

    this->updateScrollbars();

    if(wasAtBottom && this->sticky && !this->bFirstScrollSizeToContent) {
        // Scroll to bottom without animation
        // XXX: Correct way to do this would be to keep the animation, but then you have to correct
        //      the existing scrolling animation, AND the possible scroll bounce animation.
        auto target = std::max(oldScrollPos.y, this->vScrollSize.y);
        this->scrollToY(-target, false);
    }

    this->bFirstScrollSizeToContent = false;
    return this;
}

void CBaseUIScrollView::scrollToLeft() { this->scrollToX(0); }

void CBaseUIScrollView::scrollToRight() { this->scrollToX(-this->vScrollSize.x); }

void CBaseUIScrollView::scrollToBottom() { this->scrollToY(-this->vScrollSize.y); }

void CBaseUIScrollView::scrollToTop() { this->scrollToY(0); }

void CBaseUIScrollView::onMouseDownOutside(bool /*left*/, bool /*right*/) { this->container->stealFocus(); }

void CBaseUIScrollView::onMouseDownInside(bool /*left*/, bool /*right*/) {
    this->bBusy = true;
    this->vMouseBackup2 = mouse->getPos();  // to avoid spastic movement at scroll start
}

void CBaseUIScrollView::onMouseUpInside(bool /*left*/, bool /*right*/) { this->bBusy = false; }

void CBaseUIScrollView::onMouseUpOutside(bool /*left*/, bool /*right*/) { this->bBusy = false; }

void CBaseUIScrollView::onFocusStolen() {
    this->bActive = false;
    this->bScrolling = false;
    this->bScrollbarScrolling = false;
    this->bBusy = false;

    // forward focus steal to container
    this->container->stealFocus();
}

void CBaseUIScrollView::onEnabled() { this->container->setEnabled(true); }

void CBaseUIScrollView::onDisabled() {
    this->bActive = false;
    this->bScrolling = false;
    this->bScrollbarScrolling = false;
    this->bBusy = false;

    this->container->setEnabled(false);
}

void CBaseUIScrollView::onResized() {
    this->container->setSize(this->vScrollSize);

    // TODO: duplicate code
    // HACKHACK: shit code
    if(this->bVerticalScrolling && this->vScrollSize.y < this->vSize.y && this->vScrollPos.y != 1) this->scrollToY(1);
    if(this->bHorizontalScrolling && this->vScrollSize.x < this->vSize.x && this->vScrollPos.x != 1) this->scrollToX(1);

    this->updateScrollbars();
}

void CBaseUIScrollView::onMoved() {
    this->container->setPos(this->vPos + this->vScrollPos);

    this->vMouseBackup2 = mouse->getPos();  // to avoid spastic movement after we are moved

    this->updateScrollbars();
}

bool CBaseUIScrollView::isBusy() {
    return (this->container->isBusy() || this->bScrolling || this->bBusy) && this->bVisible;
}
