#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.
#include <memory>

#include "CBaseUILabel.h"

// TODO: draw frame on hover
// TODO: icon alignment/size is fucked
// XXX: shit naming
// XXX: text shadow, like CBaseUIButton

class UIButtonWithIcon : public CBaseUIContainer {
   public:
    UIButtonWithIcon(UString text, char16_t icon) : CBaseUIContainer(0, 0, 0, 0, "") {
        // XXX: hardcodes
        f32 icon_width = 20.f;
        f32 inner_margin = 4.f;
        f32 btn_height = 20.f;

        UString iconString;
        iconString.insert(0, icon);
        this->icon = new CBaseUILabel(0, 0, 0, 0, "", iconString);
        this->icon->setDrawBackground(false);
        this->icon->setDrawFrame(false);
        this->icon->setFont(osu->getFontIcons());
        this->icon->setSizeToContent();
        this->addBaseUIElement(this->icon);

        this->text = new CBaseUILabel(0, 0, 0, 0, "", text);
        this->text->setDrawBackground(false);
        this->text->setDrawFrame(false);
        this->text->setSizeToContent();
        this->addBaseUIElement(this->text);

        this->text->setRelPos(icon_width + inner_margin, 0);
        this->update_pos();

        this->setSize(icon_width + inner_margin + this->text->getSize().x, btn_height);
    }

    template <typename Callable>
    void setClickCallback(Callable&& cb) {
        this->clickCallback = std::forward<Callable>(cb);
    }

    void onMouseUpInside(bool left, bool right) override {
        if(this->clickCallback) this->clickCallback();
    }

   private:
    std::function<void()> clickCallback;
    CBaseUILabel* icon;
    CBaseUILabel* text;
};
