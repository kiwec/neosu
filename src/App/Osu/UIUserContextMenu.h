#pragma once
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
};

class UIUserContextMenuScreen : public OsuScreen {
   public:
    UIUserContextMenuScreen();

    void onResolutionChange(Vector2 newResolution) override;
    virtual void stealFocus();

    void open(i32 user_id);
    void close();
    void on_action(const UString& text, int user_action);

    i32 user_id;
    UIContextMenu *menu = NULL;
};

class UIUserLabel : public CBaseUILabel {
   public:
    UIUserLabel(i32 user_id, const UString& username);

    void onMouseUpInside(bool left = true, bool right = false) override;

    i32 user_id;
};
