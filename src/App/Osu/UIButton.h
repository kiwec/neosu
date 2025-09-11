#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUIButton.h"

class UIButton : public CBaseUIButton {
   public:
    UIButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
      : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {}

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void setColor(Color color) { this->color = color; }
    void setUseDefaultSkin() { this->bDefaultSkin = true; }

    void setTooltipText(const UString& text);

    void onMouseInside() override;
    void onMouseOutside() override;

    void animateClickColor();
    bool is_loading = false;

    // HACKHACK: enough is enough
    bool bVisible2 = true;

   private:
    void onClicked(bool left = true, bool right = false) override;
    void onFocusStolen() override;

    bool bDefaultSkin{false};
    Color color{0xffffffff};
    float fClickAnim{0.f};
    float fHoverAnim{0.f};

    std::vector<UString> tooltipTextLines;
    bool bFocusStolenDelay{false};
};
