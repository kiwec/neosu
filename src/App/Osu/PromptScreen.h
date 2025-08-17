#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "OsuScreen.h"

class CBaseUILabel;
class CBaseUITextbox;
class Graphics;
class UIButton;

class PromptScreen : public OsuScreen {
   public:
    PromptScreen();
    void onResolutionChange(vec2 newResolution) override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    using PromptResponseCallback = SA::delegate<void(const UString &)>;
    void prompt(const UString &msg, const PromptResponseCallback &callback);

   private:
    void on_ok();
    void on_cancel();

    CBaseUILabel *prompt_label;
    CBaseUITextbox *prompt_input;
    UIButton *ok_btn;
    UIButton *cancel_btn;
    PromptResponseCallback callback;
};
