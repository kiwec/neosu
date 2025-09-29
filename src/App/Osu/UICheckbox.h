#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUICheckbox.h"

class UICheckbox final : public CBaseUICheckbox {
   public:
    UICheckbox(float xPos, float yPos, float xSize, float ySize, UString name, UString text);

    void mouse_update(bool *propagate_clicks) override;

    void setTooltipText(const UString& text);

   private:
    void onFocusStolen() override;

    std::vector<UString> tooltipTextLines;

    bool bFocusStolenDelay;
};
