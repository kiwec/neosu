#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

// Important clarification: "Lobby" here refers to the place where you look
// for rooms. This is the ppy naming; to remember it, think of the #lobby
// channel the client joins when entering the lobby. See Room.h if you
// were looking for the 16-player holder thing.

#include "BanchoProtocol.h"
#include "CBaseUIScrollView.h"
#include "OsuScreen.h"

class CBaseUIButton;
class Lobby;
class UIButton;
class Room;

// NOTE: We make a CBaseUIScrollView but won't enable scrolling.
//       It's just to draw the frame! ^_^
struct RoomUIElement : CBaseUIScrollView {
    RoomUIElement(Lobby* multi, Room* room, float x, float y, float width, float height);

    UIButton* join_btn;
    CBaseUIScrollView* ui;
    Lobby* multi;
    i32 room_id;
    bool has_password;

    void updateLayout(vec2 pos, vec2 size);
    void onRoomJoinButtonClick(CBaseUIButton* btn);
};

class Lobby : public OsuScreen {
   public:
    Lobby();

    void onKeyDown(KeyboardEvent& e) override;
    void onKeyUp(KeyboardEvent& e) override;
    void onChar(KeyboardEvent& e) override;
    void onResolutionChange(vec2 newResolution) override;

    // /!\ Side-effect: sends bancho packets when changing state
    CBaseUIContainer* setVisible(bool visible) override;

    void addRoom(Room* room);
    void joinRoom(u32 id, const UString& password);
    void updateRoom(const Room& room);
    void removeRoom(u32 room_id);
    void updateLayout(vec2 newResolution);

    void on_create_room_clicked();

    void on_room_join_with_password(const UString& password);
    void on_room_join_failed();

    std::vector<Room*> rooms;
    UIButton* create_room_btn;
    CBaseUIScrollView* list;
    i32 room_to_join;
    McFont* font;
};
