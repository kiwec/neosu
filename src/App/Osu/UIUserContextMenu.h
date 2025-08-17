#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "CBaseUILabel.h"
#include "OsuScreen.h"

class UIContextMenu;

enum UserActions : uint8_t {
    UA_TRANSFER_HOST,
    KICK,
    VIEW_PROFILE,
    TOGGLE_SPECTATE,
    START_CHAT,
    INVITE_TO_GAME,
    UA_ADD_FRIEND,
    UA_REMOVE_FRIEND,
    VIEW_TOP_PLAYS,
};

class UIUserContextMenuScreen : public OsuScreen {
   public:
    UIUserContextMenuScreen();

    void onResolutionChange(vec2 newResolution) override;
    virtual void stealFocus();

    void open(i32 user_id, bool is_song_browser_button = false);
    void close();
    void on_action(const UString& text, int user_action);

    i32 user_id;
    UIContextMenu* menu = nullptr;
};

class UIUserLabel : public CBaseUILabel {
   public:
    UIUserLabel(i32 user_id, const UString& username);

    void onMouseUpInside(bool left = true, bool right = false) override;

    i32 user_id;
};
