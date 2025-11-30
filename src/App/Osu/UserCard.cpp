// Copyright (c) 2018, PG, All rights reserved.
#include "UserCard.h"

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoUsers.h"
#include "OsuConVars.h"
#include "Database.h"
#include "Engine.h"
#include "Font.h"
#include "Osu.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"
#include "UIAvatar.h"
#include "UIUserContextMenu.h"
#include "UserStatsScreen.h"

// NOTE: selected username is stored in this->sText

UserCard::UserCard(i32 user_id) : CBaseUIButton() {
    this->setID(user_id);

    this->fPP = 0.0f;
    this->fAcc = 0.0f;
    this->iLevel = 0;
    this->fPercentToNextLevel = 0.0f;

    this->fPPDelta = 0.0f;
    this->fPPDeltaAnim = 0.0f;

    // We do not pass mouse events to this->avatar
    this->setClickCallback([](CBaseUIButton *btn) {
        auto card = (UserCard *)btn;
        osu->getUserActions()->open(card->user_id, card == osu->getUserButton().get());
    });
}

UserCard::~UserCard() { anim->deleteExistingAnimation(&this->fPPDeltaAnim); }

void UserCard::draw() {
    if(!this->bVisible) return;

    int yCounter = 0;
    const float iconHeight = this->vSize.y;
    const float iconWidth = iconHeight;

    // draw user icon background
    g->setColor(argb(1.0f, 0.1f, 0.1f, 0.1f));
    g->fillRect(this->vPos.x + 1, this->vPos.y + 1, iconWidth, iconHeight);

    // draw user icon
    if(this->avatar) {
        this->avatar->setPos(this->vPos.x + 1, this->vPos.y + 1);
        this->avatar->setSize(iconWidth, iconHeight);
        this->avatar->draw_avatar(1.f);
    } else {
        g->setColor(0xffffffff);
        g->pushClipRect(McRect(this->vPos.x + 1, this->vPos.y + 2, iconWidth, iconHeight));
        g->pushTransform();
        {
            const float scale =
                Osu::getImageScaleToFillResolution(osu->getSkin()->i_user_icon, vec2(iconWidth, iconHeight));
            g->scale(scale, scale);
            g->translate(this->vPos.x + iconWidth / 2 + 1, this->vPos.y + iconHeight / 2 + 1);
            g->drawImage(osu->getSkin()->i_user_icon);
        }
        g->popTransform();
        g->popClipRect();
    }

    // draw username
    McFont *usernameFont = osu->getSongBrowserFont();
    const float usernameScale = 0.4f;
    float usernamePaddingLeft = 0.0f;
    g->pushClipRect(McRect(this->vPos.x, this->vPos.y, this->vSize.x, iconHeight));
    g->pushTransform();
    {
        const float height = this->vSize.y * 0.5f;
        const float paddingTopPercent = (1.0f - usernameScale) * 0.1f;
        const float paddingTop = height * paddingTopPercent;
        const float paddingLeftPercent = (1.0f - usernameScale) * 0.3f;
        usernamePaddingLeft = height * paddingLeftPercent;
        const float scale = (height / usernameFont->getHeight()) * usernameScale;

        yCounter += (int)(this->vPos.y + usernameFont->getHeight() * scale + paddingTop);

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + iconWidth + usernamePaddingLeft), yCounter);
        g->setColor(0xffffffff);
        g->drawString(usernameFont, this->sText);
    }
    g->popTransform();
    g->popClipRect();

    if(cv::scores_enabled.getBool()) {
        // draw performance (pp), and accuracy
        McFont *performanceFont = osu->getSubTitleFont();
        const float performanceScale = 0.3f;
        g->pushTransform();
        {
            UString performanceString = UString::format("Performance:%ipp", (int)std::round(this->fPP));
            UString accuracyString = UString::format("Accuracy:%.2f%%", this->fAcc * 100.0f);

            const float height = this->vSize.y * 0.5f;
            const float paddingTopPercent = (1.0f - performanceScale) * 0.2f;
            const float paddingTop = height * paddingTopPercent;
            const float paddingMiddlePercent = (1.0f - performanceScale) * 0.15f;
            const float paddingMiddle = height * paddingMiddlePercent;
            const float scale = (height / performanceFont->getHeight()) * performanceScale;

            yCounter += performanceFont->getHeight() * scale + paddingTop;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + iconWidth + usernamePaddingLeft), yCounter);
            g->setColor(0xffffffff);
            if(cv::user_draw_pp.getBool()) g->drawString(performanceFont, performanceString);

            yCounter += performanceFont->getHeight() * scale + paddingMiddle;

            g->translate(0, performanceFont->getHeight() * scale + paddingMiddle);
            if(cv::user_draw_accuracy.getBool()) g->drawString(performanceFont, accuracyString);
        }
        g->popTransform();

        // draw level
        McFont *scoreFont = osu->getSubTitleFont();
        const float scoreScale = 0.3f;
        g->pushTransform();
        {
            UString scoreString = UString::format("Lv%i", this->iLevel);

            const float height = this->vSize.y * 0.5f;
            const float paddingTopPercent = (1.0f - scoreScale) * 0.2f;
            const float paddingTop = height * paddingTopPercent;
            const float scale = (height / scoreFont->getHeight()) * scoreScale;

            yCounter += scoreFont->getHeight() * scale + paddingTop;

            // HACK: Horizontal scaling a bit so font size is closer to stable
            g->scale(scale * 0.9f, scale);
            g->translate((int)(this->vPos.x + iconWidth + usernamePaddingLeft), yCounter);
            g->setColor(0xffffffff);
            if(cv::user_draw_level.getBool()) g->drawString(scoreFont, scoreString);
        }
        g->popTransform();

        // draw level percentage bar (to next level)
        if(cv::user_draw_level_bar.getBool()) {
            const float barBorder = 1.f;
            const float barHeight = (int)(this->vSize.y - 2 * barBorder) * 0.15f;
            const float barWidth = (int)((this->vSize.x - 2 * barBorder) * 0.61f);
            g->setColor(0xff272727);  // WYSI
            g->fillRect(this->vPos.x + this->vSize.x - barWidth - barBorder - 1,
                        this->vPos.y + this->vSize.y - barHeight - barBorder, barWidth, barHeight);
            g->setColor(0xff888888);
            g->drawRect(this->vPos.x + this->vSize.x - barWidth - barBorder - 1,
                        this->vPos.y + this->vSize.y - barHeight - barBorder, barWidth, barHeight);
            g->setColor(0xffbf962a);
            g->fillRect(this->vPos.x + this->vSize.x - barWidth - barBorder - 1,
                        this->vPos.y + this->vSize.y - barHeight - barBorder,
                        barWidth * std::clamp<float>(this->fPercentToNextLevel, 0.0f, 1.0f), barHeight);
        }

        // draw pp increase/decrease delta
        McFont *deltaFont = performanceFont;
        const float deltaScale = 0.4f;
        if(this->fPPDeltaAnim > 0.0f) {
            UString performanceDeltaString = UString::format("%.1fpp", this->fPPDelta);
            if(this->fPPDelta > 0.0f) performanceDeltaString.insert(0, L'+');

            const float border = 1.f;

            const float height = this->vSize.y * 0.5f;
            const float scale = (height / deltaFont->getHeight()) * deltaScale;

            const float performanceDeltaStringWidth = deltaFont->getStringWidth(performanceDeltaString) * scale;

            const vec2 backgroundSize =
                vec2(performanceDeltaStringWidth + border, deltaFont->getHeight() * scale + border * 3);
            const vec2 pos =
                vec2(this->vPos.x + this->vSize.x - performanceDeltaStringWidth - border, this->vPos.y + border);
            const vec2 textPos = vec2(pos.x, pos.y + deltaFont->getHeight() * scale);

            // background (to ensure readability even with stupid long usernames)
            g->setColor(argb(1.0f - (1.0f - this->fPPDeltaAnim) * (1.0f - this->fPPDeltaAnim), 0.f, 0.f, 0.f));
            g->fillRect(pos.x, pos.y, backgroundSize.x, backgroundSize.y);

            // delta text
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate((int)textPos.x, (int)textPos.y);

                g->translate(1, 1);
                g->setColor(Color(0xff000000).setA(this->fPPDeltaAnim));

                g->drawString(deltaFont, performanceDeltaString);

                g->translate(-1, -1);
                g->setColor(Color(this->fPPDelta > 0.0f ? 0xff00ff00 : 0xffff0000).setA(this->fPPDeltaAnim));

                g->drawString(deltaFont, performanceDeltaString);
            }
            g->popTransform();
        }
    }
}

void UserCard::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    if(this->user_id > 0) {
        const UserInfo *my = BANCHO::User::get_user_info(this->user_id, true);
        this->sText = my->name;

        static i64 total_score = 0;
        if(total_score != my->total_score) {
            total_score = my->total_score;
        }
    } else {
        this->sText = BanchoState::get_username().c_str();
    }

    // calculatePlayerStats() does nothing unless username changed or scores changed
    // so this should not be an expensive operation
    // calling this on each update loop allows us to just set db->bDidScoresChangeForStats = true
    // to recalculate local user stats
    this->updateUserStats();

    CBaseUIButton::mouse_update(propagate_clicks);
}

void UserCard::updateUserStats() {
    Database::PlayerStats stats;

    bool is_self = this->user_id <= 0 || (this->user_id == BanchoState::get_uid());
    if(is_self && !BanchoState::can_submit_scores()) {
        stats = db->calculatePlayerStats(this->sText.toUtf8());
    } else {
        const UserInfo *my = BANCHO::User::get_user_info(this->user_id, true);

        int level = Database::getLevelForScore(my->total_score);
        float percentToNextLevel = 1.f;
        u32 score_for_current_level = Database::getRequiredScoreForLevel(level);
        u32 score_for_next_level = Database::getRequiredScoreForLevel(level + 1);
        if(score_for_next_level > score_for_current_level) {
            percentToNextLevel = (float)(my->total_score - score_for_current_level) /
                                 (float)(score_for_next_level - score_for_current_level);
        }

        stats = Database::PlayerStats{
            .name = my->name,
            .pp = (float)my->pp,
            .accuracy = my->accuracy,
            .numScoresWithPP = 0,
            .level = level,
            .percentToNextLevel = percentToNextLevel,
            .totalScore = (u32)my->total_score,
        };
    }

    const bool changedPP = (this->fPP != stats.pp);
    const bool changed = (changedPP || this->fAcc != stats.accuracy || this->iLevel != stats.level ||
                          this->fPercentToNextLevel != stats.percentToNextLevel);

    const bool isFirstLoad = (this->fPP == 0.0f);
    const float newPPDelta = (stats.pp - this->fPP);
    if(newPPDelta != 0.0f) this->fPPDelta = newPPDelta;

    this->fPP = stats.pp;
    this->fAcc = stats.accuracy;
    this->iLevel = stats.level;
    this->fPercentToNextLevel = stats.percentToNextLevel;

    if(changed) {
        if(changedPP && !isFirstLoad && this->fPPDelta != 0.0f && this->fPP != 0.0f) {
            this->fPPDeltaAnim = 1.0f;
            anim->moveLinear(&this->fPPDeltaAnim, 0.0f, 25.0f, true);
        }
    }
}

void UserCard::setID(i32 new_id) {
    this->avatar.reset();

    this->user_id = new_id;
    if(this->user_id > 0) {
        this->avatar = std::make_unique<UIAvatar>(this->user_id, 0.f, 0.f, 0.f, 0.f);
        this->avatar->on_screen = true;
    }
}
