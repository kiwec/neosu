#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "BanchoProtocol.h"
#include "CBaseUIScrollView.h"
#include "OsuScreen.h"
#include "score.h"

class CBaseUICheckbox;
class CBaseUILabel;
class CBaseUITextbox;
class DatabaseBeatmap;
class PauseButton;
class UIButton;
class UICheckbox;
class UIContextMenu;

class UIModList : public CBaseUIContainer {
   public:
    UIModList(u32 *flags) : CBaseUIContainer(0, 0, 0, 0, "mod_list") { this->flags = flags; }

    u32 *flags;

    void draw() override;
    bool isVisible() override;
};

class RoomScreen : public OsuScreen {
   public:
    RoomScreen();
    ~RoomScreen() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;
    void onResolutionChange(Vector2 newResolution) override;
    CBaseUIContainer *setVisible(bool visible) override;  // does nothing

    void updateLayout(Vector2 newResolution);
    void updateSettingsLayout(Vector2 newResolution);
    void ragequit(bool play_sound = true);

    void on_map_change();
    void on_room_joined(Room room);
    void on_room_updated(Room room);
    void on_match_started(Room room);
    void on_match_score_updated(Packet *packet);
    void on_all_players_loaded();
    void on_player_failed(i32 slot_id);
    void on_match_finished();
    void on_all_players_skipped();
    void on_player_skip(i32 user_id);
    void on_match_aborted();
    void onClientScoreChange(bool force = false);
    void onReadyButtonClick();

    FinishedScore get_approximate_score();

    // Host only
    void onStartGameClicked();
    void onSelectModsClicked();
    void onSelectMapClicked();
    void onChangePasswordClicked();
    void onChangeWinConditionClicked();
    void onWinConditionSelected(const UString &win_condition_str, int win_condition);
    void set_new_password(const UString &new_password);
    void onFreemodCheckboxChanged(CBaseUICheckbox *checkbox);

    CBaseUILabel* map_label = nullptr;
    CBaseUILabel* mods_label = nullptr;
    CBaseUIScrollView* settings = nullptr;
    CBaseUIScrollView* slotlist = nullptr;
    CBaseUIScrollView* map = nullptr;
    CBaseUILabel* host = nullptr;
    CBaseUILabel* room_name = nullptr;
    CBaseUILabel* room_name_iptl = nullptr;
    CBaseUITextbox* room_name_ipt = nullptr;
    UIButton* change_password_btn = nullptr;
    CBaseUILabel* win_condition = nullptr;
    UIButton* change_win_condition_btn = nullptr;
    CBaseUILabel* map_title = nullptr;
    CBaseUILabel* map_attributes = nullptr;
    CBaseUILabel* map_attributes2 = nullptr;
    CBaseUILabel* map_stars = nullptr;
    UIButton* select_map_btn = nullptr;
    UIButton* select_mods_btn = nullptr;
    UICheckbox* freemod = nullptr;
    UIModList* mods = nullptr;
    CBaseUILabel* no_mods_selected = nullptr;
    UIButton* ready_btn = nullptr;
    UIContextMenu* contextMenu = nullptr;

    PauseButton* pauseButton = nullptr;
    McFont* font = nullptr;
    McFont* lfont = nullptr;
    time_t last_packet_tms = {0};
};
