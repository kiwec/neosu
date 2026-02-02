#pragma once
// Copyright (c) 2013, PG, All rights reserved.
#include "BaseEnvironment.h"

#include "CBaseUIElement.h"
#include "Color.h"
#include "MakeDelegateWrapper.h"

#include <utility>

class McFont;

// TODO: why does this have to basically duplicate all of CBaseUILabel
class CBaseUIButton : public CBaseUIElement {
    NOCOPY_NOMOVE(CBaseUIButton)
   public:
    CBaseUIButton(std::nullptr_t notext, float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0);
    CBaseUIButton(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = {},
                  UString text = {});
    ~CBaseUIButton() override = default;

    void draw() override;

    void click(bool left = true, bool right = false) { this->onClicked(left, right); }

    using CBaseUIButtonClickCB = std::function<void(CBaseUIButton *, bool, bool)>;

    template <typename Callable>
    CBaseUIButton *setClickCallback(Callable &&cb) {
        using CBType = std::decay_t<Callable>;

        if constexpr(std::is_invocable_v<CBType, CBaseUIButton *, bool, bool>) {
            this->clickCallback = std::make_unique<CBaseUIButtonClickCB>(std::forward<Callable>(cb));
        } else if constexpr(std::is_invocable_v<CBType, bool, bool>) {
            this->clickCallback = std::make_unique<CBaseUIButtonClickCB>(
                [cb = std::forward<Callable>(cb)](CBaseUIButton *, bool left, bool right) { cb(left, right); });
        } else if constexpr(std::is_invocable_v<CBType, CBaseUIButton *>) {
            this->clickCallback = std::make_unique<CBaseUIButtonClickCB>(
                [cb = std::forward<Callable>(cb)](CBaseUIButton *btn, bool, bool) { cb(btn); });
        } else if constexpr(std::is_invocable_v<CBType>) {
            this->clickCallback = std::make_unique<CBaseUIButtonClickCB>(
                [cb = std::forward<Callable>(cb)](CBaseUIButton *, bool, bool) { cb(); });
        } else if constexpr(SA::is_delegate_v<CBType>) {
            using traits = SA::delegate_traits<CBType>;
            using FirstArg = typename traits::template nth_arg<0>;
            static_assert(
                std::is_pointer_v<FirstArg> && std::is_base_of_v<CBaseUIButton, std::remove_pointer_t<FirstArg>>,
                "Delegate first argument must be a pointer to CBaseUIButton or derived");

            if constexpr(traits::arity == 1) {
                this->clickCallback = std::make_unique<CBaseUIButtonClickCB>(
                    [cb = std::forward<Callable>(cb)](CBaseUIButton *btn, bool, bool) {
                        cb(static_cast<FirstArg>(btn));
                    });
            } else if constexpr(traits::arity == 3) {
                this->clickCallback = std::make_unique<CBaseUIButtonClickCB>(
                    [cb = std::forward<Callable>(cb)](CBaseUIButton *btn, bool left, bool right) {
                        cb(static_cast<FirstArg>(btn), left, right);
                    });
            } else {
                static_assert(Env::always_false_v<Callable>, "Unsupported delegate arity for derived button callback");
            }
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
    CBaseUIButton *setDrawShadow(bool enabled) {
        this->bDrawShadow = enabled;
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
    CBaseUIButton *setTextJustification(TEXT_JUSTIFICATION j) {
        this->textJustification = j;
        return this;
    }

    CBaseUIButton *setText(UString text) {
        if(!text.isEmpty()) {
            if(this->sText) {
                *this->sText = std::move(text);
            } else {
                this->sText = std::make_unique<UString>(std::move(text));
            }
        } else {
            this->sText.reset();
        }
        this->updateStringMetrics();
        return this;
    }

    CBaseUIButton *setFont(McFont *font) {
        this->font = font;
        this->updateStringMetrics();
        return this;
    }

    virtual CBaseUIButton *setSizeToContent(int horizontalBorderSize = 1, int verticalBorderSize = 1) {
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
    [[nodiscard]] inline const UString &getText() const {
        return this->sText ? *this->sText : CBaseUIElement::emptyUString;
    }
    [[nodiscard]] inline McFont *getFont() const { return this->font; }

    // events
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onResized() override { this->updateStringMetrics(); }

   protected:
    virtual void onClicked(bool left = true, bool right = false);

    virtual void drawText();

    void drawHoverRect(int distance);

    void updateStringMetrics();

    // same as CBaseUIElement::sName, only store if non-empty
    // UStrings have a lot of overhead (~64 bytes) even if empty
    std::unique_ptr<UString> sText{nullptr};

    // callbacks, either void, with ourself as the argument, or with the held left/right buttons
    std::unique_ptr<CBaseUIButtonClickCB> clickCallback{nullptr};

    McFont *font;

    float fStringWidth{0.f};
    float fStringHeight{0.f};

    Color frameColor{argb(255, 255, 255, 255)};
    Color backgroundColor{argb(255, 0, 0, 0)};
    Color textColor{argb(255, 255, 255, 255)};
    Color textBrightColor{argb(0, 0, 0, 0)};
    Color textDarkColor{argb(0, 0, 0, 0)};

    // settings
    TEXT_JUSTIFICATION textJustification{TEXT_JUSTIFICATION::CENTERED};

    bool bDrawFrame{true};
    bool bDrawBackground{true};
    bool bDrawShadow{true};
};
