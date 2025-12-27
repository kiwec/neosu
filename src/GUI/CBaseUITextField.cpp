// Copyright (c) 2014, PG, All rights reserved.
// TODO: finish this

#include "CBaseUITextField.h"

#include <utility>

#include "CBaseUIContainer.h"
#include "Engine.h"
#include "ResourceManager.h"
#include "Font.h"
#include "Graphics.h"

CBaseUITextField::CBaseUITextField(float xPos, float yPos, float xSize, float ySize, const UString& name, UString text)
    : CBaseUIScrollView(xPos, yPos, xSize, ySize, name) {
    this->textObject = new TextObject(2, 1, xSize, ySize, std::move(text));
    this->textObject->setParentSize(this->vSize);
    this->container->addBaseUIElement(this->textObject);
    this->setScrollSizeToContent(0);
}

void CBaseUITextField::draw() {
    CBaseUIScrollView::draw();
    if(!this->bVisible) return;

    // printf("scrollsize.y = %f, size.y = %f\n", getScrollSize().y, getSize().y);
}

void CBaseUITextField::onResized() {
    CBaseUIScrollView::onResized();
    this->textObject->setParentSize(this->vSize);
    // m_textObject->setSize(this->vSize);
    this->scrollToX(this->getRelPosX());
    this->scrollToY(this->getRelPosY());
    this->setScrollSizeToContent(0);
}

CBaseUITextField *CBaseUITextField::append(const UString& text) {
    UString oldText = this->textObject->getText();
    oldText.append(text);
    this->textObject->setText(oldText);
    return this;
}

//*********************************************************************************//
//								   TextObject
////
//*********************************************************************************//

CBaseUITextField::TextObject::TextObject(float xPos, float yPos, float xSize, float ySize, UString text)
    : CBaseUIElement(xPos, yPos, xSize, ySize, "") {
    this->font = resourceManager->getFont("FONT_DEFAULT");

    // colors
    this->textColor = 0xffffffff;

    this->setText(std::move(text));
}

void CBaseUITextField::TextObject::draw() {
    if(this->font == nullptr || this->sText.length() == 0) return;

    // g->setColor(0xffffffff);
    // g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);

    g->setColor(this->textColor);
    g->pushClipRect(McRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y));

    std::vector<UString> words = this->sText.split(ULITERAL(" "));
    float spaceWidth = this->font->getStringWidth(ULITERAL(" "));
    int border = 0;
    int lineSpacing = 4;
    float width = 0.0f;

    g->pushTransform();
    g->translate(this->vPos.x + border, this->vPos.y + this->fStringHeight + border);

    for(size_t i = 0; i < words.size(); i++) {
        float curWordWidth = this->font->getStringWidth(words[i]);

        bool newLine = words[i].find("\n") != -1;
        if(((width + curWordWidth + spaceWidth) > this->vParentSize.x - border && i != 0) || newLine) {
            g->translate(-width, this->fStringHeight + border + lineSpacing);
            width = 0.0f;
            if(newLine) continue;
        }
        width += curWordWidth + spaceWidth;

        g->drawString(this->font, words[i]);
        g->translate(curWordWidth + spaceWidth, 0);
    }

    g->popTransform();

    g->popClipRect();
}

void CBaseUITextField::TextObject::updateStringMetrics() {
    if(this->font == nullptr) return;

    this->fStringHeight = this->font->getHeight();
}

CBaseUIElement *CBaseUITextField::TextObject::setText(UString text) {
    this->sText = std::move(text);
    this->updateStringMetrics();
    return this;
}

void CBaseUITextField::TextObject::onResized() {
    if(this->sText.length() < 1) return;

    std::vector<UString> words = this->sText.split(ULITERAL(" "));
    if(words.size() < 1) return;

    float spaceWidth = this->font->getStringWidth(ULITERAL(" "));
    int border = 0;
    float width = 0.0f;
    int lineSpacing = 4;
    float height = this->fStringHeight + border + lineSpacing;

    int oldSizeX = -1;
    int oldSizeY = -1;

    for(size_t i = 0; i < words.size(); i++) {
        float curWordWidth = this->font->getStringWidth(words[i]);

        if(curWordWidth > this->vParentSize.x && curWordWidth > oldSizeX) oldSizeX = curWordWidth;

        bool newLine = words[i].find("\n") != -1;
        if(((width + curWordWidth + spaceWidth) > this->vParentSize.x - border && i != 0) ||
           (newLine && i != (int)(words.size() - 1))) {
            width = 0.0f;
            height += this->fStringHeight + border + lineSpacing;
        }
        width += curWordWidth + spaceWidth;
    }

    if(height > this->vParentSize.y && height > oldSizeY) oldSizeY = height;

    this->vSize.x = oldSizeX == -1 ? this->vParentSize.x + 1 : oldSizeX;
    this->vSize.y = oldSizeY == -1 ? this->vParentSize.y + 1 : oldSizeY;
}
