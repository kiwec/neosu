// Copyright (c) 2014, PG, All rights reserved.
#include "CBaseUILabel.h"

#include <utility>

#include "Engine.h"
#include "ResourceManager.h"

CBaseUILabel::CBaseUILabel(float xPos, float yPos, float xSize, float ySize, UString name, const UString& text)
    : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
    this->font = resourceManager->getFont("FONT_DEFAULT");
    this->setText(text);
}

void CBaseUILabel::draw() {
    if(!this->bVisible) return;

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

    // draw text
    this->drawText();
}

void CBaseUILabel::drawText() {
    if(this->font != nullptr && this->sText.length() > 0) {
        float xPosAdd = 0;
        switch(this->textJustification) {
            case TEXT_JUSTIFICATION_LEFT:
                break;
            case TEXT_JUSTIFICATION_CENTERED:
                xPosAdd = this->vSize.x / 2.0f - this->fStringWidth / 2.0f;
                break;
            case TEXT_JUSTIFICATION_RIGHT:
                xPosAdd = this->vSize.x - this->fStringWidth;
                break;
        }

        // g->pushClipRect(McRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y));

        g->setColor(this->textColor);
        g->pushTransform();
        {
            g->scale(this->fScale, this->fScale);  // XXX: not sure if scaling respects text justification
            g->translate(
                (int)(this->vPos.x + (this->bCenterText ? +this->vSize.x / 2.0f - this->fStringWidth / 2.0f : xPosAdd)),
                (int)(this->vPos.y + this->vSize.y / 2.0f + this->fStringHeight / 2.0f));
            g->drawString(this->font, this->sText);
        }
        g->popTransform();

        // g->popClipRect();
    }
}

void CBaseUILabel::mouse_update(bool* propagate_clicks) { CBaseUIElement::mouse_update(propagate_clicks); }

void CBaseUILabel::updateStringMetrics() {
    if(this->font != nullptr) {
        this->fStringWidth = this->font->getStringWidth(this->sText);
        this->fStringHeight = this->font->getHeight();
    }
}
