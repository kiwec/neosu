// Copyright (c) 2024, kiwec, All rights reserved.
#include "RoomScreen.h"

#include <sstream>
#include <utility>

#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Beatmap.h"
#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUITextbox.h"
#include "Changelog.h"
#include "Chat.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "HUD.h"
#include "Keyboard.h"
#include "LegacyReplay.h"
#include "Lobby.h"
#include "MainMenu.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "Osu.h"
#include "PromptScreen.h"
#include "RankingScreen.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser/SongBrowser.h"
#include "SongBrowser/SongButton.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "UIAvatar.h"
#include "UIButton.h"
#include "UICheckbox.h"
#include "UIContextMenu.h"
#include "UIUserContextMenu.h"

void UIModList::draw() {
    std::vector<SkinImage *> mods;

    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Perfect))
        mods.push_back(osu->getSkin()->getSelectionModPerfect());
    else if(ModMasks::legacy_eq(*this->flags, LegacyFlags::SuddenDeath))
        mods.push_back(osu->getSkin()->getSelectionModSuddenDeath());

    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::NoFail)) mods.push_back(osu->getSkin()->getSelectionModNoFail());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Easy)) mods.push_back(osu->getSkin()->getSelectionModEasy());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::TouchDevice)) mods.push_back(osu->getSkin()->getSelectionModTD());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Hidden)) mods.push_back(osu->getSkin()->getSelectionModHidden());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::HardRock)) mods.push_back(osu->getSkin()->getSelectionModHardRock());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Relax)) mods.push_back(osu->getSkin()->getSelectionModRelax());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Autoplay)) mods.push_back(osu->getSkin()->getSelectionModAutoplay());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::SpunOut)) mods.push_back(osu->getSkin()->getSelectionModSpunOut());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Autopilot)) mods.push_back(osu->getSkin()->getSelectionModAutopilot());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::Target)) mods.push_back(osu->getSkin()->getSelectionModTarget());
    if(ModMasks::legacy_eq(*this->flags, LegacyFlags::ScoreV2)) mods.push_back(osu->getSkin()->getSelectionModScorev2());

    g->setColor(0xffffffff);
    vec2 modPos = this->vPos;
    for(auto mod : mods) {
        float target_height = this->getSize().y;
        float scaling_factor = target_height / mod->getSize().y;
        float target_width = mod->getSize().x * scaling_factor;

        vec2 fixed_pos = modPos;
        fixed_pos.x += (target_width / 2);
        fixed_pos.y += (target_height / 2);
        mod->draw(fixed_pos, scaling_factor);

        // Overlap mods a bit, just like peppy's client does
        modPos.x += target_width * 0.6;
    }
}

bool UIModList::isVisible() { return *this->flags != 0; }

#define INIT_LABEL(label_name, default_text, is_big)                      \
    do {                                                                  \
        label_name = new CBaseUILabel(0, 0, 0, 0, "label", default_text); \
        label_name->setFont(is_big ? lfont : font);                       \
        label_name->setSizeToContent(0, 0);                               \
        label_name->setDrawFrame(false);                                  \
        label_name->setDrawBackground(false);                             \
    } while(0)

#define ADD_ELEMENT(element)                                       \
    do {                                                           \
        element->setPos(10, settings_y);                           \
        this->settings->getContainer()->addBaseUIElement(element); \
        settings_y += element->getSize().y + 20;                   \
    } while(0)

#define ADD_BUTTON(button, label)                                       \
    do {                                                                \
        button->setPos(label->getSize().x + 20, label->getPos().y - 7); \
        this->settings->getContainer()->addBaseUIElement(button);       \
    } while(0)

RoomScreen::RoomScreen() : OsuScreen() {
    this->font = resourceManager->getFont("FONT_DEFAULT");
    this->lfont = osu->getSubTitleFont();

    this->pauseButton = new PauseButton(0, 0, 0, 0, "pause_btn", "");
    this->pauseButton->setClickCallback(SA::MakeDelegate<&MainMenu::onPausePressed>(osu->mainMenu));
    this->addBaseUIElement(this->pauseButton);

    this->settings = new CBaseUIScrollView(0, 0, 0, 0, "room_settings");
    this->settings->setDrawFrame(false);  // it's off by 1 pixel, turn it OFF
    this->settings->setDrawBackground(true);
    this->settings->setBackgroundColor(0xdd000000);
    this->settings->setHorizontalScrolling(false);
    this->addBaseUIElement(this->settings);

    INIT_LABEL(this->room_name, "Multiplayer room", true);
    INIT_LABEL(this->host, "Host: None", false);  // XXX: make it an UIUserLabel

    INIT_LABEL(this->room_name_iptl, "Room name", false);
    this->room_name_ipt = new CBaseUITextbox(0, 0, this->settings->getSize().x, 40, "");
    this->room_name_ipt->setText("Multiplayer room");

    this->change_password_btn = new UIButton(0, 0, 190, 40, "change_password_btn", "Change password");
    this->change_password_btn->setColor(0xff0e94b5);
    this->change_password_btn->setUseDefaultSkin();
    this->change_password_btn->setClickCallback(SA::MakeDelegate<&RoomScreen::onChangePasswordClicked>(this));

    INIT_LABEL(this->win_condition, "Win condition: Score", false);
    this->change_win_condition_btn = new UIButton(0, 0, 240, 40, "change_win_condition_btn", "Win condition: Score");
    this->change_win_condition_btn->setColor(0xff00ff00);
    this->change_win_condition_btn->setUseDefaultSkin();
    this->change_win_condition_btn->setClickCallback(
        SA::MakeDelegate<&RoomScreen::onChangeWinConditionClicked>(this));

    INIT_LABEL(map_label, "Beatmap", true);
    this->select_map_btn = new UIButton(0, 0, 130, 40, "select_map_btn", "Select map");
    this->select_map_btn->setColor(0xff0e94b5);
    this->select_map_btn->setUseDefaultSkin();
    this->select_map_btn->setClickCallback(SA::MakeDelegate<&RoomScreen::onSelectMapClicked>(this));

    INIT_LABEL(this->map_title, "(no map selected)", false);
    INIT_LABEL(this->map_stars, "", false);
    INIT_LABEL(this->map_attributes, "", false);
    INIT_LABEL(this->map_attributes2, "", false);

    INIT_LABEL(mods_label, "Mods", true);
    this->select_mods_btn = new UIButton(0, 0, 180, 40, "select_mods_btn", "Select mods [F1]");
    this->select_mods_btn->setColor(0xff0e94b5);
    this->select_mods_btn->setUseDefaultSkin();
    this->select_mods_btn->setClickCallback(SA::MakeDelegate<&RoomScreen::onSelectModsClicked>(this));
    this->freemod = new UICheckbox(0, 0, 200, 50, "allow_freemod", "Freemod");
    this->freemod->setDrawFrame(false);
    this->freemod->setDrawBackground(false);
    this->freemod->setChangeCallback(SA::MakeDelegate<&RoomScreen::onFreemodCheckboxChanged>(this));
    this->mods = new UIModList(&bancho->room.mods);
    INIT_LABEL(this->no_mods_selected, "No mods selected.", false);

    this->ready_btn = new UIButton(0, 0, 300, 50, "start_game_btn", "Start game");
    this->ready_btn->setColor(0xff00ff00);
    this->ready_btn->setUseDefaultSkin();
    this->ready_btn->setClickCallback(SA::MakeDelegate<&RoomScreen::onReadyButtonClick>(this));
    this->ready_btn->is_loading = true;

    auto player_list_label = new CBaseUILabel(50, 50, 0, 0, "label", "Player list");
    player_list_label->setFont(this->lfont);
    player_list_label->setSizeToContent(0, 0);
    player_list_label->setDrawFrame(false);
    player_list_label->setDrawBackground(false);
    this->addBaseUIElement(player_list_label);
    this->slotlist = new CBaseUIScrollView(50, 90, 0, 0, "slot_list");
    this->slotlist->setDrawFrame(true);
    this->slotlist->setDrawBackground(true);
    this->slotlist->setBackgroundColor(0xdd000000);
    this->slotlist->setHorizontalScrolling(false);
    this->addBaseUIElement(this->slotlist);

    this->contextMenu = new UIContextMenu(50, 50, 150, 0, "", this->settings);
    this->addBaseUIElement(this->contextMenu);

    this->updateLayout(osu->getScreenSize());
}

RoomScreen::~RoomScreen() {
    this->settings->getContainer()->invalidate();
    SAFE_DELETE(this->room_name);
    SAFE_DELETE(this->change_password_btn);
    SAFE_DELETE(this->host);
    SAFE_DELETE(this->room_name_iptl);
    SAFE_DELETE(this->room_name_ipt);
    SAFE_DELETE(this->select_map_btn);
    SAFE_DELETE(this->select_mods_btn);
    SAFE_DELETE(this->change_win_condition_btn);
    SAFE_DELETE(this->win_condition);
    SAFE_DELETE(map_label);
    SAFE_DELETE(this->map_title);
    SAFE_DELETE(this->map_stars);
    SAFE_DELETE(this->map_attributes);
    SAFE_DELETE(this->map_attributes2);
    SAFE_DELETE(mods_label);
    SAFE_DELETE(this->freemod);
    SAFE_DELETE(this->no_mods_selected);
    SAFE_DELETE(this->mods);
    SAFE_DELETE(this->ready_btn);
}

void RoomScreen::draw() {
    if(!this->bVisible) return;

    static i32 current_map_id = -1;
    if(bancho->room.map_id == -1 || bancho->room.map_id == 0) {
        this->map_title->setText("Host is selecting a map...");
        this->map_title->setSizeToContent(0, 0);
        this->ready_btn->is_loading = true;
    } else if(bancho->room.map_id != current_map_id) {
        float progress = -1.f;
        auto beatmap = Downloader::download_beatmap(bancho->room.map_id, bancho->room.map_md5, &progress);
        if(progress == -1.f) {
            auto error_str = UString::format("Failed to download Beatmap #%d :(", bancho->room.map_id);
            this->map_title->setText(error_str);
            this->map_title->setSizeToContent(0, 0);
            this->ready_btn->is_loading = true;
        } else if(progress < 1.f) {
            auto text = UString::format("Downloading... %.2f%%", progress * 100.f);
            this->map_title->setText(text.toUtf8());
            this->map_title->setSizeToContent(0, 0);
            this->ready_btn->is_loading = true;
        } else if(beatmap != nullptr) {
            current_map_id = bancho->room.map_id;
            this->on_map_change();
        }
    }

    // XXX: Add convar for toggling room backgrounds
    SongBrowser::drawSelectedBeatmapBackgroundImage(1.0);
    OsuScreen::draw();

    // Update avatar visibility status
    for(auto elm : this->slotlist->getContainer()->getElements()) {
        if(elm->getName() == UString("avatar")) {
            // NOTE: Not checking horizontal visibility
            auto avatar = (UIAvatar *)elm;
            bool is_below_top = avatar->getPos().y + avatar->getSize().y >= this->slotlist->getPos().y;
            bool is_above_bottom = avatar->getPos().y <= this->slotlist->getPos().y + this->slotlist->getSize().y;
            avatar->on_screen = is_below_top && is_above_bottom;
        }
    }
}

void RoomScreen::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible || osu->songBrowser2->isVisible()) return;

    const bool room_name_changed = this->room_name_ipt->getText() != bancho->room.name;
    if(bancho->room.is_host() && room_name_changed) {
        // XXX: should only update 500ms after last input
        bancho->room.name = this->room_name_ipt->getText();

        Packet packet;
        packet.id = MATCH_CHANGE_SETTINGS;
        bancho->room.pack(&packet);
        BANCHO::Net::send_packet(packet);

        // Update room name in rich presence info
        RichPresence::onMultiplayerLobby();
    }

    this->pauseButton->setPaused(!osu->getSelectedBeatmap()->isPreviewMusicPlaying());

    this->contextMenu->mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;

    OsuScreen::mouse_update(propagate_clicks);
}

void RoomScreen::onKeyDown(KeyboardEvent &key) {
    if(!this->bVisible || osu->songBrowser2->isVisible()) return;

    if(key.getKeyCode() == KEY_ESCAPE) {
        key.consume();
        this->ragequit();
        return;
    }

    if(key.getKeyCode() == KEY_F1) {
        key.consume();
        if(bancho->room.freemods || bancho->room.is_host()) {
            osu->modSelector->setVisible(!osu->modSelector->isVisible());
            this->bVisible = !osu->modSelector->isVisible();
        }
        return;
    }

    OsuScreen::onKeyDown(key);
}

void RoomScreen::onKeyUp(KeyboardEvent &key) {
    if(!this->bVisible || osu->songBrowser2->isVisible()) return;
    OsuScreen::onKeyUp(key);
}

void RoomScreen::onChar(KeyboardEvent &key) {
    if(!this->bVisible || osu->songBrowser2->isVisible()) return;
    OsuScreen::onChar(key);
}

void RoomScreen::onResolutionChange(vec2 newResolution) { this->updateLayout(newResolution); }

CBaseUIContainer *RoomScreen::setVisible(bool visible) {
    if(this->bVisible == visible) return this;

    // NOTE: Calling setVisible(false) does not quit the room! Call ragequit() instead.
    this->bVisible = visible;

    if(visible) {
        soundEngine->play(osu->getSkin()->menuBack);
    }

    return this;
}

void RoomScreen::updateSettingsLayout(vec2 newResolution) {
    const bool is_host = bancho->room.is_host();
    int settings_y = 10;

    this->settings->getContainer()->invalidate();
    this->settings->setPos(round(newResolution.x * 0.6), 0);
    this->settings->setSize(round(newResolution.x * 0.4), newResolution.y);

    // Room name (title)
    this->room_name->setText(bancho->room.name);
    this->room_name->setSizeToContent();
    ADD_ELEMENT(this->room_name);
    if(is_host) {
        ADD_BUTTON(this->change_password_btn, this->room_name);
    }

    // Host name
    if(!is_host) {
        UString host_str = "Host: None";
        if(bancho->room.host_id > 0) {
            auto host = BANCHO::User::get_user_info(bancho->room.host_id, true);
            host_str = UString::format("Host: %s", host->name.toUtf8());
        }
        this->host->setText(host_str);
        ADD_ELEMENT(this->host);
    }

    if(is_host) {
        // Room name (input)
        ADD_ELEMENT(this->room_name_iptl);
        this->room_name_ipt->setSize(this->settings->getSize().x - 20, 40);
        ADD_ELEMENT(this->room_name_ipt);
    }

    // Win condition
    if(bancho->room.win_condition == SCOREV1) {
        this->win_condition->setText("Win condition: Score");
    } else if(bancho->room.win_condition == ACCURACY) {
        this->win_condition->setText("Win condition: Accuracy");
    } else if(bancho->room.win_condition == COMBO) {
        this->win_condition->setText("Win condition: Combo");
    } else if(bancho->room.win_condition == SCOREV2) {
        this->win_condition->setText("Win condition: ScoreV2");
    }
    if(is_host) {
        this->change_win_condition_btn->setText(this->win_condition->getText());
        ADD_ELEMENT(this->change_win_condition_btn);
    } else {
        ADD_ELEMENT(this->win_condition);
    }

    // Beatmap
    settings_y += 10;
    ADD_ELEMENT(map_label);
    if(is_host) {
        ADD_BUTTON(this->select_map_btn, map_label);
    }
    ADD_ELEMENT(this->map_title);
    if(!this->ready_btn->is_loading) {
        ADD_ELEMENT(this->map_stars);
        ADD_ELEMENT(this->map_attributes);
        ADD_ELEMENT(this->map_attributes2);
    }

    // Mods
    settings_y += 10;
    ADD_ELEMENT(mods_label);
    if(is_host || bancho->room.freemods) {
        ADD_BUTTON(this->select_mods_btn, mods_label);
    }
    if(is_host) {
        this->freemod->setChecked(bancho->room.freemods);
        ADD_ELEMENT(this->freemod);
    }
    if(bancho->room.mods == 0) {
        ADD_ELEMENT(this->no_mods_selected);
    } else {
        this->mods->flags = &bancho->room.mods;
        this->mods->setSize(300, 90);
        ADD_ELEMENT(this->mods);
    }

    // Ready button
    int nb_ready = 0;
    bool is_ready = false;
    for(auto &slot : bancho->room.slots) {
        if(slot.has_player() && slot.is_ready()) {
            nb_ready++;
            if(slot.player_id == bancho->user_id) {
                is_ready = true;
            }
        }
    }
    if(is_host && is_ready && nb_ready > 1) {
        auto force_start_str = UString::format("Force start (%d/%d)", nb_ready, bancho->room.nb_players);
        if(bancho->room.all_players_ready()) {
            force_start_str = UString("Start game");
        }
        this->ready_btn->setText(force_start_str);
        this->ready_btn->setColor(0xff00ff00);
    } else {
        this->ready_btn->setText(is_ready ? "Not ready" : "Ready!");
        this->ready_btn->setColor(is_ready ? 0xffff0000 : 0xff00ff00);
    }
    ADD_ELEMENT(this->ready_btn);

    this->settings->setScrollSizeToContent();
}

void RoomScreen::updateLayout(vec2 newResolution) {
    this->setSize(newResolution);
    this->updateSettingsLayout(newResolution);

    const float dpiScale = Osu::getUIScale();
    this->pauseButton->setSize(30 * dpiScale, 30 * dpiScale);
    this->pauseButton->setPos(round(newResolution.x * 0.6) - this->pauseButton->getSize().x * 2 - 10 * dpiScale,
                              this->pauseButton->getSize().y + 10 * dpiScale);

    // XXX: Display detailed user presence
    this->slotlist->setSize(newResolution.x * 0.6 - 200, newResolution.y * 0.6 - 110);
    this->slotlist->freeElements();
    int y_total = 10;
    for(auto &slot : bancho->room.slots) {
        if(slot.has_player()) {
            auto user_info = BANCHO::User::get_user_info(slot.player_id, true);
            auto username = user_info->name;
            if(slot.is_player_playing()) {
                username = UString::format("[playing] %s", user_info->name.toUtf8());
            }

            const float SLOT_HEIGHT = 40.f;
            auto avatar = new UIAvatar(slot.player_id, 10, y_total, SLOT_HEIGHT, SLOT_HEIGHT);
            this->slotlist->getContainer()->addBaseUIElement(avatar);

            auto user_box = new UIUserLabel(slot.player_id, username);
            user_box->setFont(this->lfont);
            user_box->setPos(avatar->getRelPos().x + avatar->getSize().x + 10, y_total);

            auto color = 0xffffffff;
            if(slot.is_ready()) {
                color = 0xff00ff00;
            } else if(slot.no_map()) {
                color = 0xffdd0000;
            }
            user_box->setTextColor(color);
            user_box->setSizeToContent();
            user_box->setSize(user_box->getSize().x, SLOT_HEIGHT);
            this->slotlist->getContainer()->addBaseUIElement(user_box);

            auto user_mods = new UIModList(&slot.mods);
            user_mods->setPos(user_box->getPos().x + user_box->getSize().x + 30, y_total);
            user_mods->setSize(350, SLOT_HEIGHT);
            this->slotlist->getContainer()->addBaseUIElement(user_mods);

            y_total += SLOT_HEIGHT + 5;
        }
    }
    this->slotlist->setScrollSizeToContent();
}

// Exit to main menu
void RoomScreen::ragequit(bool play_sound) {
    this->bVisible = false;
    bancho->match_started = false;

    Packet packet;
    packet.id = EXIT_ROOM;
    BANCHO::Net::send_packet(packet);

    osu->modSelector->resetMods();
    osu->modSelector->updateButtons();

    bancho->room = Room();
    osu->mainMenu->setVisible(true);
    osu->chat->removeChannel("#multiplayer");
    osu->chat->updateVisibility();

    Replay::Mods::use(osu->previous_mods);

    if(play_sound) {
        soundEngine->play(osu->getSkin()->menuBack);
    }
}

void RoomScreen::on_map_change() {
    // Results screen has map background and such showing, so prevent map from changing while we're on it.
    if(osu->rankingScreen->isVisible()) return;

    debugLog("Map changed to ID {:d}, MD5 {:s}: {:s}\n", bancho->room.map_id, bancho->room.map_md5.hash.data(),
             bancho->room.map_name.toUtf8());
    this->ready_btn->is_loading = true;

    // Deselect current map
    this->pauseButton->setPaused(true);
    osu->songBrowser2->beatmap->deselect();

    if(bancho->room.map_id == 0) {
        this->map_title->setText("(no map selected)");
        this->map_title->setSizeToContent(0, 0);
        this->ready_btn->is_loading = true;
    } else {
        auto beatmap = db->getBeatmapDifficulty(bancho->room.map_md5);
        if(beatmap != nullptr) {
            osu->songBrowser2->onDifficultySelected(beatmap, false);
            this->map_title->setText(bancho->room.map_name);
            this->map_title->setSizeToContent(0, 0);
            auto attributes = UString::format("AR: %.1f, CS: %.1f, HP: %.1f, OD: %.1f", beatmap->getAR(),
                                              beatmap->getCS(), beatmap->getHP(), beatmap->getOD());
            this->map_attributes->setText(attributes);
            this->map_attributes->setSizeToContent(0, 0);
            auto attributes2 = UString::format("Length: %d seconds, BPM: %d (%d - %d)", beatmap->getLengthMS() / 1000,
                                               beatmap->getMostCommonBPM(), beatmap->getMinBPM(), beatmap->getMaxBPM());
            this->map_attributes2->setText(attributes2);
            this->map_attributes2->setSizeToContent(0, 0);

            // TODO @kiwec: calc actual star rating on the fly and update
            auto stars = UString::format("Star rating: %.2f*", beatmap->getStarsNomod());
            this->map_stars->setText(stars);
            this->map_stars->setSizeToContent(0, 0);
            this->ready_btn->is_loading = false;

            Packet packet;
            packet.id = MATCH_HAS_BEATMAP;
            BANCHO::Net::send_packet(packet);
        } else {
            Packet packet;
            packet.id = MATCH_NO_BEATMAP;
            BANCHO::Net::send_packet(packet);
        }
    }

    this->updateLayout(osu->getScreenSize());
}

void RoomScreen::on_room_joined(Room room) {
    bancho->room = room;
    debugLog("Joined room #{:d}\nPlayers:\n", room.id);
    for(auto &slot : room.slots) {
        if(slot.has_player()) {
            auto user_info = BANCHO::User::get_user_info(slot.player_id, true);
            debugLog("- {:s}\n", user_info->name.toUtf8());
        }
    }

    this->on_map_change();

    // Close all screens and stop any activity the player is in
    stop_spectating();
    if(osu->isInPlayMode()) {
        osu->getSelectedBeatmap()->stop(true);
    }
    osu->rankingScreen->setVisible(false);
    osu->songBrowser2->setVisible(false);
    osu->changelog->setVisible(false);
    osu->mainMenu->setVisible(false);
    osu->lobby->setVisible(false);

    this->updateLayout(osu->getScreenSize());
    this->bVisible = true;

    RichPresence::setBanchoStatus(room.name.toUtf8(), MULTIPLAYER);
    RichPresence::onMultiplayerLobby();
    osu->chat->openChannel("#multiplayer");

    osu->previous_mods = Replay::Mods::from_cvars();

    osu->modSelector->resetMods();
    osu->modSelector->enableModsFromFlags(bancho->room.mods);
}

void RoomScreen::on_room_updated(Room room) {
    if(bancho->is_playing_a_multi_map() || !bancho->is_in_a_multi_room()) return;

    if(bancho->room.nb_players < room.nb_players) {
        soundEngine->play(osu->getSkin()->roomJoined);
    } else if(bancho->room.nb_players > room.nb_players) {
        soundEngine->play(osu->getSkin()->roomQuit);
    }
    if(bancho->room.nb_ready() < room.nb_ready()) {
        soundEngine->play(osu->getSkin()->roomReady);
    } else if(bancho->room.nb_ready() > room.nb_ready()) {
        soundEngine->play(osu->getSkin()->roomNotReady);
    }
    if(!bancho->room.all_players_ready() && room.all_players_ready()) {
        soundEngine->play(osu->getSkin()->matchConfirm);
    }

    bool was_host = bancho->room.is_host();
    bool map_changed = bancho->room.map_id != room.map_id;
    bancho->room = room;

    Slot* player_slot = nullptr;
    for(auto &slot : bancho->room.slots) {
        if(slot.player_id == bancho->user_id) {
            player_slot = &slot;
            break;
        }
    }
    if(player_slot == nullptr) {
        // Player got kicked
        this->ragequit();
        return;
    }

    if(map_changed) {
        this->on_map_change();
    }

    if(!was_host && bancho->room.is_host()) {
        if(this->room_name_ipt->getText() != bancho->room.name) {
            this->room_name_ipt->setText(bancho->room.name);

            // Update room name in rich presence info
            RichPresence::onMultiplayerLobby();
        }
    }

    osu->modSelector->updateButtons();
    osu->modSelector->resetMods();
    osu->modSelector->enableModsFromFlags(bancho->room.mods | player_slot->mods);

    this->updateLayout(osu->getScreenSize());
}

void RoomScreen::on_match_started(Room room) {
    bancho->room = std::move(room);
    if(osu->getSelectedBeatmap() == nullptr) {
        debugLog("We received MATCH_STARTED without being ready, wtf!\n");
        return;
    }

    this->last_packet_tms = time(nullptr);

    if(osu->getSelectedBeatmap()->play()) {
        this->bVisible = false;
        bancho->match_started = true;
        osu->songBrowser2->bHasSelectedAndIsPlaying = true;
        osu->chat->updateVisibility();

        soundEngine->play(osu->getSkin()->matchStart);
    } else {
        this->ragequit();  // map failed to load
    }
}

void RoomScreen::on_match_score_updated(Packet *packet) {
    auto frame = BANCHO::Proto::read<ScoreFrame>(packet);
    if(frame.slot_id > 15) return;

    auto slot = &bancho->room.slots[frame.slot_id];
    slot->last_update_tms = frame.time;
    slot->num300 = frame.num300;
    slot->num100 = frame.num100;
    slot->num50 = frame.num50;
    slot->num_geki = frame.num_geki;
    slot->num_katu = frame.num_katu;
    slot->num_miss = frame.num_miss;
    slot->total_score = frame.total_score;
    slot->max_combo = frame.max_combo;
    slot->current_combo = frame.current_combo;
    slot->is_perfect = frame.is_perfect;
    slot->current_hp = frame.current_hp;
    slot->tag = frame.tag;

    bool is_scorev2 = BANCHO::Proto::read<u8>(packet);
    if(is_scorev2) {
        slot->sv2_combo = BANCHO::Proto::read<f64>(packet);
        slot->sv2_bonus = BANCHO::Proto::read<f64>(packet);
    }

    osu->hud->updateScoreboard(true);
}

void RoomScreen::on_all_players_loaded() {
    bancho->room.all_players_loaded = true;
    osu->chat->updateVisibility();
}

void RoomScreen::on_player_failed(i32 slot_id) {
    if(slot_id < 0 || slot_id > 15) return;
    bancho->room.slots[slot_id].died = true;
}

FinishedScore RoomScreen::get_approximate_score() {
    FinishedScore score;
    score.player_id = bancho->user_id;
    score.playerName = bancho->username.toUtf8();
    score.diff2 = osu->getSelectedBeatmap()->getSelectedDifficulty2();

    for(auto &i : bancho->room.slots) {
        auto slot = &i;
        if(slot->player_id != bancho->user_id) continue;

        score.mods = Replay::Mods::from_legacy(slot->mods);
        score.passed = !slot->died;
        score.unixTimestamp = slot->last_update_tms;
        score.num300s = slot->num300;
        score.num100s = slot->num100;
        score.num50s = slot->num50;
        score.numGekis = slot->num_geki;
        score.numKatus = slot->num_katu;
        score.numMisses = slot->num_miss;
        score.score = slot->total_score;
        score.comboMax = slot->max_combo;
        score.perfect = slot->is_perfect;
    }

    score.grade = score.calculate_grade();

    return score;
}

// All players have finished.
void RoomScreen::on_match_finished() {
    if(!bancho->is_playing_a_multi_map()) return;
    memcpy(bancho->last_scores, bancho->room.slots, sizeof(bancho->room.slots));

    osu->onPlayEnd(this->get_approximate_score(), false, false);

    bancho->match_started = false;
    osu->rankingScreen->setVisible(true);
    osu->chat->updateVisibility();

    // Display room presence instead of map again
    RichPresence::onMultiplayerLobby();
}

void RoomScreen::on_all_players_skipped() { bancho->room.all_players_skipped = true; }

void RoomScreen::on_player_skip(i32 user_id) {
    for(auto &slot : bancho->room.slots) {
        if(slot.player_id == user_id) {
            slot.skipped = true;
            break;
        }
    }
}

void RoomScreen::on_match_aborted() {
    if(!bancho->is_playing_a_multi_map()) return;
    osu->onPlayEnd(this->get_approximate_score(), false, true);
    this->bVisible = true;
    bancho->match_started = false;

    // Display room presence instead of map again
    RichPresence::onMultiplayerLobby();
}

void RoomScreen::onClientScoreChange(bool force) {
    if(!bancho->is_playing_a_multi_map()) return;

    // Update at most once every 250ms
    bool should_update = difftime(time(nullptr), this->last_packet_tms) > 0.25;
    if(!should_update && !force) return;

    Packet packet;
    packet.id = UPDATE_MATCH_SCORE;
    BANCHO::Proto::write<ScoreFrame>(&packet, ScoreFrame::get());
    BANCHO::Net::send_packet(packet);

    this->last_packet_tms = time(nullptr);
}

void RoomScreen::onReadyButtonClick() {
    if(this->ready_btn->is_loading) return;
    soundEngine->play(osu->getSkin()->getMenuHit());

    int nb_ready = 0;
    bool is_ready = false;
    for(auto &slot : bancho->room.slots) {
        if(slot.has_player() && slot.is_ready()) {
            nb_ready++;
            if(slot.player_id == bancho->user_id) {
                is_ready = true;
            }
        }
    }
    if(bancho->room.is_host() && is_ready && nb_ready > 1) {
        Packet packet;
        packet.id = START_MATCH;
        BANCHO::Net::send_packet(packet);
    } else {
        Packet packet;
        packet.id = is_ready ? MATCH_NOT_READY : MATCH_READY;
        BANCHO::Net::send_packet(packet);
    }
}

void RoomScreen::onSelectModsClicked() {
    soundEngine->play(osu->getSkin()->getMenuHit());
    osu->modSelector->setVisible(true);
    this->bVisible = false;
}

void RoomScreen::onSelectMapClicked() {
    if(!bancho->room.is_host()) return;

    soundEngine->play(osu->getSkin()->getMenuHit());

    Packet packet;
    packet.id = MATCH_CHANGE_SETTINGS;
    bancho->room.map_id = -1;
    bancho->room.map_name = "";
    bancho->room.map_md5 = "";
    bancho->room.pack(&packet);
    BANCHO::Net::send_packet(packet);

    osu->songBrowser2->setVisible(true);
}

void RoomScreen::onChangePasswordClicked() {
    osu->prompt->prompt("New password:", SA::MakeDelegate<&RoomScreen::set_new_password>(this));
}

void RoomScreen::onChangeWinConditionClicked() {
    this->contextMenu->setVisible(false);
    this->contextMenu->begin();
    this->contextMenu->addButton("Score V1", SCOREV1);
    this->contextMenu->addButton("Score V2", SCOREV2);
    this->contextMenu->addButton("Accuracy", ACCURACY);
    this->contextMenu->addButton("Combo", COMBO);
    this->contextMenu->end(false, false);
    this->contextMenu->setPos(mouse->getPos());
    this->contextMenu->setClickCallback(SA::MakeDelegate<&RoomScreen::onWinConditionSelected>(this));
    this->contextMenu->setVisible(true);
}

void RoomScreen::onWinConditionSelected(const UString & /*win_condition_str*/, int win_condition) {
    bancho->room.win_condition = win_condition;

    Packet packet;
    packet.id = MATCH_CHANGE_SETTINGS;
    bancho->room.pack(&packet);
    BANCHO::Net::send_packet(packet);

    this->updateLayout(osu->getScreenSize());
}

void RoomScreen::set_new_password(const UString &new_password) {
    bancho->room.has_password = new_password.length() > 0;
    bancho->room.password = new_password;

    Packet packet;
    packet.id = CHANGE_ROOM_PASSWORD;
    bancho->room.pack(&packet);
    BANCHO::Net::send_packet(packet);
}

void RoomScreen::onFreemodCheckboxChanged(CBaseUICheckbox *checkbox) {
    if(!bancho->room.is_host()) return;

    bancho->room.freemods = checkbox->isChecked();

    Packet packet;
    packet.id = MATCH_CHANGE_SETTINGS;
    bancho->room.pack(&packet);
    BANCHO::Net::send_packet(packet);

    this->on_room_updated(bancho->room);
}
