// Copyright (c) 2024, kiwec, All rights reserved.
#include "UIUserContextMenu.h"

#include <algorithm>

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Chat.h"
#include "Engine.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "SpectatorScreen.h"
#include "UIContextMenu.h"
#include "UserCard.h"
#include "UserStatsScreen.h"

UIUserContextMenuScreen::UIUserContextMenuScreen() : OsuScreen() {
    this->bVisible = true;
    this->menu = new UIContextMenu();
    this->addBaseUIElement(this->menu);
}

void UIUserContextMenuScreen::onResolutionChange(Vector2 newResolution) {
    this->setSize(newResolution);
    OsuScreen::onResolutionChange(newResolution);
}

void UIUserContextMenuScreen::stealFocus() {
    OsuScreen::stealFocus();
    this->close();
}

void UIUserContextMenuScreen::open(i32 user_id, bool is_song_browser_button) {
    this->close();
    this->user_id = user_id;

    int slot_number = -1;
    if(bancho->is_in_a_multi_room()) {
        for(int i = 0; i < 16; i++) {
            if(bancho->room.slots[i].player_id == user_id) {
                slot_number = i;
                break;
            }
        }
    }

    this->menu->begin(is_song_browser_button ? osu->userButton->getSize().x : 0);

    /*
    if(!osu->userStats->isVisible() && (user_id <= 0 || (user_id == bancho->user_id))) {
        this->menu->addButton("View top plays", VIEW_TOP_PLAYS);
    }
    */

    if(user_id > 0) {
        this->menu->addButton("View profile page", VIEW_PROFILE);
    };

    if(user_id != bancho->user_id) {
        if(bancho->room.is_host() && slot_number != -1) {
            this->menu->addButton("Set as Host", UA_TRANSFER_HOST);
            this->menu->addButton("Kick", KICK);
        }

        auto user_info = BANCHO::User::get_user_info(user_id, true);
        if(user_info->has_presence) {
            // Without user info, we don't have the username
            this->menu->addButton("Start Chat", START_CHAT);

            // XXX: Not implemented
            // menu->addButton("Invite to game", INVITE_TO_GAME);
        }

        if(user_info->is_friend()) {
            this->menu->addButton("Revoke friendship", UA_REMOVE_FRIEND);
        } else {
            this->menu->addButton("Add as friend", UA_ADD_FRIEND);
        }

        if(cv::enable_spectating.getBool()) {
            if(bancho->spectated_player_id == user_id) {
                menu->addButton("Stop spectating", TOGGLE_SPECTATE);
            } else {
                menu->addButton("Spectate", TOGGLE_SPECTATE);
            }
        }
    }

    if(is_song_browser_button) {
        // Menu would open halfway off-screen, extra code to remove the jank
        this->menu->end(true, false);
        auto userPos = osu->userButton->getPos();
        this->menu->setPos(userPos.x, userPos.y - this->menu->getSize().y);
    } else {
        this->menu->end(false, false);
        this->menu->setPos(mouse->getPos());
    }
    this->menu->setClickCallback(SA::MakeDelegate<&UIUserContextMenuScreen::on_action>(this));
    this->menu->setVisible(true);
}

void UIUserContextMenuScreen::close() { this->menu->setVisible(false); }

void UIUserContextMenuScreen::on_action(const UString& /*text*/, int user_action) {
    auto user_info = BANCHO::User::get_user_info(this->user_id);
    int slot_number = -1;
    if(bancho->is_in_a_multi_room()) {
        for(int i = 0; i < 16; i++) {
            if(bancho->room.slots[i].player_id == this->user_id) {
                slot_number = i;
                break;
            }
        }
    }

    if(user_action == UA_TRANSFER_HOST) {
        Packet packet;
        packet.id = TRANSFER_HOST;
        BANCHO::Proto::write<u32>(&packet, slot_number);
        BANCHO::Net::send_packet(packet);
    } else if(user_action == KICK) {
        Packet packet;
        packet.id = MATCH_LOCK;
        BANCHO::Proto::write<u32>(&packet, slot_number);
        BANCHO::Net::send_packet(packet);  // kick by locking the slot
        BANCHO::Net::send_packet(packet);  // unlock the slot
    } else if(user_action == START_CHAT) {
        osu->chat->openChannel(user_info->name);
    } else if(user_action == VIEW_PROFILE) {
        // Fallback in case we're offline
        auto endpoint = bancho->endpoint;
        if(endpoint == "") endpoint = "ppy.sh";

        auto url = fmt::format("https://osu.{}/u/{}", endpoint, this->user_id);
        osu->notificationOverlay->addNotification("Opening browser, please wait ...", 0xffffffff, false, 0.75f);
        env->openURLInDefaultBrowser(url);
    } else if(user_action == UA_ADD_FRIEND) {
        Packet packet;
        packet.id = FRIEND_ADD;
        BANCHO::Proto::write<i32>(&packet, this->user_id);
        BANCHO::Net::send_packet(packet);
        BANCHO::User::friends.push_back(this->user_id);
    } else if(user_action == UA_REMOVE_FRIEND) {
        Packet packet;
        packet.id = FRIEND_REMOVE;
        BANCHO::Proto::write<i32>(&packet, this->user_id);
        BANCHO::Net::send_packet(packet);

        auto it = std::ranges::find(BANCHO::User::friends, this->user_id);
        if(it != BANCHO::User::friends.end()) {
            BANCHO::User::friends.erase(it);
        }
    } else if(user_action == TOGGLE_SPECTATE) {
        if(bancho->spectated_player_id == this->user_id) {
            stop_spectating();
        } else {
            start_spectating(this->user_id);
        }
    } else if(user_action == VIEW_TOP_PLAYS) {
        osu->userStats->setVisible(true);
    }

    this->menu->setVisible(false);
}

UIUserLabel::UIUserLabel(i32 user_id, const UString& username) : CBaseUILabel() {
    this->user_id = user_id;
    this->setText(username);
    this->setDrawFrame(false);
    this->setDrawBackground(false);
}

void UIUserLabel::onMouseUpInside(bool /*left*/, bool /*right*/) { osu->user_actions->open(this->user_id); }
