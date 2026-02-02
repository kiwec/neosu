// Copyright (c) 2013, PG, All rights reserved.
#include "CBaseUIButton.h"

#include <utility>

#include "Engine.h"
#include "Graphics.h"
#include "Mouse.h"
#include "ResourceManager.h"
#include "Font.h"

CBaseUIButton::CBaseUIButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
    : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
    // this->grabs_clicks = true; // shouldn't this be set?

    this->font = engine->getDefaultFont();

    // settings
    this->bDrawFrame = true;
    this->bDrawBackground = true;

    // colors
    this->frameColor = argb(255, 255, 255, 255);
    this->backgroundColor = argb(255, 0, 0, 0);
    this->textColor = argb(255, 255, 255, 255);
    this->textBrightColor = this->textDarkColor = argb(0, 0, 0, 0);

    if(!text.isEmpty()) {
        this->setText(std::move(text));
    }
}

void CBaseUIButton::draw() {
    if(!this->isVisible() || !this->isVisibleOnScreen()) return;

    // draw background
    if(this->bDrawBackground) {
        g->setColor(this->backgroundColor);
        g->fillRect(this->getPos().x + 1, this->getPos().y + 1, this->getSize().x - 1, this->getSize().y - 1);
    }

    // draw frame
    if(this->bDrawFrame) {
        g->setColor(this->frameColor);
        g->drawRect(this->getPos(), this->getSize());
    }

    // draw hover rects
    const int hoverRectOffset = std::round(3.f * ((float)this->font->getDPI() / 96.f));  // NOTE: abusing font dpi
    g->setColor(this->frameColor);
    if(this->bEnabled && this->isMouseInside()) {
        if(!this->bActive && !mouse->isLeftDown())
            this->drawHoverRect(hoverRectOffset);
        else if(this->bActive)
            this->drawHoverRect(hoverRectOffset);
    }
    if(this->bActive && this->bEnabled) this->drawHoverRect(hoverRectOffset * 2);

    // draw text
    this->drawText();
}

void CBaseUIButton::drawText() {
    if(this->font == nullptr || !this->isVisible() || !this->isVisibleOnScreen() || this->getText().length() < 1)
        return;

    const int shadowOffset = std::round(1.f * ((float)this->font->getDPI() / 96.f));  // NOTE: abusing font dpi

    // NOTE: don't decrease the size of this clip rect, console box suggestion text gets cut off at the bottom
    g->pushClipRect(McRect(this->getPos().x + 1, this->getPos().y + 1, this->getSize().x - 1, this->getSize().y));
    {
        g->setColor(this->textColor);
        g->pushTransform();
        {
            // Justification logic pasted from CBaseUILabel::drawText()
            f32 xPosAdd = 0;
            switch(this->textJustification) {
                case TEXT_JUSTIFICATION::LEFT:
                    break;
                case TEXT_JUSTIFICATION::CENTERED:
                    xPosAdd = this->getSize().x / 2.0f - this->fStringWidth / 2.0f;
                    break;
                case TEXT_JUSTIFICATION::RIGHT:
                    xPosAdd = this->getSize().x - this->fStringWidth;
                    break;
            }

            g->translate((i32)(this->getPos().x + xPosAdd),
                         (i32)(this->getPos().y + this->getSize().y / 2.f + this->fStringHeight / 2.f));

            // shadow
            if(this->bDrawShadow) {
                g->translate(shadowOffset, shadowOffset);
                g->setColor(this->textDarkColor ? this->textDarkColor : Colors::invert(this->textColor));
                g->drawString(this->font, this->getText());
                g->translate(-shadowOffset, -shadowOffset);
            }

            // top
            g->setColor(this->textBrightColor ? this->textBrightColor : this->textColor);
            g->drawString(this->font, this->getText());
        }
        g->popTransform();
    }
    g->popClipRect();
}

void CBaseUIButton::drawHoverRect(int distance) {
    g->drawLine(this->getPos().x, this->getPos().y - distance, this->getPos().x + this->getSize().x + 1,
                this->getPos().y - distance);
    g->drawLine(this->getPos().x, this->getPos().y + this->getSize().y + distance,
                this->getPos().x + this->getSize().x + 1, this->getPos().y + this->getSize().y + distance);
    g->drawLine(this->getPos().x - distance, this->getPos().y, this->getPos().x - distance,
                this->getPos().y + this->getSize().y + 1);
    g->drawLine(this->getPos().x + this->getSize().x + distance, this->getPos().y,
                this->getPos().x + this->getSize().x + distance, this->getPos().y + this->getSize().y + 1);
}

void CBaseUIButton::onMouseUpInside(bool left, bool right) { this->onClicked(left, right); }

void CBaseUIButton::onClicked(bool left, bool right) {
    if(this->clickCallback && (*this->clickCallback)) {
        (*this->clickCallback)(this, left, right);
    }
}

void CBaseUIButton::updateStringMetrics() {
    if(this->font == nullptr) return;

    this->fStringHeight = this->font->getHeight();
    this->fStringWidth = this->font->getStringWidth(this->getText());
}
