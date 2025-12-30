// Copyright (c) 2024, kiwec, All rights reserved.
#include "ScoreboardSlot.h"

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoUsers.h"
#include "OsuConVars.h"
#include "Engine.h"
#include "Font.h"
#include "Osu.h"
#include "Skin.h"
#include "SkinImage.h"

ScoreboardSlot::ScoreboardSlot(const SCORE_ENTRY &score, int index) {
    this->avatar = std::make_unique<UIAvatar>(score.player_id, 0.f, 0.f, 0.f, 0.f);
    this->score = score;
    this->index = index;

    const UserInfo *user = BANCHO::User::try_get_user_info(score.player_id);
    this->is_friend = user && user->is_friend();
}

ScoreboardSlot::~ScoreboardSlot() {
    anim::deleteExistingAnimation(&this->fAlpha);
    anim::deleteExistingAnimation(&this->fFlash);
    anim::deleteExistingAnimation(&this->y);
}

void ScoreboardSlot::draw() {
    if(this->fAlpha == 0.f) return;

    g->pushTransform();

    g->setBlendMode(DrawBlendMode::BLEND_MODE_PREMUL_ALPHA);

    const float height = roundf(osu->getVirtScreenHeight() * 0.07f);
    const float width = roundf(height * 2.6f);  // does not include avatar_width
    const float avatar_height = height;
    const float avatar_width = avatar_height;
    const float padding = roundf(height * 0.05f);

    float start_y = osu->getVirtScreenHeight() / 2.0f - (height * 2.5f);
    start_y += this->y * height;
    start_y = roundf(start_y);

    if(this->fFlash > 0.f && !cv::avoid_flashes.getBool()) {
        g->setColor(Color(0xffffffff).setA(this->fFlash));

        g->fillRect(0, start_y, avatar_width + width, height);
    }

    // Draw background
    if(this->score.dead) {
        g->setColor(0xff660000);
    } else if(this->score.highlight) {
        g->setColor(0xff777777);
    } else if(this->index == 0) {
        g->setColor(0xff1b6a8c);
    } else {
        g->setColor(this->is_friend ? 0xff9C205C : 0xff114459);
    }
    g->setAlpha(0.3f * this->fAlpha);

    if(cv::hud_scoreboard_use_menubuttonbackground.getBool()) {
        // XXX: Doesn't work on resolutions more vertical than 4:3
        float bg_scale = 0.625f;
        auto bg_img = osu->getSkin()->i_menu_button_bg2;
        float oScale = bg_img->getResolutionScale() * 0.99f;
        g->fillRect(0, start_y, avatar_width, height);
        bg_img->draw(vec2(avatar_width + (bg_img->getSizeBase().x / 2) * bg_scale - (470 * oScale) * bg_scale,
                          start_y + height / 2),
                     bg_scale);
    } else {
        g->fillRect(0, start_y, avatar_width + width, height);
    }

    // Draw avatar
    this->avatar->on_screen = true;
    this->avatar->setPos(0, start_y);
    this->avatar->setSize(avatar_width, avatar_height);
    this->avatar->setVisible(true);
    this->avatar->draw_avatar(0.8f * this->fAlpha);

    // Draw index
    g->pushTransform();
    {
        McFont *indexFont = osu->getSongBrowserFontBold();
        UString indexString = UString::format("%i", this->index + 1);
        const float scale = (avatar_height / indexFont->getHeight()) * 0.5f;

        g->scale(scale, scale);

        // * 0.9f because the returned font height isn't accurate :c
        g->translate(avatar_width / 2.0f - (indexFont->getStringWidth(indexString) * scale / 2.0f),
                     start_y + (avatar_height / 2.0f) + indexFont->getHeight() * scale / 2.0f * 0.9f);

        g->translate(0.5f, 0.5f);
        g->setColor(Color(0xff000000).setA(0.3f * this->fAlpha));

        g->drawString(indexFont, indexString);

        g->translate(-0.5f, -0.5f);
        g->setColor(Color(0xffffffff).setA(0.7f * this->fAlpha));

        g->drawString(indexFont, indexString);
    }
    g->popTransform();

    // Draw name
    const bool drawTextShadow = (cv::background_dim.getFloat() < 0.7f);
    const Color textShadowColor = 0x66000000;
    const float nameScale = 0.315f;
    g->pushTransform();
    {
        McFont *nameFont = osu->getSongBrowserFont();
        g->pushClipRect(McRect(avatar_width, start_y, width, height));

        const float scale = (height / nameFont->getHeight()) * nameScale;
        g->scale(scale, scale);
        g->translate(avatar_width + padding, start_y + padding + nameFont->getHeight() * scale);
        if(drawTextShadow) {
            g->translate(1, 1);
            g->setColor(Color(textShadowColor).setA(this->fAlpha));

            g->drawString(nameFont, this->score.name);
            g->translate(-1, -1);
        }

        if(this->score.dead) {
            g->setColor(0xffee0000);
        } else if(this->score.highlight) {
            g->setColor(0xffffffff);
        } else if(this->index == 0) {
            g->setColor(0xffeeeeee);
        } else {
            g->setColor(0xffaaaaaa);
        }

        g->setAlpha(this->fAlpha);
        g->drawString(nameFont, this->score.name);
        g->popClipRect();
    }
    g->popTransform();

    // Draw combo
    const Color comboAccuracyColor = 0xff5d9ca1;
    const Color comboAccuracyColorHighlight = 0xff99fafe;
    const Color comboAccuracyColorTop = 0xff84dbe0;
    McFont *scoreFont = osu->getSongBrowserFont();
    const f32 comboScale = 0.26f;
    const f32 scoreScale = (height / scoreFont->getHeight()) * comboScale;

    auto draw_score = [&](const UString &text) {
        g->pushTransform();
        {
            g->scale(scoreScale, scoreScale);
            g->translate(avatar_width + padding * 1.35f, start_y + height - 2 * padding);
            if(drawTextShadow) {
                g->translate(1, 1);
                g->setColor(Color(textShadowColor).setA(this->fAlpha));
                g->drawString(scoreFont, text);
                g->translate(-1, -1);
            }

            if(this->score.dead) {
                g->setColor(0xffee0000);
            } else if(this->score.highlight) {
                g->setColor(0xffffffff);
            } else if(this->index == 0) {
                g->setColor(0xffeeeeee);
            } else {
                g->setColor(0xffaaaaaa);
            }

            g->setAlpha(this->fAlpha);
            g->drawString(scoreFont, text);
        }
        g->popTransform();
    };

    // draw combo
    g->pushTransform();
    {
        UString comboString = UString::format("%ix", this->score.maxCombo);
        const float stringWidth = scoreFont->getStringWidth(comboString);

        g->scale(scoreScale, scoreScale);
        g->translate(avatar_width + width - stringWidth * scoreScale - padding * 1.35f, start_y + height - 2 * padding);
        if(drawTextShadow) {
            g->translate(1, 1);
            g->setColor(Color(textShadowColor).setA(this->fAlpha));

            g->drawString(scoreFont, comboString);
            g->translate(-1, -1);
        }

        if(this->score.highlight) {
            g->setColor(comboAccuracyColorHighlight);
        } else if(this->index == 0) {
            g->setColor(comboAccuracyColorTop);
        } else {
            g->setColor(comboAccuracyColor);
        }

        g->setAlpha(this->fAlpha);
        g->drawString(scoreFont, comboString);
    }
    g->popTransform();

    if(osu->getHUD()->getScoringMetric() == ACCURACY) {
        // draw accuracy
        g->pushTransform();
        {
            UString accString = UString::format("%.2f%%", this->score.accuracy * 100.0f);
            g->scale(scoreScale, scoreScale);
            g->translate(avatar_width + padding * 1.35f, start_y + height - 2 * padding);
            {
                g->translate(1, 1);
                g->setColor(Color(textShadowColor).setA(this->fAlpha));

                g->drawString(scoreFont, accString);
                g->translate(-1, -1);
            }

            if(this->score.highlight) {
                g->setColor(comboAccuracyColorHighlight);
            } else if(this->index == 0) {
                g->setColor(comboAccuracyColorTop);
            } else {
                g->setColor(comboAccuracyColor);
            }

            g->setAlpha(this->fAlpha);
            g->drawString(scoreFont, accString);
        }

        g->popTransform();
    } else if(osu->getHUD()->getScoringMetric() == MISSES) {
        // draw misses
        draw_score(fmt::format("{} misses", this->score.misses));
    } else if(osu->getHUD()->getScoringMetric() == PP) {
        // draw pp
        draw_score(UString::format("%.2fpp", this->score.pp));
    } else {
        // draw score
        draw_score(fmt::format("{}", this->score.score));
    }

    g->setBlendMode(DrawBlendMode::BLEND_MODE_ALPHA);

    g->popTransform();
}

void ScoreboardSlot::updateIndex(int new_index, bool is_player, bool animate) {
    int player_idx = osu->getHUD()->player_slot->index;
    if(is_player) {
        if(animate && new_index < this->index) {
            this->fFlash = 1.f;
            anim::moveQuartOut(&this->fFlash, 0.0f, 0.5f, 0.0f, true);
        }

        // Ensure the player is always visible
        player_idx = new_index;
    }

    int min_visible_idx = player_idx - 4;
    if(min_visible_idx < 0) min_visible_idx = 0;

    int max_visible_idx = player_idx;
    if(max_visible_idx < 5) max_visible_idx = 5;

    bool is_visible = new_index == 0 || (new_index >= min_visible_idx && new_index <= max_visible_idx);

    float scoreboard_y = 0;
    if(min_visible_idx == 0) {
        scoreboard_y = new_index;
    } else if(new_index > 0) {
        scoreboard_y = (new_index + 1) - min_visible_idx;
    }

    if(this->was_visible && !is_visible) {
        if(animate) {
            anim::moveQuartOut(&this->y, scoreboard_y, 0.5f, 0.0f, true);
            anim::moveQuartOut(&this->fAlpha, 0.0f, 0.5f, 0.0f, true);
        } else {
            this->y = scoreboard_y;
            this->fAlpha = 0.0f;
        }
        this->was_visible = false;
    } else if(!this->was_visible && is_visible) {
        anim::deleteExistingAnimation(&this->y);
        this->y = scoreboard_y;
        if(animate) {
            this->fAlpha = 0.f;
            anim::moveQuartOut(&this->fAlpha, 1.0f, 0.5f, 0.0f, true);
        } else {
            this->fAlpha = 1.0f;
        }
        this->was_visible = true;
    } else if(this->was_visible || is_visible) {
        if(animate) {
            anim::moveQuartOut(&this->y, scoreboard_y, 0.5f, 0.0f, true);
        } else {
            this->y = scoreboard_y;
        }
    }

    this->index = new_index;
}
