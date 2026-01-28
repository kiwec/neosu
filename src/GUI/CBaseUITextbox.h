#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "CBaseUIElement.h"
#include "Color.h"

class McFont;

// TODO: so much duplication for text things between textbox/label/button
class CBaseUITextbox : public CBaseUIElement {
    NOCOPY_NOMOVE(CBaseUITextbox)
   public:
    CBaseUITextbox(float xPos = 0.0f, float yPos = 0.0f, float xSize = 0.0f, float ySize = 0.0f, UString name = "");
    ~CBaseUITextbox() override = default;

    void draw() override;
    void update(CBaseUIEventCtx &c) override;
    void onFocusStolen() override;
    void onResized() override;

    void onChar(KeyboardEvent &e) override;
    void onKeyDown(KeyboardEvent &e) override;

    UString getVisibleText();
    [[nodiscard]] inline const UString &getText() const { return this->sText; }
    [[nodiscard]] inline McFont *getFont() const { return this->font; }

    CBaseUITextbox *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUITextbox *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }

    CBaseUITextbox *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }
    CBaseUITextbox *setTextColor(Color textColor) {
        this->textColor = textColor;
        return this;
    }
    CBaseUITextbox *setCaretColor(Color caretColor) {
        this->caretColor = caretColor;
        return this;
    }
    CBaseUITextbox *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUITextbox *setFrameBrightColor(Color frameBrightColor) {
        this->frameBrightColor = frameBrightColor;
        return this;
    }
    CBaseUITextbox *setFrameDarkColor(Color frameDarkColor) {
        this->frameDarkColor = frameDarkColor;
        return this;
    }

    CBaseUITextbox *setFont(McFont *font);
    CBaseUITextbox *setTextAddX(float textAddX) {
        this->iTextAddX = textAddX;
        return this;
    }
    CBaseUITextbox *setCaretWidth(int caretWidth) {
        this->iCaretWidth = caretWidth;
        return this;
    }
    CBaseUITextbox *setTextJustification(TEXT_JUSTIFICATION textJustification) {
        this->textJustification = textJustification;
        this->setText(this->sText);
        return this;
    }

    virtual CBaseUITextbox *setText(UString text);

    void setCursorPosRight();

    bool hitEnter();
    [[nodiscard]] bool hasSelectedText() const;
    void clear();
    void focus(bool move_caret = true);

    bool is_password = false;

    UString sText;
    int iCaretPosition;
    void tickCaret();
    void updateTextPos();

    // Dummy function for RoomScreen generic macro
    void setSizeToContent(int /*a*/, int /*b*/) {}

   protected:
    virtual void drawText();

    // events
    void onMouseOutside() override;
    void onMouseDownInside(bool left = true, bool right = false) override;
    void onMouseDownOutside(bool left = true, bool right = false) override;
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onMouseUpOutside(bool left = true, bool right = false) override;

    void handleCaretKeyboardMove();
    void handleCaretKeyboardDelete();
    void updateCaretX();

    void handleDeleteSelectedText();
    void insertTextFromClipboard();
    void deselectText();
    UString getSelectedText();

    McFont *font;

    double fCaretBlinkStartTime{0.};
    float fTextScrollAddX;
    float fTextWidth;

    int iTextAddX;
    int iTextAddY;
    int iCaretX;
    int iCaretWidth;

    int iSelectStart;
    int iSelectEnd;
    int iSelectX;

    Color textColor;
    Color frameColor;
    Color frameBrightColor;
    Color frameDarkColor;
    Color caretColor;
    Color backgroundColor;

    TEXT_JUSTIFICATION textJustification{TEXT_JUSTIFICATION::LEFT};

    bool bHitenter;
    bool bContextMouse;
    bool bBlockMouse;
    bool bCatchMouse;
    bool bDrawFrame;
    bool bDrawBackground;
    bool bSelectCheck;
};
