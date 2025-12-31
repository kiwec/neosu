#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.
#include "CBaseUILabel.h"

class UIIcon final : public CBaseUILabel {
   public:
    UIIcon(char16_t icon);

    void mouse_update(bool* propagate_clicks) override;
    void setTooltipText(const UString& text);

   private:
    void onFocusStolen() override;

    std::vector<UString> tooltipTextLines;

    bool bFocusStolenDelay{false};
};
