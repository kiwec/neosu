// Copyright (c) 2025, kiwec, All rights reserved.
#include "UserCard2.h"

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoUsers.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"
#include "Icons.h"
#include "Mouse.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"
#include "TooltipOverlay.h"
#include "UIAvatar.h"
#include "UIUserContextMenu.h"

static const u32 AVATAR_MARGIN = 4;

UserCard2::UserCard2(i32 user_id) : CBaseUIButton() {
    this->info = BANCHO::User::get_user_info(user_id);
    this->avatar = new UIAvatar(user_id, 0.f, 0.f, 0.f, 0.f);
    this->avatar->on_screen = true;
    this->setClickCallback([user_id] { osu->user_actions->open(user_id); });
}

UserCard2::~UserCard2() { SAFE_DELETE(this->avatar); }

void UserCard2::draw() {
    if(!this->bVisible) return;

    auto screen = osu->getScreenSize();
    bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);
    f32 global_scale = screen.x / (is_widescreen ? 1366.f : 1024.f);
    auto card_size = vec2{global_scale * 325, global_scale * 78};
    this->setSize(card_size);

    // position user icon
    const f32 iconHeight = this->vSize.y - AVATAR_MARGIN * 2;
    const f32 iconWidth = iconHeight;
    this->avatar->setPos(this->vPos.x + AVATAR_MARGIN, this->vPos.y + AVATAR_MARGIN);
    this->avatar->setSize(iconWidth, iconHeight);

    g->pushClipRect(McRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y));

    UString status_text = "";
    switch(this->info->action) {
        case IDLE:
            status_text = "Idle ";
            break;
        case AFK:
            status_text = "Afk ";
            break;
        case PLAYING:
            status_text = "Playing ";
            break;
        case EDITING:
            status_text = "Editing ";
            break;
        case MODDING:
            status_text = "Modding ";
            break;
        case MULTIPLAYER:
            status_text = "Multiplaying ";
            break;
        case WATCHING:
            status_text = "Watching ";
            break;
        case TESTING:
        case TESTING2:
            status_text = "Testing ";
            break;
        case SUBMITTING:
            status_text = "Submitting ";
            break;
        case PAUSED:
            status_text = "Paused ";
            break;
        case MULTIPLAYING:
            status_text = "Multiplaying ";
            break;
        default:
            break;
    }

    if(this->info->irc_user) {
        status_text = "IRC user";
    } else {
        status_text.append(this->info->info_text);
    }

    // Draw background
    // Colors from https://osu.ppy.sh/wiki/en/Client/Interface/Chat_console
    switch(this->info->action) {
        case PLAYING:
        case PAUSED:
            // Grey - Playing a beatmap in solo
            g->setColor(0xff839595);
            break;
        case WATCHING:
            // Light Blue - Watching a replay or spectating someone.
            g->setColor(0xff36368f);
            break;
        case EDITING:
            // Red - Editing their own beatmap.
            g->setColor(0xff933838);
            break;
        case TESTING:
            // Purple - Test playing a beatmap in the editor.
            g->setColor(0xff913791);
            break;
        case SUBMITTING:
            // Turquoise - Submitting (either uploading or updating) the beatmap that they have made.
            // We use a darker hue since we don't have text shadow.
            g->setColor(0xff528d6a);
            break;
        case MODDING:
            // Green - Modding or editing someone else's beatmap.
            g->setColor(0xff348a34);
            break;
        case MULTIPLAYER:
            // Brown - User is in multiplayer, but not playing.
            g->setColor(0xff8e5d18);
            break;
        case MULTIPLAYING:
            // Yellow - Currently playing in multiplayer.
            g->setColor(0xffc9ad00);
            break;
        case AFK:
            // Black - Inactive or away from keyboard (afk).
            g->setColor(0xff313131);
            break;
            g->setColor(0xff);
            break;
            g->setColor(0xff);
            break;
        case IDLE:
        case OSU_DIRECT:
        default:
            // Dark Blue - Player is currently idle or not doing anything or just chatting.
            g->setColor(0xff091b47);
            break;
    }
    g->fillRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);

    // draw user icon
    g->setColor(0xff1a1a1a);
    g->fillRect(this->vPos.x + AVATAR_MARGIN, this->vPos.y + AVATAR_MARGIN, this->avatar->getSize().x,
                this->avatar->getSize().y);
    this->avatar->draw_avatar(1.f);

    // draw username
    f32 xCounter = 0;
    f32 yCounter = 0;
    g->pushTransform();
    {
        McFont *usernameFont = osu->getSongBrowserFont();
        const f32 usernameScale = 0.4f;
        const f32 height = this->vSize.y * 0.5f;
        const f32 paddingTopPercent = (1.0f - usernameScale) * 0.1f;
        const f32 paddingTop = height * paddingTopPercent;

        /* TODO: use?
         * const f32 paddingLeftPercent = (1.0f - usernameScale) * 0.3f;
         */

        const f32 scale = height / usernameFont->getHeight() * usernameScale;

        xCounter += this->avatar->getSize().x + height * 0.2f;
        yCounter += usernameFont->getHeight() * scale + paddingTop;

        g->scale(scale, scale);
        g->translate(this->vPos.x + xCounter, this->vPos.y + yCounter);
        g->setColor(0xffffffff);
        g->drawString(usernameFont, this->info->name);
    }
    g->popTransform();

    // draw status
    const auto font = resourceManager->getFont("FONT_DEFAULT");
    const f32 status_scale = this->vSize.y * 0.5f / font->getHeight() * 0.3f;
    yCounter += font->getHeight() * status_scale * 0.1f;
    auto line_width = this->vSize.x - (xCounter + AVATAR_MARGIN);
    auto lines = font->wrap(status_text, line_width);
    for(auto &line : lines) {
        yCounter += (font->getHeight() * status_scale * 1.5);

        g->pushTransform();
        g->setColor(0xffffffff);
        g->scale(status_scale, status_scale);
        g->translate(this->vPos.x + xCounter, this->vPos.y + yCounter);
        g->drawString(font, line);
        g->popTransform();
    }

    g->popClipRect();
}
