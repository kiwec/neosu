#pragma once
// Copyright (c) 2013, PG, All rights reserved.
#include <utility>

#include "CBaseUIElement.h"

class McFont;

class CBaseUIButton : public CBaseUIElement {
   public:
    CBaseUIButton(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "",
                  UString text = "");
    ~CBaseUIButton() override { ; }

    void draw() override;

    void click(bool left = true, bool right = false) { this->onClicked(left, right); }

    template <typename Callable>
    CBaseUIButton *setClickCallback(Callable &&cb) {
        using CBType = std::decay_t<Callable>;

        if constexpr(std::is_invocable_v<CBType, CBaseUIButton *, bool, bool>) {
            this->clickCallback = std::forward<Callable>(cb);
        } else if constexpr(std::is_invocable_v<CBType, bool, bool>) {
            this->clickCallback = [cb = std::forward<Callable>(cb)](CBaseUIButton *, bool left, bool right) { cb(left, right); };
        } else if constexpr(std::is_invocable_v<CBType, CBaseUIButton *>) {
            this->clickCallback = [cb = std::forward<Callable>(cb)](CBaseUIButton *btn, bool, bool) { cb(btn); };
        } else if constexpr(std::is_invocable_v<CBType>) {
            this->clickCallback = [cb = std::forward<Callable>(cb)](CBaseUIButton *, bool, bool) { cb(); };
        } else {
            static_assert(Env::always_false_v<Callable>, "Programmer Error (bad callback signature)");
        }

        return this;
    }

    // set
    CBaseUIButton *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUIButton *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }
    CBaseUIButton *setTextLeft(bool textLeft) {
        this->bTextLeft = textLeft;
        this->updateStringMetrics();
        return this;
    }

    CBaseUIButton *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUIButton *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }
    CBaseUIButton *setTextColor(Color textColor) {
        this->textColor = textColor;
        this->textBrightColor = this->textDarkColor = 0;
        return this;
    }
    CBaseUIButton *setTextBrightColor(Color textBrightColor) {
        this->textBrightColor = textBrightColor;
        return this;
    }
    CBaseUIButton *setTextDarkColor(Color textDarkColor) {
        this->textDarkColor = textDarkColor;
        return this;
    }

    CBaseUIButton *setText(UString text) {
        this->sText = std::move(text);
        this->updateStringMetrics();
        return this;
    }
    CBaseUIButton *setFont(McFont *font) {
        this->font = font;
        this->updateStringMetrics();
        return this;
    }

    CBaseUIButton *setSizeToContent(int horizontalBorderSize = 1, int verticalBorderSize = 1) {
        this->setSize(this->fStringWidth + 2 * horizontalBorderSize, this->fStringHeight + 2 * verticalBorderSize);
        return this;
    }
    CBaseUIButton *setWidthToContent(int horizontalBorderSize = 1) {
        this->setSizeX(this->fStringWidth + 2 * horizontalBorderSize);
        return this;
    }

    // get
    [[nodiscard]] inline Color getFrameColor() const { return this->frameColor; }
    [[nodiscard]] inline Color getBackgroundColor() const { return this->backgroundColor; }
    [[nodiscard]] inline Color getTextColor() const { return this->textColor; }
    [[nodiscard]] inline const UString &getText() const { return this->sText; }
    [[nodiscard]] inline McFont *getFont() const { return this->font; }
    [[nodiscard]] inline bool isTextLeft() const { return this->bTextLeft; }

    // events
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onResized() override { this->updateStringMetrics(); }

   protected:
    virtual void onClicked(bool left = true, bool right = false);

    virtual void drawText();

    void drawHoverRect(int distance);

    void updateStringMetrics();

    UString sText;

    // callbacks, either void, with ourself as the argument, or with the held left/right buttons
    std::function<void(CBaseUIButton *, bool, bool)> clickCallback;

    McFont *font;

    float fStringWidth;
    float fStringHeight;

    Color frameColor;
    Color backgroundColor;
    Color textColor;
    Color textBrightColor;
    Color textDarkColor;

    bool bDrawFrame;
    bool bDrawBackground;
    bool bTextLeft;
};
