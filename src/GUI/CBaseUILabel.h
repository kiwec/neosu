#pragma once
// Copyright (c) 2014, PG, All rights reserved.
#include "CBaseUIElement.h"
#include "Color.h"
#include "UString.h"
class McFont;

class CBaseUILabel : public CBaseUIElement {
    NOCOPY_NOMOVE(CBaseUILabel)
   public:
    CBaseUILabel(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = {},
                 const UString &text = {});
    ~CBaseUILabel() override = default;

    void draw() override;
    void update(CBaseUIEventCtx &c) override;

    // cancer
    void setRelSizeX(float x) { this->relRect.setSize({x, this->relRect.getSize().y}); }

    // set
    CBaseUILabel *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUILabel *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }
    CBaseUILabel *setDrawTextShadow(bool drawTextShadow) {
        this->bDrawTextShadow = drawTextShadow;
        return this;
    }
    CBaseUILabel *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUILabel *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }
    CBaseUILabel *setTextColor(Color textColor) {
        this->textColor = textColor;
        return this;
    }
    CBaseUILabel *setShadowColor(Color shadowColor) {
        this->shadowColor = shadowColor;
        return this;
    }
    CBaseUILabel *setText(const UString &text) {
        this->sText = text;
        this->updateStringMetrics();
        return this;
    }
    CBaseUILabel *setFont(McFont *font) {
        this->font = font;
        this->updateStringMetrics();
        return this;
    }

    CBaseUILabel *setSizeToContent(int horizontalBorderSize = 1, int verticalBorderSize = 1) {
        this->setSize(this->fStringWidth + 2 * horizontalBorderSize, this->fStringHeight + 2 * verticalBorderSize);
        return this;
    }
    CBaseUILabel *setWidthToContent(int horizontalBorderSize = 1) {
        this->setSizeX(this->fStringWidth + 2 * horizontalBorderSize);
        return this;
    }
    CBaseUILabel *setTextJustification(TEXT_JUSTIFICATION textJustification) {
        this->textJustification = textJustification;
        return this;
    }
    CBaseUILabel *setScale(float newScale) {
        this->fScale = newScale;
        return this;
    }

    // get
    [[nodiscard]] inline Color getFrameColor() const { return this->frameColor; }
    [[nodiscard]] inline Color getBackgroundColor() const { return this->backgroundColor; }
    [[nodiscard]] inline Color getTextColor() const { return this->textColor; }
    [[nodiscard]] inline McFont *getFont() const { return this->font; }
    [[nodiscard]] inline const UString &getText() const { return this->sText; }

    void onResized() override { this->updateStringMetrics(); }

   protected:
    virtual void drawText();

    void updateStringMetrics();

    UString sText;
    McFont *font;

    float fStringWidth{0.f};
    float fStringHeight{0.f};

    Color frameColor{0xffffffff};
    Color backgroundColor{0xff000000};
    Color textColor{0xffffffff};
    Color shadowColor{0xff000000};

    float fScale{1.f};
    TEXT_JUSTIFICATION textJustification{TEXT_JUSTIFICATION::LEFT};

    bool bDrawFrame{true};
    bool bDrawBackground{true};

    // TODO: this should probably be enabled by default,
    // but need to look over everything to make sure we're not double-drawing shadows
    bool bDrawTextShadow{false};
};
