// Copyright (c) 2013, PG, All rights reserved.
#include "CBaseUIButton.h"

#include <utility>

#include "Engine.h"
#include "Mouse.h"
#include "ResourceManager.h"

CBaseUIButton::CBaseUIButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
    : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
    this->font = resourceManager->getFont("FONT_DEFAULT");

    // settings
    this->bDrawFrame = true;
    this->bDrawBackground = true;
    this->bTextLeft = false;

    // colors
    this->frameColor = argb(255, 255, 255, 255);
    this->backgroundColor = argb(255, 0, 0, 0);
    this->textColor = argb(255, 255, 255, 255);
    this->textBrightColor = this->textDarkColor = argb(0, 0, 0, 0);

    this->setText(std::move(text));
}

void CBaseUIButton::draw() {
    if(!this->isVisible() || !this->isVisibleOnScreen()) return;

    // draw background
    if(this->bDrawBackground) {
        g->setColor(this->backgroundColor);
        g->fillRect(this->vPos.x + 1, this->vPos.y + 1, this->vSize.x - 1, this->vSize.y - 1);
    }

    // draw frame
    if(this->bDrawFrame) {
        g->setColor(this->frameColor);
        g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
    }

    // draw hover rects
    const int hoverRectOffset = std::round(3.0f * ((float)this->font->getDPI() / 96.0f));  // NOTE: abusing font dpi
    g->setColor(this->frameColor);
    if(this->bMouseInside && this->bEnabled) {
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
    if(this->font == nullptr || !this->isVisible() || !this->isVisibleOnScreen() || this->sText.length() < 1) return;

    const int shadowOffset = std::round(1.0f * ((float)this->font->getDPI() / 96.0f));  // NOTE: abusing font dpi

    g->pushClipRect(McRect(this->vPos.x + 1, this->vPos.y + 1, this->vSize.x - 1, this->vSize.y - 1));
    {
        g->setColor(this->textColor);
        g->pushTransform();
        {
            g->translate((int)(this->vPos.x + this->vSize.x / 2.0f - this->fStringWidth / 2.0f),
                         (int)(this->vPos.y + this->vSize.y / 2.0f + this->fStringHeight / 2.0f));

            // shadow
            g->translate(shadowOffset, shadowOffset);
            {
                if(this->textDarkColor != 0)
                    g->setColor(this->textDarkColor);
                else
                    g->setColor(Colors::invert(this->textColor));
            }
            g->drawString(this->font, this->sText);

            // top
            g->translate(-shadowOffset, -shadowOffset);
            {
                if(this->textBrightColor != 0)
                    g->setColor(this->textBrightColor);
                else
                    g->setColor(this->textColor);
            }
            g->drawString(this->font, this->sText);
        }
        g->popTransform();
    }
    g->popClipRect();
}

void CBaseUIButton::drawHoverRect(int distance) {
    g->drawLine(this->vPos.x, this->vPos.y - distance, this->vPos.x + this->vSize.x + 1, this->vPos.y - distance);
    g->drawLine(this->vPos.x, this->vPos.y + this->vSize.y + distance, this->vPos.x + this->vSize.x + 1,
                this->vPos.y + this->vSize.y + distance);
    g->drawLine(this->vPos.x - distance, this->vPos.y, this->vPos.x - distance, this->vPos.y + this->vSize.y + 1);
    g->drawLine(this->vPos.x + this->vSize.x + distance, this->vPos.y, this->vPos.x + this->vSize.x + distance,
                this->vPos.y + this->vSize.y + 1);
}

void CBaseUIButton::onMouseUpInside(bool left, bool right) { this->onClicked(left, right); }

void CBaseUIButton::onClicked(bool left, bool right) {
    if(clickCallback) {
        clickCallback(this, left, right);
    }
}

void CBaseUIButton::updateStringMetrics() {
    if(this->font != nullptr) {
        this->fStringHeight = this->font->getHeight();

        if(this->bTextLeft)
            this->fStringWidth =
                this->vSize.x - 4;  // TODO: this is broken af, why is it like this? where is this even used/needed
        else
            this->fStringWidth = this->font->getStringWidth(this->sText);
    }
}
