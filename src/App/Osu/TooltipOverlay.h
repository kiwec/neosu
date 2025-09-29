#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "OsuScreen.h"

class TooltipOverlay final : public OsuScreen {
    NOCOPY_NOMOVE(TooltipOverlay)
   public:
    TooltipOverlay();
    ~TooltipOverlay() override;

    void draw() override;

    void begin();
    void addLine(const UString& text);
    void end();

   private:
    float fAnim;
    std::vector<UString> lines;

    bool bDelayFadeout;
};
