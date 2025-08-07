// Copyright (c) 2014, PG, All rights reserved.
#include "CBaseUICheckbox.h"

#include <utility>

#include "Engine.h"
#include "Font.h"

CBaseUICheckbox::CBaseUICheckbox(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
    this->bChecked = false;
    this->changeCallback = {};

    CBaseUIButton::setClickCallback(SA::MakeDelegate<&CBaseUICheckbox::onPressed>(this));
}

void CBaseUICheckbox::draw() {
    if(!this->bVisible) return;

    const float dpiScale = ((float)this->font->getDPI() / 96.0f);  // NOTE: abusing font dpi

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
    const int hoverRectOffset = std::round(3.0f * dpiScale);
    g->setColor(this->frameColor);
    if(this->bMouseInside && this->bEnabled) this->drawHoverRect(hoverRectOffset);
    if(this->bActive) this->drawHoverRect(hoverRectOffset * 2);

    // draw block
    const int innerBlockPosOffset = std::round(2.0f * dpiScale);
    const int blockBorder = std::round(this->getBlockBorder());
    const int blockSize = std::round(this->getBlockSize());
    const int innerBlockSizeOffset = 2 * innerBlockPosOffset - 1;
    g->drawRect(this->vPos.x + blockBorder, this->vPos.y + blockBorder, blockSize, blockSize);
    if(this->bChecked)
        g->fillRect(this->vPos.x + blockBorder + innerBlockPosOffset, this->vPos.y + blockBorder + innerBlockPosOffset,
                    blockSize - innerBlockSizeOffset, blockSize - innerBlockSizeOffset);

    // draw text
    const int shadowOffset = std::round(1.0f * dpiScale);
    if(this->font != NULL && this->sText.length() > 0) {
        // g->pushClipRect(McRect(this->vPos.x + 1, this->vPos.y + 1, this->vSize.x - 1, this->vSize.y - 1));

        g->setColor(this->textColor);
        g->pushTransform();
        {
            g->translate((int)(this->vPos.x + this->getBlockBorder() * 2 + this->getBlockSize()),
                         (int)(this->vPos.y + this->vSize.y / 2.0f + this->fStringHeight / 2.0f));
            /// g->translate(this->vPos.x + (this->vSize.x - blockBorder*2 - blockSize)/2.0f - m_fStringWidth/2.0f +
            /// blockBorder*2 + blockSize, this->vPos.y + m_vSize.y/2.0f + m_fStringHeight/2.0f);

            g->translate(shadowOffset, shadowOffset);
            g->setColor(0xff212121);
            g->drawString(this->font, this->sText);

            g->translate(-shadowOffset, -shadowOffset);
            g->setColor(this->textColor);
            g->drawString(this->font, this->sText);
        }
        g->popTransform();

        // g->popClipRect();
    }
}

CBaseUICheckbox *CBaseUICheckbox::setChecked(bool checked, bool fireChangeEvent) {
    if(this->bChecked != checked) {
        if(fireChangeEvent)
            this->onPressed();
        else
            this->bChecked = checked;
    }

    return this;
}

void CBaseUICheckbox::onPressed() {
    this->bChecked = !this->bChecked;
    if(this->changeCallback != NULL) this->changeCallback(this);
}

CBaseUICheckbox *CBaseUICheckbox::setSizeToContent(int horizontalBorderSize, int verticalBorderSize) {
    // HACKHACK: broken
    CBaseUIButton::setSizeToContent(horizontalBorderSize, verticalBorderSize);
    /// setSize(this->fStringWidth+2*horizontalBorderSize, this->fStringHeight + 2*verticalBorderSize);
    this->setSize(this->getBlockBorder() * 2 + this->getBlockSize() + this->getBlockBorder() + this->fStringWidth +
                      horizontalBorderSize * 2,
                  this->fStringHeight + verticalBorderSize * 2);

    return this;
}

CBaseUICheckbox *CBaseUICheckbox::setWidthToContent(int horizontalBorderSize) {
    // HACKHACK: broken
    CBaseUIButton::setWidthToContent(horizontalBorderSize);
    /// setSize(this->fStringWidth+2*horizontalBorderSize, this->fStringHeight + 2*verticalBorderSize);
    this->setSizeX(this->getBlockBorder() * 2 + this->getBlockSize() + this->getBlockBorder() + this->fStringWidth +
                   horizontalBorderSize * 2);

    return this;
}
