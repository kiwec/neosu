// Copyright (c) 2024, kiwec, All rights reserved.
#include "ChatLink.h"

#include <codecvt>
#include <regex>
#include <utility>

#include "Bancho.h"
#include "Lobby.h"
#include "MainMenu.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "Environment.h"
#include "Parsing.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "TooltipOverlay.h"
#include "UI.h"
#include "UIUserContextMenu.h"

ChatLink::ChatLink(float xPos, float yPos, float xSize, float ySize, const UString& link, const UString& label)
    : CBaseUILabel(xPos, yPos, xSize, ySize, link, label) {
    this->link = link;
    this->setDrawFrame(false);
    this->setDrawBackground(true);
    this->setBackgroundColor(0xff2e3784);
}

void ChatLink::update() {
    CBaseUILabel::update();

    if(this->isMouseInside()) {
        ui->getTooltipOverlay()->begin();
        ui->getTooltipOverlay()->addLine(fmt::format("link: {}", this->link.toUtf8()));
        ui->getTooltipOverlay()->end();

        this->setBackgroundColor(0xff3d48ac);
    } else {
        this->setBackgroundColor(0xff2e3784);
    }
}

void ChatLink::open_beatmap_link(i32 map_id, i32 set_id) {
    if(ui->getSongBrowser()->isVisible()) {
        ui->getSongBrowser()->map_autodl = map_id;
        ui->getSongBrowser()->set_autodl = set_id;
    } else if(ui->getMainMenu()->isVisible()) {
        ui->setScreen(ui->getSongBrowser());
        ui->getSongBrowser()->map_autodl = map_id;
        ui->getSongBrowser()->set_autodl = set_id;
    } else {
        env->openURLInDefaultBrowser(this->link.toUtf8());
    }
}

void ChatLink::onMouseUpInside(bool /*left*/, bool /*right*/) {
    std::string link_str = this->link.toUtf8();
    std::smatch match;

    // This lazy escaping is only good for endpoint URLs, not anything more serious
    UString escaped_endpoint;
    for(int i = 0; i < BanchoState::endpoint.length(); i++) {
        if(BanchoState::endpoint[i] == L'.') {
            escaped_endpoint.append("\\.");
        } else {
            escaped_endpoint.append(BanchoState::endpoint[i]);
        }
    }

    // Detect multiplayer invite links
    if(this->link.startsWith("osump://")) {
        if(ui->getRoom()->isVisible()) {
            ui->getNotificationOverlay()->addNotification("You are already in a multiplayer room.");
            return;
        }

        // If the password has a space in it, parsing will break, but there's no way around it...
        // osu!stable also considers anything after a space to be part of the lobby title :(
        std::regex_search(link_str, match, std::regex(R"(osump://(\d+)/(\S*))"));
        u32 invite_id = Parsing::strto<u32>(match.str(1));
        UString password = match.str(2).c_str();
        ui->getLobby()->joinRoom(invite_id, password);
        return;
    }

    // Detect user links
    // https:\/\/(osu\.)?akatsuki\.gg\/u(sers)?\/(\d+)
    UString user_pattern = US_(R"(https?://(osu\.)?)");
    user_pattern.append(escaped_endpoint);
    user_pattern.append(US_(R"(/u(sers)?/(\d+))"));
    if(std::regex_search(link_str, match, std::regex(user_pattern.toUtf8()))) {
        i32 user_id = Parsing::strto<i32>(match.str(3));
        ui->getUserActions()->open(user_id);
        return;
    }

    // Detect beatmap links
    // https:\/\/((osu\.)?akatsuki\.gg|osu\.ppy\.sh)\/b(eatmaps)?\/(\d+)
    UString map_pattern = US_(R"(https?://((osu\.)?)");
    map_pattern.append(escaped_endpoint);
    map_pattern.append(US_(R"(|osu\.ppy\.sh)/b(eatmaps)?/(\d+))"));
    if(std::regex_search(link_str, match, std::regex(map_pattern.toUtf8()))) {
        i32 map_id = Parsing::strto<i32>(match.str(4));
        this->open_beatmap_link(map_id, 0);
        return;
    }

    // Detect beatmapset links
    // https:\/\/((osu\.)?akatsuki\.gg|osu\.ppy\.sh)\/beatmapsets\/(\d+)(#osu\/(\d+))?
    UString set_pattern = US_(R"(https?://((osu\.)?)");
    set_pattern.append(escaped_endpoint);
    set_pattern.append(US_(R"(|osu\.ppy\.sh)/beatmapsets/(\d+)(#osu/(\d+))?)"));
    if(std::regex_search(link_str, match, std::regex(set_pattern.toUtf8()))) {
        i32 set_id = Parsing::strto<i32>(match.str(3));
        i32 map_id = 0;
        if(match[5].matched) {
            map_id = Parsing::strto<i32>(match.str(5));
        }

        this->open_beatmap_link(map_id, set_id);
        return;
    }

    env->openURLInDefaultBrowser(this->link.toUtf8());
}
