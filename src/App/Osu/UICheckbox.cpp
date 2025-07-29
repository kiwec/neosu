#include "UICheckbox.h"

#include <utility>

#include "Engine.h"
#include "Osu.h"
#include "TooltipOverlay.h"

UICheckbox::UICheckbox(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
    : CBaseUICheckbox(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
    this->bFocusStolenDelay = false;
}

void UICheckbox::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUICheckbox::mouse_update(propagate_clicks);

    if(this->isMouseInside() && this->tooltipTextLines.size() > 0 && !this->bFocusStolenDelay) {
        osu->getTooltipOverlay()->begin();
        {
            for(const auto& tooltipTextLine : this->tooltipTextLines) {
                osu->getTooltipOverlay()->addLine(tooltipTextLine);
            }
        }
        osu->getTooltipOverlay()->end();
    }

    this->bFocusStolenDelay = false;
}

void UICheckbox::onFocusStolen() {
    CBaseUICheckbox::onFocusStolen();

    this->bMouseInside = false;
    this->bFocusStolenDelay = true;
}

void UICheckbox::setTooltipText(const UString& text) { this->tooltipTextLines = text.split("\n"); }
