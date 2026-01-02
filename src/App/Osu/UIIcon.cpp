// Copyright (c) 2025, kiwec, All rights reserved.
#include "UIIcon.h"

#include "Osu.h"
#include "TooltipOverlay.h"

UIIcon::UIIcon(char16_t icon) : CBaseUILabel(0.f, 0.f, 0.f, 0.f, "", UString(&icon, 1)) {
    this->setFont(osu->getFontIcons());
    this->setDrawBackground(false);
    this->setDrawFrame(false);
}

void UIIcon::mouse_update(bool* propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUILabel::mouse_update(propagate_clicks);

    if(this->isMouseInside() && this->tooltipTextLines.size() > 0 && !this->bFocusStolenDelay) {
        osu->getTooltipOverlay()->begin();
        for(const auto& tooltipTextLine : this->tooltipTextLines) {
            osu->getTooltipOverlay()->addLine(tooltipTextLine);
        }
        osu->getTooltipOverlay()->end();
    }

    this->bFocusStolenDelay = false;
}

void UIIcon::onFocusStolen() {
    CBaseUILabel::onFocusStolen();
    this->bMouseInside = false;
    this->bFocusStolenDelay = true;
}

void UIIcon::setTooltipText(const UString& text) { this->tooltipTextLines = text.split(US_("\n")); }
