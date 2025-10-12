// Copyright (c) 2018, PG, All rights reserved.
#include "ScoreButton.h"

#include "OptionsMenu.h"
#include "SongBrowser.h"
// ---

#include <chrono>
#include <utility>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "ConVar.h"
#include "Console.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "GameRules.h"
#include "Icons.h"
#include "Keyboard.h"
#include "LeaderboardPPCalcThread.h"
#include "LegacyReplay.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"
#include "UIAvatar.h"
#include "UIContextMenu.h"
#include "UserStatsScreen.h"
#include "Logging.h"

UString ScoreButton::recentScoreIconString;

ScoreButton::ScoreButton(UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize, STYLE style)
    : CBaseUIButton(xPos, yPos, xSize, ySize, "", ""), contextMenu(contextMenu), style(style) {
    if(recentScoreIconString.length() < 1) recentScoreIconString.insert(0, Icons::ARROW_CIRCLE_UP);
}

ScoreButton::~ScoreButton() {
    anim->deleteExistingAnimation(&this->fIndexNumberAnim);
    SAFE_DELETE(this->avatar);
}

void ScoreButton::draw() {
    if(!this->bVisible) return;

    // background
    if(this->style == STYLE::SONG_BROWSER) {
        // XXX: Make it flash with song BPM
        g->setColor(Color(0xff000000).setA(0.59f * (0.5f + 0.5f * this->fIndexNumberAnim)));

        Image *backgroundImage = osu->getSkin()->getMenuButtonBackground();
        g->pushTransform();
        {
            f32 scale = SongBrowser::getUIScale2(osu->getSkin()->isMenuButtonBackground2x() ? 0.5f : 1.f);
            scale *= 0.555f;  // idk (lol, idiot kiwec)

            g->scale(scale, scale);
            g->translate(this->vPos.x, this->vPos.y + this->vSize.y / 2);
            g->drawImage(backgroundImage, AnchorPoint::LEFT);
        }
        g->popTransform();
    } else if(this->style == STYLE::TOP_RANKS) {
        g->setColor(Color(0xff666666).setA(0.59f * (0.5f + 0.5f * this->fIndexNumberAnim)));  // from 33413c to 4e7466

        g->fillRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
    }

    const int yPos = (int)this->vPos.y;  // avoid max shimmering

    // index number
    if(this->avatar) {
        const float margin = this->vSize.y * 0.1;
        f32 avatar_width = this->vSize.y - (2.f * margin);
        this->avatar->setPos(this->vPos.x + margin, this->vPos.y + margin);
        this->avatar->setSize(avatar_width, avatar_width);

        // Update avatar visibility status
        // NOTE: Not checking horizontal visibility
        auto m_scoreBrowser = osu->getSongBrowser()->scoreBrowser;
        bool is_below_top = this->avatar->getPos().y + this->avatar->getSize().y >= m_scoreBrowser->getPos().y;
        bool is_above_bottom = this->avatar->getPos().y <= m_scoreBrowser->getPos().y + m_scoreBrowser->getSize().y;
        this->avatar->on_screen = is_below_top && is_above_bottom;
        this->avatar->draw_avatar(1.f);
    }
    const float indexNumberScale = 0.35f;
    const float indexNumberWidthPercent = (this->style == STYLE::TOP_RANKS ? 0.075f : 0.15f);
    McFont *indexNumberFont = osu->getSongBrowserFontBold();
    g->pushTransform();
    {
        UString indexNumberString = UString::format("%i", this->iScoreIndexNumber);
        const float scale = (this->vSize.y / indexNumberFont->getHeight()) * indexNumberScale;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + this->vSize.x * indexNumberWidthPercent * 0.5f -
                           indexNumberFont->getStringWidth(indexNumberString) * scale / 2.0f),
                     (int)(yPos + this->vSize.y / 2.0f + indexNumberFont->getHeight() * scale / 2.0f));
        g->translate(0.5f, 0.5f);
        g->setColor(Color(0xff000000).setA(1.0f - (1.0f - this->fIndexNumberAnim)));

        g->drawString(indexNumberFont, indexNumberString);
        g->translate(-0.5f, -0.5f);
        g->setColor(Color(0xffffffff).setA(1.0f - (1.0f - this->fIndexNumberAnim) * (1.0f - this->fIndexNumberAnim)));

        g->drawString(indexNumberFont, indexNumberString);
    }
    g->popTransform();

    // grade
    const float gradeHeightPercent = 0.8f;
    SkinImage *grade = getGradeImage(this->scoreGrade);
    int gradeWidth = 0;
    g->pushTransform();
    {
        const float scale = Osu::getImageScaleToFitResolution(
            grade->getSizeBaseRaw(),
            vec2(this->vSize.x * (1.0f - indexNumberWidthPercent), this->vSize.y * gradeHeightPercent));
        gradeWidth = grade->getSizeBaseRaw().x * scale;

        g->setColor(0xffffffff);
        grade->drawRaw(vec2((int)(this->vPos.x + this->vSize.x * indexNumberWidthPercent + gradeWidth / 2.0f),
                            (int)(this->vPos.y + this->vSize.y / 2.0f)),
                       scale);
    }
    g->popTransform();

    const float gradePaddingRight = this->vSize.y * 0.165f;

    // username | (artist + songName + diffName)
    const float usernameScale = (this->style == STYLE::TOP_RANKS ? 0.6f : 0.7f);
    McFont *usernameFont = osu->getSongBrowserFont();
    g->pushClipRect(McRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y));
    g->pushTransform();
    {
        const float height = this->vSize.y * 0.5f;
        const float paddingTopPercent = (1.0f - usernameScale) * (this->style == STYLE::TOP_RANKS ? 0.15f : 0.4f);
        const float paddingTop = height * paddingTopPercent;
        const float scale = (height / usernameFont->getHeight()) * usernameScale;

        UString &string = (this->style == STYLE::TOP_RANKS ? this->sScoreTitle : this->sScoreUsername);

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + this->vSize.x * indexNumberWidthPercent + gradeWidth + gradePaddingRight),
                     (int)(yPos + height / 2.0f + usernameFont->getHeight() * scale / 2.0f + paddingTop));
        g->translate(0.75f, 0.75f);
        g->setColor(Color(0xff000000).setA(0.75f));

        g->drawString(usernameFont, string);
        g->translate(-0.75f, -0.75f);
        g->setColor(this->is_friend ? 0xffD424B0 : 0xffffffff);
        g->drawString(usernameFont, string);
    }
    g->popTransform();
    g->popClipRect();

    // score | pp | weighted 95% (pp)
    const float scoreScale = 0.5f;
    McFont *scoreFont = (this->vSize.y < 50 ? resourceManager->getFont("FONT_DEFAULT")
                                            : usernameFont);  // HACKHACK: switch font for very low resolutions
    g->pushTransform();
    {
        const float height = this->vSize.y * 0.5f;
        const float paddingBottomPercent = (1.0f - scoreScale) * (this->style == STYLE::TOP_RANKS ? 0.1f : 0.25f);
        const float paddingBottom = height * paddingBottomPercent;
        const float scale = (height / scoreFont->getHeight()) * scoreScale;

        UString &string = (this->style == STYLE::TOP_RANKS ? this->sScoreScorePPWeightedPP : this->sScoreScorePP);

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + this->vSize.x * indexNumberWidthPercent + gradeWidth + gradePaddingRight),
                     (int)(yPos + height * 1.5f + scoreFont->getHeight() * scale / 2.0f - paddingBottom));
        g->translate(0.75f, 0.75f);
        g->setColor(Color(0xff000000).setA(0.75f));

        const auto &scoreStr = (this->style == STYLE::TOP_RANKS ? string : this->sScoreScore);
        g->drawString(scoreFont, (cv::scores_sort_by_pp.getBool() ? string : scoreStr));
        g->translate(-0.75f, -0.75f);
        g->setColor((this->style == STYLE::TOP_RANKS ? 0xffdeff87 : 0xffffffff));
        g->drawString(scoreFont, (cv::scores_sort_by_pp.getBool() ? string : scoreStr));

        if(this->style == STYLE::TOP_RANKS) {
            g->translate(scoreFont->getStringWidth(string) * scale, 0);
            g->translate(0.75f, 0.75f);
            g->setColor(Color(0xff000000).setA(0.75f));

            g->drawString(scoreFont, this->sScoreScorePPWeightedWeight);
            g->translate(-0.75f, -0.75f);
            g->setColor(0xffbbbbbb);
            g->drawString(scoreFont, this->sScoreScorePPWeightedWeight);
        }
    }
    g->popTransform();

    const float rightSideThirdHeight = this->vSize.y * 0.333f;
    const float rightSidePaddingRight = (this->style == STYLE::TOP_RANKS ? 5 : this->vSize.x * 0.025f);

    // mods
    const float modScale = 0.7f;
    McFont *modFont = osu->getSubTitleFont();
    g->pushTransform();
    {
        const float height = rightSideThirdHeight;
        const float paddingTopPercent = (1.0f - modScale) * 0.45f;
        const float paddingTop = height * paddingTopPercent;
        const float scale = (height / modFont->getHeight()) * modScale;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + this->vSize.x - modFont->getStringWidth(this->sScoreMods) * scale -
                           rightSidePaddingRight),
                     (int)(yPos + height * 0.5f + modFont->getHeight() * scale / 2.0f + paddingTop));
        g->translate(0.75f, 0.75f);
        g->setColor(Color(0xff000000).setA(0.75f));

        g->drawString(modFont, this->sScoreMods);
        g->translate(-0.75f, -0.75f);
        g->setColor(0xffffffff);
        g->drawString(modFont, this->sScoreMods);
    }
    g->popTransform();

    // accuracy
    const float accScale = 0.65f;
    McFont *accFont = osu->getSubTitleFont();
    g->pushTransform();
    {
        const UString &scoreAccuracy =
            (this->style == STYLE::TOP_RANKS ? this->sScoreAccuracyFC : this->sScoreAccuracy);

        const float height = rightSideThirdHeight;
        const float paddingTopPercent = (1.0f - modScale) * 0.45f;
        const float paddingTop = height * paddingTopPercent;
        const float scale = (height / accFont->getHeight()) * accScale;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + this->vSize.x - accFont->getStringWidth(scoreAccuracy) * scale -
                           rightSidePaddingRight),
                     (int)(yPos + height * 1.5f + accFont->getHeight() * scale / 2.0f + paddingTop));
        g->translate(0.75f, 0.75f);
        g->setColor(Color(0xff000000).setA(0.75f));

        g->drawString(accFont, scoreAccuracy);
        g->translate(-0.75f, -0.75f);
        g->setColor((this->style == STYLE::TOP_RANKS ? 0xffffcc22 : 0xffffffff));
        g->drawString(accFont, scoreAccuracy);
    }
    g->popTransform();

    // custom info (Spd.)
    if(this->style == STYLE::SONG_BROWSER && this->sCustom.length() > 0) {
        const float customScale = 0.50f;
        McFont *customFont = osu->getSubTitleFont();
        g->pushTransform();
        {
            const float height = rightSideThirdHeight;
            const float paddingTopPercent = (1.0f - modScale) * 0.45f;
            const float paddingTop = height * paddingTopPercent;
            const float scale = (height / customFont->getHeight()) * customScale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + this->vSize.x - customFont->getStringWidth(this->sCustom) * scale -
                               rightSidePaddingRight),
                         (int)(yPos + height * 2.325f + customFont->getHeight() * scale / 2.0f + paddingTop));
            g->translate(0.75f, 0.75f);
            g->setColor(Color(0xff000000).setA(0.75f));

            g->drawString(customFont, this->sCustom);
            g->translate(-0.75f, -0.75f);
            g->setColor(0xffffffff);
            g->drawString(customFont, this->sCustom);
        }
        g->popTransform();
    }

    if(this->style == STYLE::TOP_RANKS) {
        // weighted percent
        const float weightScale = 0.65f;
        McFont *weightFont = osu->getSubTitleFont();
        g->pushTransform();
        {
            const float height = rightSideThirdHeight;
            const float paddingBottomPercent = (1.0f - weightScale) * 0.05f;
            const float paddingBottom = height * paddingBottomPercent;
            const float scale = (height / weightFont->getHeight()) * weightScale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + this->vSize.x - weightFont->getStringWidth(this->sScoreWeight) * scale -
                               rightSidePaddingRight),
                         (int)(yPos + height * 2.5f + weightFont->getHeight() * scale / 2.0f - paddingBottom));
            g->translate(0.75f, 0.75f);
            g->setColor(Color(0xff000000).setA(0.75f));

            g->drawString(weightFont, this->sScoreWeight);
            g->translate(-0.75f, -0.75f);
            g->setColor(0xff999999);
            g->drawString(weightFont, this->sScoreWeight);
        }
        g->popTransform();
    }

    // recent icon + elapsed time since score
    const float upIconScale = 0.35f;
    const float timeElapsedScale = accScale;
    McFont *iconFont = osu->getFontIcons();
    McFont *timeFont = accFont;
    if(this->iScoreUnixTimestamp > 0) {
        const float iconScale = (this->vSize.y / iconFont->getHeight()) * upIconScale;
        const float iconHeight = iconFont->getHeight() * iconScale;
        f32 iconPaddingLeft = 2;
        if(this->style == STYLE::TOP_RANKS) iconPaddingLeft += this->vSize.y * 0.125f;

        g->pushTransform();
        {
            g->scale(iconScale, iconScale);
            g->translate((int)(this->vPos.x + this->vSize.x + iconPaddingLeft),
                         (int)(yPos + this->vSize.y / 2 + iconHeight / 2));
            g->translate(1, 1);
            g->setColor(Color(0xff000000).setA(0.75f));

            g->drawString(iconFont, recentScoreIconString);
            g->translate(-1, -1);
            g->setColor(0xffffffff);
            g->drawString(iconFont, recentScoreIconString);
        }
        g->popTransform();

        // elapsed time since score
        if(this->sScoreTime.length() > 0) {
            const float timeHeight = rightSideThirdHeight;
            const float timeScale = (timeHeight / timeFont->getHeight()) * timeElapsedScale;
            const float timePaddingLeft = 8;

            g->pushTransform();
            {
                g->scale(timeScale, timeScale);
                g->translate((int)(this->vPos.x + this->vSize.x + iconPaddingLeft +
                                   iconFont->getStringWidth(recentScoreIconString) * iconScale + timePaddingLeft),
                             (int)(yPos + this->vSize.y / 2 + timeFont->getHeight() * timeScale / 2));
                g->translate(0.75f, 0.75f);
                g->setColor(Color(0xff000000).setA(0.85f));

                g->drawString(timeFont, this->sScoreTime);
                g->translate(-0.75f, -0.75f);
                g->setColor(0xffffffff);
                g->drawString(timeFont, this->sScoreTime);
            }
            g->popTransform();
        }
    }

    // TODO: difference to below score in list, +12345

    if(this->style == STYLE::TOP_RANKS) {
        g->setColor(0xff111111);
        g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
    }
}

void ScoreButton::mouse_update(bool *propagate_clicks) {
    // Update pp
    if(this->score.get_pp() == -1.0) {
        if(this->score.get_or_calc_pp() != -1.0) {
            // NOTE: Allows dropped sliderends. Should fix with @PPV3
            const bool fullCombo =
                (this->score.maxPossibleCombo > 0 && this->score.numMisses == 0 && this->score.numSliderBreaks == 0);

            Sync::scoped_lock lock(db->scores_mtx);
            auto &scores = this->score.is_online_score ? db->online_scores : db->scores;
            for(auto &other : scores[this->score.beatmap_hash]) {
                if(other.unixTimestamp == this->score.unixTimestamp) {
                    osu->getSongBrowser()->score_resort_scheduled = true;
                    other = this->score;
                    break;
                }
            }

            this->sScoreScorePP = UString::format(
                (this->score.perfect ? "PP: %ipp (%ix PFC)" : (fullCombo ? "PP: %ipp (%ix FC)" : "PP: %ipp (%ix)")),
                (int)std::round(this->score.get_pp()), this->score.comboMax);
        }
    }

    if(!this->bVisible) {
        return;
    }

    // dumb hack to avoid taking focus and drawing score button tooltips over options menu
    if(osu->getOptionsMenu()->isVisible()) {
        return;
    }

    if(this->avatar) {
        this->avatar->mouse_update(propagate_clicks);
        if(!*propagate_clicks) return;
    }

    CBaseUIButton::mouse_update(propagate_clicks);

    // HACKHACK: this should really be part of the UI base
    // right click detection
    if(mouse->isRightDown()) {
        if(!this->bRightClickCheck) {
            this->bRightClickCheck = true;
            this->bRightClick = this->isMouseInside();
        }
    } else {
        if(this->bRightClick) {
            if(this->isMouseInside()) this->onRightMouseUpInside();
        }

        this->bRightClickCheck = false;
        this->bRightClick = false;
    }

    // tooltip (extra stats)
    if(this->isMouseInside()) {
        if(!this->isContextMenuVisible()) {
            if(this->fIndexNumberAnim > 0.0f) {
                const auto &tooltipOverlay{osu->getTooltipOverlay()};
                tooltipOverlay->begin();
                {
                    for(const auto &tooltipLine : this->tooltipLines) {
                        if(tooltipLine.length() > 0) tooltipOverlay->addLine(tooltipLine);
                    }
                    // debug
                    if(keyboard->isShiftDown()) {
                        tooltipOverlay->addLine(fmt::format("Client: {:s}", this->score.client));
                    }
                }
                tooltipOverlay->end();
            }
        } else {
            anim->deleteExistingAnimation(&this->fIndexNumberAnim);
            this->fIndexNumberAnim = 0.0f;
        }
    }

    // update elapsed time string
    this->updateElapsedTimeString();

    // stuck anim reset
    if(!this->isMouseInside() && !anim->isAnimating(&this->fIndexNumberAnim)) this->fIndexNumberAnim = 0.0f;
}

void ScoreButton::highlight() {
    this->bIsPulseAnim = true;

    const int numPulses = 10;
    const float timescale = 1.75f;
    for(int i = 0; i < 2 * numPulses; i++) {
        if(i % 2 == 0)
            anim->moveQuadOut(&this->fIndexNumberAnim, 1.0f, 0.125f * timescale,
                              ((i / 2) * (0.125f + 0.15f)) * timescale - 0.001f, (i == 0));
        else
            anim->moveLinear(&this->fIndexNumberAnim, 0.0f, 0.15f * timescale,
                             (0.125f + (i / 2) * (0.125f + 0.15f)) * timescale - 0.001f);
    }
}

void ScoreButton::resetHighlight() {
    this->bIsPulseAnim = false;
    anim->deleteExistingAnimation(&this->fIndexNumberAnim);
    this->fIndexNumberAnim = 0.0f;
}

void ScoreButton::updateElapsedTimeString() {
    if(this->iScoreUnixTimestamp > 0) {
        const u64 curUnixTime =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        const u64 delta = curUnixTime - this->iScoreUnixTimestamp;

        const u64 deltaInSeconds = delta;
        const u64 deltaInMinutes = delta / 60;
        const u64 deltaInHours = deltaInMinutes / 60;
        const u64 deltaInDays = deltaInHours / 24;
        const u64 deltaInYears = deltaInDays / 365;

        if(deltaInHours < 96 || this->style == STYLE::TOP_RANKS) {
            if(deltaInDays > 364)
                this->sScoreTime = UString::format("%iy", (int)(deltaInYears));
            else if(deltaInHours > 47)
                this->sScoreTime = UString::format("%id", (int)(deltaInDays));
            else if(deltaInHours >= 1)
                this->sScoreTime = UString::format("%ih", (int)(deltaInHours));
            else if(deltaInMinutes > 0)
                this->sScoreTime = UString::format("%im", (int)(deltaInMinutes));
            else
                this->sScoreTime = UString::format("%is", (int)(deltaInSeconds));
        } else {
            this->iScoreUnixTimestamp = 0;

            if(this->sScoreTime.length() > 0) this->sScoreTime.clear();
        }
    }
}

void ScoreButton::onClicked(bool left, bool right) {
    soundEngine->play(osu->getSkin()->getMenuHit());
    CBaseUIButton::onClicked(left, right);
}

void ScoreButton::onMouseInside() {
    this->bIsPulseAnim = false;

    if(!this->isContextMenuVisible())
        anim->moveQuadOut(&this->fIndexNumberAnim, 1.0f, 0.125f * (1.0f - this->fIndexNumberAnim), true);
}

void ScoreButton::onMouseOutside() {
    this->bIsPulseAnim = false;

    anim->moveLinear(&this->fIndexNumberAnim, 0.0f, 0.15f * this->fIndexNumberAnim, true);
}

void ScoreButton::onFocusStolen() {
    CBaseUIButton::onFocusStolen();

    this->bRightClick = false;

    if(!this->bIsPulseAnim) {
        anim->deleteExistingAnimation(&this->fIndexNumberAnim);
        this->fIndexNumberAnim = 0.0f;
    }
}

void ScoreButton::onRightMouseUpInside() {
    const vec2 pos = mouse->getPos();

    if(this->contextMenu != nullptr) {
        this->contextMenu->setPos(pos);
        this->contextMenu->setRelPos(pos);
        this->contextMenu->begin(0, true);
        {
            if(!this->score.server.empty() && this->score.player_id != 0) {
                this->contextMenu->addButton("View Profile", 4);
            }

            this->contextMenu->addButton("Use Mods", 1);  // for scores without mods this will just nomod
            auto *replayButton = this->contextMenu->addButton("Watch replay", 2);
            if(!this->score.has_possible_replay())  // e.g. mcosu scores will never have replays
            {
                replayButton->setEnabled(false);
                replayButton->setTextColor(0xff888888);
                replayButton->setTextDarkColor(0xff000000);
                // debugLog("disallowing replay button, client: {}", this->score.client);
            }

            CBaseUIButton *spacer = this->contextMenu->addButton("---");
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);
            CBaseUIButton *deleteButton = this->contextMenu->addButton("Delete Score", 3);
            if(this->score.is_peppy_imported()) {
                // XXX: gray it out and have hover reason why user can't delete instead
                //      ...or allow delete and just store it as hidden in db
                deleteButton->setEnabled(false);
                deleteButton->setTextColor(0xff888888);
                deleteButton->setTextDarkColor(0xff000000);
            }
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&ScoreButton::onContextMenu>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    }
}

void ScoreButton::onContextMenu(const UString &text, int id) {
    if(osu->getUserStatsScreen()->isVisible()) {
        auto score = this->getScore();
        osu->getUserStatsScreen()->setVisible(false);

        auto song_button = (CarouselButton *)osu->getSongBrowser()->hashToSongButton[score.beatmap_hash];
        osu->getSongBrowser()->selectSongButton(song_button);
    }

    if(id == 1) {
        this->onUseModsClicked();
        return;
    }

    if(id == 2) {
        LegacyReplay::load_and_watch(this->score);
        return;
    }

    if(id == 3) {
        if(keyboard->isShiftDown())
            this->onDeleteScoreConfirmed(text, 1);
        else
            this->onDeleteScoreClicked();

        return;
    }

    if(id == 4) {
        auto user_url = fmt::format("https://osu.{}/u/{}", this->score.server, this->score.player_id);
        env->openURLInDefaultBrowser(user_url);
        return;
    }
}

void ScoreButton::onUseModsClicked() {
    osu->useMods(this->score);
    soundEngine->play(osu->getSkin()->getCheckOn());
}

void ScoreButton::onDeleteScoreClicked() {
    if(this->contextMenu != nullptr) {
        this->contextMenu->begin(0, true);
        {
            this->contextMenu->addButton("Really delete score?")->setEnabled(false);
            CBaseUIButton *spacer = this->contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);
            this->contextMenu->addButton("Yes", 1)->setTextLeft(false);
            this->contextMenu->addButton("No")->setTextLeft(false);
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&ScoreButton::onDeleteScoreConfirmed>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    }
}

void ScoreButton::onDeleteScoreConfirmed(const UString & /*text*/, int id) {
    if(id != 1) return;

    debugLog("Deleting score");

    // absolutely disgusting
    osu->getSongBrowser()->onScoreContextMenu(this, 2);

    osu->getUserStatsScreen()->rebuildScoreButtons();
}

void ScoreButton::setScore(const FinishedScore &score, const DatabaseBeatmap *map, int index, const UString &titleString,
                           float weight) {
    this->score = score;
    this->score.beatmap_hash = map->getMD5Hash();
    this->score.map = map;
    // debugLog(
    //     "score.beatmap_hash {} this->beatmap_hash {} score.has_possible_replay {} this->has_possible_replay {} "
    //     "score.playername {} this->playername {}",
    //     score.beatmap_hash.string(), this->score.beatmap_hash.string(), score.has_possible_replay(),
    //     this->score.has_possible_replay(), score.playerName, this->score.playerName);
    this->iScoreIndexNumber = index;

    f32 AR = score.mods.ar_override;
    f32 OD = score.mods.od_override;
    f32 HP = score.mods.hp_override;
    f32 CS = score.mods.cs_override;

    const float accuracy =
        LiveScore::calculateAccuracy(score.num300s, score.num100s, score.num50s, score.numMisses) * 100.0f;

    // NOTE: Allows dropped sliderends. Should fix with @PPV3
    const bool fullCombo = (score.maxPossibleCombo > 0 && score.numMisses == 0 && score.numSliderBreaks == 0);

    this->is_friend = false;

    SAFE_DELETE(this->avatar);
    if(score.player_id != 0) {
        this->avatar = new UIAvatar(score.player_id, this->vPos.x, this->vPos.y, this->vSize.y, this->vSize.y);

        auto user = BANCHO::User::try_get_user_info(score.player_id);
        this->is_friend = user && user->is_friend();
    }

    // display
    this->scoreGrade = score.calculate_grade();
    this->sScoreUsername = UString(score.playerName.c_str());
    this->sScoreScore = UString::format(
        (score.perfect ? "Score: %llu (%ix PFC)" : (fullCombo ? "Score: %llu (%ix FC)" : "Score: %llu (%ix)")),
        score.score, score.comboMax);

    if(score.get_pp() == -1.0) {
        this->sScoreScorePP = UString::format(
            (score.perfect ? "PP: ??? (%ix PFC)" : (fullCombo ? "PP: ??? (%ix FC)" : "PP: ??? (%ix)")), score.comboMax);
    } else {
        this->sScoreScorePP = UString::format(
            (score.perfect ? "PP: %ipp (%ix PFC)" : (fullCombo ? "PP: %ipp (%ix FC)" : "PP: %ipp (%ix)")),
            (int)std::round(score.get_pp()), score.comboMax);
    }

    this->sScoreAccuracy = UString::format("%.2f%%", accuracy);
    this->sScoreAccuracyFC =
        UString::format((score.perfect ? "PFC %.2f%%" : (fullCombo ? "FC %.2f%%" : "%.2f%%")), accuracy);
    this->sScoreMods = getModsStringForDisplay(score.mods);
    this->sCustom = (score.mods.speed != 1.0f ? UString::format("Spd: %gx", score.mods.speed) : UString(""));
    if(map != nullptr) {
        const LegacyReplay::BEATMAP_VALUES beatmapValuesForModsLegacy = LegacyReplay::getBeatmapValuesForModsLegacy(
            score.mods.to_legacy(), map->getAR(), map->getCS(), map->getOD(), map->getHP());
        if(AR == -1.f) AR = beatmapValuesForModsLegacy.AR;
        if(OD == -1.f) OD = beatmapValuesForModsLegacy.OD;
        if(HP == -1.f) HP = beatmapValuesForModsLegacy.HP;
        if(CS == -1.f) CS = beatmapValuesForModsLegacy.CS;

        // only show these values if they are not default (or default with applied mods)
        // only show these values if they are not default with applied mods

        if(beatmapValuesForModsLegacy.CS != CS) {
            if(this->sCustom.length() > 0) this->sCustom.append(", ");

            this->sCustom.append(UString::format("CS:%.4g", CS));
        }

        if(beatmapValuesForModsLegacy.AR != AR) {
            if(this->sCustom.length() > 0) this->sCustom.append(", ");

            this->sCustom.append(UString::format("AR:%.4g", AR));
        }

        if(beatmapValuesForModsLegacy.OD != OD) {
            if(this->sCustom.length() > 0) this->sCustom.append(", ");

            this->sCustom.append(UString::format("OD:%.4g", OD));
        }

        if(beatmapValuesForModsLegacy.HP != HP) {
            if(this->sCustom.length() > 0) this->sCustom.append(", ");

            this->sCustom.append(UString::format("HP:%.4g", HP));
        }
    }

    struct tm tm;
    std::time_t timestamp = score.unixTimestamp;
    localtime_x(&timestamp, &tm);

    std::array<char, 64> dateString{};
    int written = std::strftime(dateString.data(), dateString.size(), "%d-%b-%y %H:%M:%S", &tm);

    this->sScoreDateTime = UString(dateString.data(), written);
    this->iScoreUnixTimestamp = score.unixTimestamp;

    UString achievedOn = "Achieved on ";
    achievedOn.append(this->sScoreDateTime);

    // tooltip
    this->tooltipLines.clear();
    this->tooltipLines.push_back(achievedOn);

    this->tooltipLines.push_back(UString::format("300:%i 100:%i 50:%i Miss:%i SBreak:%i", score.num300s, score.num100s,
                                                 score.num50s, score.numMisses, score.numSliderBreaks));

    this->tooltipLines.push_back(UString::format("Accuracy: %.2f%%", accuracy));

    UString tooltipMods = "Mods: ";
    if(this->sScoreMods.length() > 0)
        tooltipMods.append(this->sScoreMods);
    else
        tooltipMods.append("None");

    using namespace ModMasks;
    using namespace Replay::ModFlags;

    this->tooltipLines.push_back(tooltipMods);
    if(eq(score.mods.flags, NoHP)) this->tooltipLines.emplace_back("+ no HP drain");
    if(eq(score.mods.flags, ApproachDifferent)) this->tooltipLines.emplace_back("+ approach different");
    if(eq(score.mods.flags, ARTimewarp)) this->tooltipLines.emplace_back("+ AR timewarp");
    if(eq(score.mods.flags, ARWobble)) this->tooltipLines.emplace_back("+ AR wobble");
    if(eq(score.mods.flags, FadingCursor)) this->tooltipLines.emplace_back("+ fading cursor");
    if(eq(score.mods.flags, FullAlternate)) this->tooltipLines.emplace_back("+ full alternate");
    if(eq(score.mods.flags, FPoSu_Strafing)) this->tooltipLines.emplace_back("+ FPoSu strafing");
    if(eq(score.mods.flags, FPS)) this->tooltipLines.emplace_back("+ FPS");
    if(eq(score.mods.flags, HalfWindow)) this->tooltipLines.emplace_back("+ half window");
    if(eq(score.mods.flags, Jigsaw1)) this->tooltipLines.emplace_back("+ jigsaw1");
    if(eq(score.mods.flags, Jigsaw2)) this->tooltipLines.emplace_back("+ jigsaw2");
    if(eq(score.mods.flags, Mafham)) this->tooltipLines.emplace_back("+ mafham");
    if(eq(score.mods.flags, Millhioref)) this->tooltipLines.emplace_back("+ millhioref");
    if(eq(score.mods.flags, Minimize)) this->tooltipLines.emplace_back("+ minimize");
    if(eq(score.mods.flags, Ming3012)) this->tooltipLines.emplace_back("+ ming3012");
    if(eq(score.mods.flags, MirrorHorizontal)) this->tooltipLines.emplace_back("+ mirror (horizontal)");
    if(eq(score.mods.flags, MirrorVertical)) this->tooltipLines.emplace_back("+ mirror (vertical)");
    if(eq(score.mods.flags, No50s)) this->tooltipLines.emplace_back("+ no 50s");
    if(eq(score.mods.flags, No100s)) this->tooltipLines.emplace_back("+ no 100s");
    if(eq(score.mods.flags, ReverseSliders)) this->tooltipLines.emplace_back("+ reverse sliders");
    if(eq(score.mods.flags, Timewarp)) this->tooltipLines.emplace_back("+ timewarp");
    if(eq(score.mods.flags, Shirone)) this->tooltipLines.emplace_back("+ shirone");
    if(eq(score.mods.flags, StrictTracking)) this->tooltipLines.emplace_back("+ strict tracking");
    if(eq(score.mods.flags, Wobble1)) this->tooltipLines.emplace_back("+ wobble1");
    if(eq(score.mods.flags, Wobble2)) this->tooltipLines.emplace_back("+ wobble2");

    if(this->style == STYLE::TOP_RANKS) {
        const int weightRounded = std::round(weight * 100.0f);
        const int ppWeightedRounded = std::round(score.get_pp() * weight);

        this->sScoreTitle = titleString;
        this->sScoreScorePPWeightedPP = UString::format("%ipp", (int)std::round(score.get_pp()));
        this->sScoreScorePPWeightedWeight =
            UString::format("     weighted %i%% (%ipp)", weightRounded, ppWeightedRounded);
        this->sScoreWeight = UString::format("weighted %i%%", weightRounded);

        this->tooltipLines.push_back(UString::format("Stars: %.2f (%.2f aim, %.2f speed)", score.ppv2_total_stars,
                                                     score.ppv2_aim_stars, score.ppv2_speed_stars));
        this->tooltipLines.push_back(UString::format("Speed: %.3gx", score.mods.speed));
        this->tooltipLines.push_back(UString::format("CS:%.4g AR:%.4g OD:%.4g HP:%.4g", CS, AR, OD, HP));
        this->tooltipLines.push_back(
            UString::format("Error: %.2fms - %.2fms avg", score.hitErrorAvgMin, score.hitErrorAvgMax));
        this->tooltipLines.push_back(UString::format("Unstable Rate: %.2f", score.unstableRate));
    }

    // custom
    this->updateElapsedTimeString();
}

bool ScoreButton::isContextMenuVisible() { return (this->contextMenu != nullptr && this->contextMenu->isVisible()); }

SkinImage *ScoreButton::getGradeImage(FinishedScore::Grade grade) {
    const auto &skin{osu->getSkin()};
    if(!skin) {
        debugLog("no skin to return a grade image for");
        return nullptr;
    }

    using enum FinishedScore::Grade;
    switch(grade) {
        case XH:
            return skin->getRankingXHsmall();
        case SH:
            return skin->getRankingSHsmall();
        case X:
            return skin->getRankingXsmall();
        case S:
            return skin->getRankingSsmall();
        case A:
            return skin->getRankingAsmall();
        case B:
            return skin->getRankingBsmall();
        case C:
            return skin->getRankingCsmall();
        default:
            return skin->getRankingDsmall();
    }
}

UString ScoreButton::getModsStringForDisplay(const Replay::Mods &mods) {
    using namespace ModMasks;
    using namespace Replay::ModFlags;

    UString modsString;

    if(eq(mods.flags, NoFail)) modsString.append("NF,");
    if(eq(mods.flags, Easy)) modsString.append("EZ,");
    if(eq(mods.flags, TouchDevice)) modsString.append("TD,");
    if(eq(mods.flags, Hidden)) modsString.append("HD,");
    if(eq(mods.flags, HardRock)) modsString.append("HR,");
    if(eq(mods.flags, SuddenDeath)) modsString.append("SD,");
    if(eq(mods.flags, Relax)) modsString.append("Relax,");
    if(eq(mods.flags, Flashlight)) modsString.append("FL,");
    if(eq(mods.flags, SpunOut)) modsString.append("SO,");
    if(eq(mods.flags, Autopilot)) modsString.append("AP,");
    if(eq(mods.flags, Perfect)) modsString.append("PF,");
    if(eq(mods.flags, ScoreV2)) modsString.append("v2,");
    if(eq(mods.flags, Target)) modsString.append("Target,");
    if(eq(mods.flags, Nightmare)) modsString.append("Nightmare,");
    if(eq(mods.flags, MirrorHorizontal) || eq(mods.flags, MirrorVertical)) modsString.append("Mirror,");
    if(eq(mods.flags, FPoSu)) modsString.append("FPoSu,");

    if(modsString.length() > 0) modsString.pop_back();

    return modsString;
}
