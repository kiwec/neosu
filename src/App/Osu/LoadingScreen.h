#pragma once
// Copyright (c) 2026, kiwec, All rights reserved.
#include "types.h"
#include "UIOverlay.h"

class LoadingScreen final : public UIOverlay {
   public:
    LoadingScreen(UIOverlay* parent, std::function<f32()> get_progress_fn, std::function<void()> cancel_fn);

    void draw() override;
    void onKeyDown(KeyboardEvent& e) override;
    CBaseUIContainer* setVisible(bool visible) override;

   private:
    UIOverlay* parent;
    std::function<void()> cancel_fn;
    std::function<f32()> get_progress_fn;
};
