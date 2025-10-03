#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "OsuScreen.h"

class McFont;
class PauseButton;
class CBaseUILabel;
class UserCard;
class CBaseUIScrollView;
class UIButton;

class SpectatorScreen final : public OsuScreen {
   public:
    SpectatorScreen();

    void mouse_update(bool* propagate_clicks) override;
    void draw() override;
    bool isVisible() override;
    CBaseUIElement* setVisible(bool visible) override;
    void onKeyDown(KeyboardEvent& e) override;
    void onStopSpectatingClicked();

    UserCard* userCard = nullptr;

   private:
    McFont* font = nullptr;
    McFont* lfont = nullptr;
    PauseButton* pauseButton = nullptr;
    CBaseUIScrollView* background = nullptr;
    UIButton* stop_btn = nullptr;
    CBaseUILabel* spectating = nullptr;
    CBaseUILabel* status = nullptr;
};

void start_spectating(i32 user_id);
void stop_spectating();

// convar callback
void spectate_by_username(std::string_view username);
