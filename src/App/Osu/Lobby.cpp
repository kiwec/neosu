#include "Lobby.h"

#include <utility>

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "Chat.h"
#include "Engine.h"
#include "Keyboard.h"
#include "MainMenu.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "PromptScreen.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "UIButton.h"

RoomUIElement::RoomUIElement(Lobby* multi, Room* room, float x, float y, float width, float height)
    : CBaseUIScrollView(x, y, width, height, "") {
    // NOTE: We can't store the room pointer, since it might expire later
    this->multi = multi;
    this->room_id = room->id;
    this->has_password = room->has_password;

    this->setBlockScrolling(true);
    this->setDrawFrame(true);

    float title_width = multi->font->getStringWidth(room->name) + 20;
    auto title_ui = new CBaseUILabel(10, 5, title_width, 30, "", room->name);
    title_ui->setDrawFrame(false);
    title_ui->setDrawBackground(false);
    this->getContainer()->addBaseUIElement(title_ui);

    char player_count_str[256] = {0};
    snprintf(player_count_str, 255, "Players: %d/%d", room->nb_players, room->nb_open_slots);
    float player_count_width = multi->font->getStringWidth(player_count_str) + 20;
    auto slots_ui = new CBaseUILabel(10, 33, player_count_width, 30, "", UString(player_count_str));
    slots_ui->setDrawFrame(false);
    slots_ui->setDrawBackground(false);
    this->getContainer()->addBaseUIElement(slots_ui);

    this->join_btn = new UIButton(10, 65, 120, 30, "", "Join room");
    this->join_btn->setUseDefaultSkin();
    this->join_btn->setColor(0xff00ff00);
    this->join_btn->setClickCallback(fastdelegate::MakeDelegate(this, &RoomUIElement::onRoomJoinButtonClick));
    this->getContainer()->addBaseUIElement(this->join_btn);

    if(room->has_password) {
        auto pwlabel = new CBaseUILabel(135, 64, 150, 30, "", "(password required)");
        pwlabel->setDrawFrame(false);
        pwlabel->setDrawBackground(false);
        this->getContainer()->addBaseUIElement(pwlabel);
    }
}

void RoomUIElement::onRoomJoinButtonClick(CBaseUIButton*  /*btn*/) {
    if(this->has_password) {
        this->multi->room_to_join = this->room_id;
        osu->prompt->prompt("Room password:",
                              fastdelegate::MakeDelegate(this->multi, &Lobby::on_room_join_with_password));
    } else {
        this->multi->joinRoom(this->room_id, "");
    }
}

Lobby::Lobby() : OsuScreen() {
    this->font = resourceManager->getFont("FONT_DEFAULT");

    auto heading = new CBaseUILabel(50, 30, 300, 40, "", "Multiplayer rooms");
    heading->setFont(osu->getTitleFont());
    heading->setSizeToContent(0, 0);
    heading->setDrawFrame(false);
    heading->setDrawBackground(false);
    this->addBaseUIElement(heading);

    this->create_room_btn = new UIButton(0, 0, 200, 50, "", "Create new room");
    this->create_room_btn->setUseDefaultSkin();
    this->create_room_btn->setColor(0xff00ff00);
    this->create_room_btn->setClickCallback(fastdelegate::MakeDelegate(this, &Lobby::on_create_room_clicked));
    this->addBaseUIElement(this->create_room_btn);

    this->list = new CBaseUIScrollView(0, 0, 0, 0, "");
    this->list->setDrawFrame(false);
    this->list->setDrawBackground(true);
    this->list->setBackgroundColor(0xdd000000);
    this->list->setHorizontalScrolling(false);
    this->addBaseUIElement(this->list);

    this->updateLayout(osu->getScreenSize());
}

void Lobby::onKeyDown(KeyboardEvent& key) {
    if(!this->bVisible) return;

    if(key.getKeyCode() == KEY_ESCAPE) {
        key.consume();
        this->setVisible(false);
        osu->mainMenu->setVisible(true);
        soundEngine->play(osu->getSkin()->menuBack);
        return;
    }

    // XXX: search bar
}

void Lobby::onKeyUp(KeyboardEvent&  /*key*/) {
    if(!this->bVisible) return;

    // XXX: search bar
}

void Lobby::onChar(KeyboardEvent&  /*key*/) {
    if(!this->bVisible) return;

    // XXX: search bar
}

void Lobby::onResolutionChange(Vector2 newResolution) { this->updateLayout(newResolution); }

CBaseUIContainer* Lobby::setVisible(bool visible) {
    if(visible == this->bVisible) return this;
    this->bVisible = visible;

    if(visible) {
        Packet packet;
        packet.id = JOIN_ROOM_LIST;
        send_packet(packet);

        packet = Packet();
        packet.id = CHANNEL_JOIN;
        write_string(&packet, "#lobby");
        send_packet(packet);

        // LOBBY presence is broken so we send MULTIPLAYER
        RichPresence::setBanchoStatus("Looking to play", MULTIPLAYER);

        // XXX: Could call refreshBeatmaps() here so we load them if not already done so.
        //      Would need to edit it a bit to work outside of songBrowser2, + display loading progress.
        //      Ideally, you'd do this in the background and still be able to browse rooms.
    } else {
        Packet packet;
        packet.id = EXIT_ROOM_LIST;
        send_packet(packet);

        packet = Packet();
        packet.id = CHANNEL_PART;
        write_string(&packet, "#lobby");
        send_packet(packet);

        for(auto room : this->rooms) {
            delete room;
        }
        this->rooms.clear();
    }

    osu->chat->updateVisibility();
    return this;
}

void Lobby::updateLayout(Vector2 newResolution) {
    this->setSize(newResolution);

    this->list->clear();
    this->list->setPos(round(newResolution.x * 0.6), 0);
    this->list->setSize(round(newResolution.x * 0.4), newResolution.y);

    if(this->rooms.empty()) {
        auto noRoomsOpenElement = new CBaseUILabel(0, 0, 0, 0, "", "There are no matches available.");
        noRoomsOpenElement->setTextJustification(CBaseUILabel::TEXT_JUSTIFICATION::TEXT_JUSTIFICATION_CENTERED);
        noRoomsOpenElement->setSizeToContent(20, 20);
        noRoomsOpenElement->setPos(this->list->getSize().x / 2 - noRoomsOpenElement->getSize().x / 2,
                                   this->list->getSize().y / 2 - noRoomsOpenElement->getSize().y / 2);
        this->list->getContainer()->addBaseUIElement(noRoomsOpenElement);
    }

    float heading_ratio = 70 / newResolution.y;
    float chat_ratio = 0.3;
    float free_ratio = 1.f - (heading_ratio + chat_ratio);
    this->create_room_btn->setPos(round(newResolution.x * 0.3) - this->create_room_btn->getSize().x / 2,
                              70 + round(newResolution.y * free_ratio / 2) - this->create_room_btn->getSize().y / 2);

    float y = 10;
    const float room_height = 105;
    for(auto room : this->rooms) {
        auto room_ui = new RoomUIElement(this, room, 10, y, this->list->getSize().x - 20, room_height);
        this->list->getContainer()->addBaseUIElement(room_ui);
        y += room_height + 20;
    }

    this->list->setScrollSizeToContent();
}

void Lobby::addRoom(Room* room) {
    this->rooms.push_back(room);
    this->updateLayout(this->getSize());
}

void Lobby::joinRoom(u32 id, const UString& password) {
    Packet packet;
    packet.id = JOIN_ROOM;
    write<u32>(&packet, id);
    write_string(&packet, password.toUtf8());
    send_packet(packet);

    for(CBaseUIElement* elm : this->list->getContainer()->getElements()) {
        RoomUIElement* room = dynamic_cast<RoomUIElement*>(elm);
        if(room == NULL) continue;
        if(room->room_id != id) continue;
        room->join_btn->is_loading = true;
        break;
    }

    debugLog("Joining room #%d with password '%s'\n", id, password.toUtf8());
    osu->getNotificationOverlay()->addNotification("Joining room...");
}

void Lobby::updateRoom(const Room& room) {
    for(auto old_room : this->rooms) {
        if(old_room->id == room.id) {
            *old_room = room;
            this->updateLayout(this->getSize());
            return;
        }
    }

    // On bancho.py, if a player creates a room when we're already in the lobby,
    // we won't receive a ROOM_CREATED but only a ROOM_UPDATED packet.
    auto new_room = new Room();
    *new_room = room;
    this->addRoom(new_room);
}

void Lobby::removeRoom(u32 room_id) {
    for(auto room : this->rooms) {
        if(room->id == room_id) {
            auto it = std::find(this->rooms.begin(), this->rooms.end(), room);
            this->rooms.erase(it);
            delete room;
            break;
        }
    }

    this->updateLayout(this->getSize());
}

void Lobby::on_create_room_clicked() {
    bancho->room = Room();
    bancho->room.name = "New room";  // XXX: doesn't work
    bancho->room.host_id = bancho->user_id;
    for(int i = 0; i < 16; i++) {
        bancho->room.slots[i].status = 1;  // open slot
    }
    bancho->room.slots[0].status = 4;  // not ready
    bancho->room.slots[0].player_id = bancho->user_id;

    Packet packet = {0};
    packet.id = CREATE_ROOM;
    bancho->room.pack(&packet);
    send_packet(packet);

    osu->getNotificationOverlay()->addNotification("Creating room...");
}

void Lobby::on_room_join_with_password(const UString& password) { this->joinRoom(this->room_to_join, password); }

void Lobby::on_room_join_failed() {
    // Updating layout will reset is_loading to false
    this->updateLayout(this->getSize());
}
