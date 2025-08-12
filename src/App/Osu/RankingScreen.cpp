// Copyright (c) 2016, PG, All rights reserved.
#include "RankingScreen.h"

#include <cmath>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "Beatmap.h"
#include "CBaseUIContainer.h"
#include "CBaseUIImage.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "Chat.h"
#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "Engine.h"
#include "GameRules.h"
#include "Icons.h"
#include "Keyboard.h"
#include "LegacyReplay.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser/LeaderboardPPCalcThread.h"
#include "SongBrowser/ScoreButton.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "TooltipOverlay.h"
#include "UIButton.h"
#include "UIRankingScreenInfoLabel.h"
#include "UIRankingScreenRankingPanel.h"
#include "score.h"

class RankingScreenIndexLabel : public CBaseUILabel {
   public:
    RankingScreenIndexLabel() : CBaseUILabel(-1, 0, 0, 0, "", "You achieved the #1 score on local rankings!") {
        this->bVisible2 = false;
    }

    void draw() override {
        if(!this->bVisible || !this->bVisible2) return;

        // draw background gradient
        const Color topColor = 0xdd634e13;
        const Color bottomColor = 0xdd785f15;
        g->fillGradient(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y, topColor, topColor, bottomColor,
                        bottomColor);

        // draw ranking index text
        const float textScale = 0.45f;
        g->pushTransform();
        {
            const float scale = (this->vSize.y / this->fStringHeight) * textScale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + this->vSize.x / 2 - this->fStringWidth * scale / 2),
                         (int)(this->vPos.y + this->vSize.y / 2 + this->font->getHeight() * scale / 2));
            g->translate(1, 1);
            g->setColor(0xff000000);
            g->drawString(this->font, this->sText);
            g->translate(-1, -1);
            g->setColor(this->textColor);
            g->drawString(this->font, this->sText);
        }
        g->popTransform();
    }

    void setVisible2(bool visible2) { this->bVisible2 = visible2; }

    [[nodiscard]] inline bool isVisible2() const { return this->bVisible2; }

   private:
    bool bVisible2;
};

class RankingScreenBottomElement : public CBaseUILabel {
   public:
    RankingScreenBottomElement() : CBaseUILabel(-1, 0, 0, 0, "", "") { this->bVisible2 = false; }

    void draw() override {
        if(!this->bVisible || !this->bVisible2) return;

        // draw background gradient
        const Color topColor = 0xdd3a2e0c;
        const Color bottomColor = 0xdd493a0f;
        g->fillGradient(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y, topColor, topColor, bottomColor,
                        bottomColor);
    }

    void setVisible2(bool visible2) { this->bVisible2 = visible2; }

    [[nodiscard]] inline bool isVisible2() const { return this->bVisible2; }

   private:
    bool bVisible2;
};

class RankingScreenScrollDownInfoButton : public CBaseUIButton {
   public:
    RankingScreenScrollDownInfoButton() : CBaseUIButton(0, 0, 0, 0, "") {
        this->bVisible2 = false;
        this->fAlpha = 1.0f;
    }

    void draw() override {
        if(!this->bVisible || !this->bVisible2) return;

        const float textScale = 0.45f;
        g->pushTransform();
        {
            const float scale = (this->vSize.y / this->fStringHeight) * textScale;

            float animation = std::fmod((float)(engine->getTime() - 0.0f) * 3.2f, 2.0f);
            if(animation > 1.0f) animation = 2.0f - animation;

            animation = -animation * (animation - 2);  // quad out
            const float offset = -this->fStringHeight * scale * 0.25f + animation * this->fStringHeight * scale * 0.25f;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + this->vSize.x / 2 - this->fStringWidth * scale / 2),
                         (int)(this->vPos.y + this->vSize.y / 2 + this->fStringHeight * scale / 2 - offset));
            g->translate(2, 2);
            g->setColor(0xff000000);
            g->setAlpha(this->fAlpha);
            g->drawString(this->font, this->sText);
            g->translate(-2, -2);
            g->setColor(0xffffffff);
            g->setAlpha(this->fAlpha);
            g->drawString(this->font, this->sText);
        }
        g->popTransform();
    }

    void setAlpha(float alpha) { this->fAlpha = alpha; }

    void setVisible2(bool visible2) { this->bVisible2 = visible2; }

    [[nodiscard]] inline bool isVisible2() const { return this->bVisible2; }

    bool bVisible2;
    float fAlpha;
};

RankingScreen::RankingScreen() : ScreenBackable() {
    this->rankings = new CBaseUIScrollView(-1, 0, 0, 0, "");
    this->rankings->setHorizontalScrolling(false);
    this->rankings->setVerticalScrolling(false);
    this->rankings->setDrawFrame(false);
    this->rankings->setDrawBackground(false);
    this->rankings->setDrawScrollbars(false);
    this->addBaseUIElement(this->rankings);

    this->songInfo = new UIRankingScreenInfoLabel(5, 5, 0, 0, "");
    this->addBaseUIElement(this->songInfo);

    this->rankingTitle = new CBaseUIImage(osu->getSkin()->getRankingTitle()->getName(), 0, 0, 0, 0, "");
    this->rankingTitle->setDrawBackground(false);
    this->rankingTitle->setDrawFrame(false);
    this->addBaseUIElement(this->rankingTitle);

    this->rankingPanel = new UIRankingScreenRankingPanel();
    this->rankingPanel->setDrawBackground(false);
    this->rankingPanel->setDrawFrame(false);
    this->rankings->getContainer()->addBaseUIElement(this->rankingPanel);

    this->rankingGrade = new CBaseUIImage(osu->getSkin()->getRankingA()->getName(), 0, 0, 0, 0, "");
    this->rankingGrade->setDrawBackground(false);
    this->rankingGrade->setDrawFrame(false);
    this->rankings->getContainer()->addBaseUIElement(this->rankingGrade);

    this->rankingBottom = new RankingScreenBottomElement();
    this->rankings->getContainer()->addBaseUIElement(this->rankingBottom);

    this->rankingIndex = new RankingScreenIndexLabel();
    this->rankingIndex->setDrawFrame(false);
    this->rankingIndex->setCenterText(true);
    this->rankingIndex->setFont(osu->getSongBrowserFont());
    this->rankingIndex->setTextColor(0xffffcb21);
    this->rankings->getContainer()->addBaseUIElement(this->rankingIndex);

    this->retry_btn = new UIButton(0, 0, 0, 0, "", "Retry");
    this->retry_btn->setClickCallback(SA::MakeDelegate<&RankingScreen::onRetryClicked>(this));
    this->rankings->getContainer()->addBaseUIElement(this->retry_btn);
    this->watch_btn = new UIButton(0, 0, 0, 0, "", "Watch replay");
    this->watch_btn->setClickCallback(SA::MakeDelegate<&RankingScreen::onWatchClicked>(this));
    this->rankings->getContainer()->addBaseUIElement(this->watch_btn);

    this->setGrade(FinishedScore::Grade::D);
    this->setIndex(0);  // TEMP

    this->fUnstableRate = 0.0f;
    this->fHitErrorAvgMin = 0.0f;
    this->fHitErrorAvgMax = 0.0f;

    this->bModSS = false;
    this->bModSD = false;
    this->bModEZ = false;
    this->bModHD = false;
    this->bModHR = false;
    this->bModNightmare = false;
    this->bModScorev2 = false;
    this->bModTarget = false;
    this->bModSpunout = false;
    this->bModRelax = false;
    this->bModNF = false;
    this->bModAutopilot = false;
    this->bModAuto = false;
    this->bModTD = false;

    this->bIsUnranked = false;
}

void RankingScreen::draw() {
    if(!this->bVisible) return;

    // draw background image
    if(cv::draw_rankingscreen_background_image.getBool()) {
        SongBrowser::drawSelectedBeatmapBackgroundImage();

        // draw top black bar
        g->setColor(0xff000000);
        g->fillRect(0, 0, osu->getScreenWidth(),
                    this->rankingTitle->getSize().y * cv::rankingscreen_topbar_height_percent.getFloat());
    }

    ScreenBackable::draw();

    // draw active mods
    const Vector2 modPosStart =
        Vector2(this->rankings->getSize().x - osu->getUIScale(20), this->rankings->getRelPosY() + osu->getUIScale(260));
    Vector2 modPos = modPosStart;
    Vector2 modPosMax;
    if(this->bModTD) this->drawModImage(osu->getSkin()->getSelectionModTD(), modPos, modPosMax);
    if(this->bModSS)
        this->drawModImage(osu->getSkin()->getSelectionModPerfect(), modPos, modPosMax);
    else if(this->bModSD)
        this->drawModImage(osu->getSkin()->getSelectionModSuddenDeath(), modPos, modPosMax);
    if(this->bModEZ) this->drawModImage(osu->getSkin()->getSelectionModEasy(), modPos, modPosMax);
    if(this->bModHD) this->drawModImage(osu->getSkin()->getSelectionModHidden(), modPos, modPosMax);
    if(this->bModHR) this->drawModImage(osu->getSkin()->getSelectionModHardRock(), modPos, modPosMax);
    if(this->bModNightmare) this->drawModImage(osu->getSkin()->getSelectionModNightmare(), modPos, modPosMax);
    if(this->bModScorev2) this->drawModImage(osu->getSkin()->getSelectionModScorev2(), modPos, modPosMax);
    if(this->bModTarget) this->drawModImage(osu->getSkin()->getSelectionModTarget(), modPos, modPosMax);
    if(this->bModSpunout) this->drawModImage(osu->getSkin()->getSelectionModSpunOut(), modPos, modPosMax);
    if(this->bModRelax) this->drawModImage(osu->getSkin()->getSelectionModRelax(), modPos, modPosMax);
    if(this->bModNF) this->drawModImage(osu->getSkin()->getSelectionModNoFail(), modPos, modPosMax);
    if(this->bModAutopilot) this->drawModImage(osu->getSkin()->getSelectionModAutopilot(), modPos, modPosMax);
    if(this->bModAuto) this->drawModImage(osu->getSkin()->getSelectionModAutoplay(), modPos, modPosMax);

    // draw experimental mods
    if(this->enabledExperimentalMods.size() > 0) {
        McFont *experimentalModFont = osu->getSubTitleFont();
        const UString prefix = "+ ";

        float maxStringWidth = 0.0f;
        for(auto &enabledExperimentalMod : this->enabledExperimentalMods) {
            UString experimentalModName{enabledExperimentalMod->getName()};
            experimentalModName.insert(0, prefix);
            const float width = experimentalModFont->getStringWidth(experimentalModName);
            if(width > maxStringWidth) maxStringWidth = width;
        }

        const int backgroundMargin = 6;
        const float heightMultiplier = 1.25f;
        const int experimentalModHeight = (experimentalModFont->getHeight() * heightMultiplier);
        const Vector2 experimentalModPos = Vector2(modPosStart.x - maxStringWidth - backgroundMargin,
                                                   std::max(modPosStart.y, modPosMax.y) + osu->getUIScale(10) +
                                                       experimentalModFont->getHeight() * heightMultiplier);
        const int backgroundWidth = maxStringWidth + 2 * backgroundMargin;
        const int backgroundHeight =
            experimentalModHeight * this->enabledExperimentalMods.size() + 2 * backgroundMargin;

        g->setColor(0x77000000);
        g->fillRect((int)experimentalModPos.x - backgroundMargin,
                    (int)experimentalModPos.y - experimentalModFont->getHeight() - backgroundMargin, backgroundWidth,
                    backgroundHeight);

        g->pushTransform();
        {
            g->translate((int)experimentalModPos.x, (int)experimentalModPos.y);
            for(auto &enabledExperimentalMod : this->enabledExperimentalMods) {
                UString experimentalModName{enabledExperimentalMod->getName()};
                experimentalModName.insert(0, prefix);

                g->translate(1.5f, 1.5f);
                g->setColor(0xff000000);
                g->drawString(experimentalModFont, experimentalModName);
                g->translate(-1.5f, -1.5f);
                g->setColor(0xffffffff);
                g->drawString(experimentalModFont, experimentalModName);
                g->translate(0, experimentalModHeight);
            }
        }
        g->popTransform();
    }

    // draw pp
    if(cv::rankingscreen_pp.getBool()) {
        const UString ppString = this->getPPString();
        const Vector2 ppPos = this->getPPPosRaw();

        g->pushTransform();
        {
            g->translate((int)ppPos.x + 2, (int)ppPos.y + 2);
            g->setColor(0xff000000);
            g->drawString(osu->getTitleFont(), ppString);
            g->translate(-2, -2);
            g->setColor(0xffffffff);
            g->drawString(osu->getTitleFont(), ppString);
        }
        g->popTransform();
    }
}

void RankingScreen::drawModImage(SkinImage *image, Vector2 &pos, Vector2 &max) {
    g->setColor(0xffffffff);
    image->draw(Vector2(pos.x - image->getSize().x / 2.0f, pos.y));

    pos.x -= osu->getUIScale(20);

    if(pos.y + image->getSize().y / 2 > max.y) max.y = pos.y + image->getSize().y / 2;
}

void RankingScreen::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    ScreenBackable::mouse_update(propagate_clicks);

    if(this->score.get_pp() == -1.0) {
        pp_calc_request request;
        request.mods_legacy = this->score.mods.to_legacy();
        request.speed = this->score.mods.speed;
        request.AR = this->score.mods.get_naive_ar(this->score.diff2);
        request.OD = this->score.mods.get_naive_od(this->score.diff2);
        request.CS = this->score.diff2->getCS();
        if(this->score.mods.cs_override != -1.f) request.CS = this->score.mods.cs_override;
        request.rx = ModMasks::eq(this->score.mods.flags, Replay::ModFlags::Relax);
        request.td = ModMasks::eq(this->score.mods.flags, Replay::ModFlags::TouchDevice);
        request.comboMax = this->score.comboMax;
        request.numMisses = this->score.numMisses;
        request.num300s = this->score.num300s;
        request.num100s = this->score.num100s;
        request.num50s = this->score.num50s;

        auto info = lct_get_pp(request);
        if(info.pp != -1.0) {
            this->score.ppv2_score = info.pp;
            this->score.ppv2_version = DifficultyCalculator::PP_ALGORITHM_VERSION;
            this->score.ppv2_total_stars = info.total_stars;
            this->score.ppv2_aim_stars = info.aim_stars;
            this->score.ppv2_speed_stars = info.speed_stars;
        }
    }

    // tooltip (pp + accuracy + unstable rate)
    if(!osu->getOptionsMenu()->isMouseInside() && mouse->getPos().x < osu->getScreenWidth() * 0.5f) {
        osu->getTooltipOverlay()->begin();
        {
            osu->getTooltipOverlay()->addLine(UString::format("%.2fpp", this->score.get_pp()));
            if(this->score.ppv2_total_stars > 0.0) {
                osu->getTooltipOverlay()->addLine(
                    UString::format("Stars: %.2f (%.2f aim, %.2f speed)", this->score.ppv2_total_stars,
                                    this->score.ppv2_aim_stars, this->score.ppv2_speed_stars));
            }
            osu->getTooltipOverlay()->addLine(UString::format("Speed: %.3gx", this->score.mods.speed));
            f32 AR = this->score.mods.ar_override;
            if(AR == -1.f) {
                AR = GameRules::getRawApproachRateForSpeedMultiplier(
                    GameRules::getRawApproachTime(this->score.diff2->getAR()), this->score.mods.speed);
            }
            f32 OD = this->score.mods.od_override;
            if(OD == -1.f) {
                OD = GameRules::getRawOverallDifficultyForSpeedMultiplier(
                    GameRules::getRawHitWindow300(this->score.diff2->getOD()), this->score.mods.speed);
            }
            f32 HP = this->score.mods.hp_override;
            if(HP == -1.f) HP = this->score.diff2->getHP();
            f32 CS = this->score.mods.cs_override;
            if(CS == -1.f) CS = this->score.diff2->getCS();
            osu->getTooltipOverlay()->addLine(UString::format("CS:%.2f AR:%.2f OD:%.2f HP:%.2f", CS, AR, OD, HP));

            if(this->sMods.length() > 0) osu->getTooltipOverlay()->addLine(this->sMods);

            if(this->fUnstableRate > 0.f) {
                osu->getTooltipOverlay()->addLine("Accuracy:");
                osu->getTooltipOverlay()->addLine(
                    UString::format("Error: %.2fms - %.2fms avg", this->fHitErrorAvgMin, this->fHitErrorAvgMax));
                osu->getTooltipOverlay()->addLine(UString::format("Unstable Rate: %.2f", this->fUnstableRate));
            }
        }
        osu->getTooltipOverlay()->end();
    }
}

CBaseUIContainer *RankingScreen::setVisible(bool visible) {
    ScreenBackable::setVisible(visible);

    if(this->bVisible) {
        this->backButton->resetAnimation();
        this->rankings->scrollToY(0, false);

        this->updateLayout();
    } else {
        // Stop applause sound
        if(osu->getSkin()->getApplause() != nullptr && osu->getSkin()->getApplause()->isPlaying()) {
            soundEngine->stop(osu->getSkin()->getApplause());
        }

        if(bancho->is_in_a_multi_room()) {
            // We backed out of the ranking screen, display the room again
            osu->room->setVisible(true);
            osu->chat->updateVisibility();

            // Since we prevented on_map_change() from running while the ranking screen was visible, run it now.
            osu->room->on_map_change();
        } else {
            osu->songBrowser2->setVisible(true);
        }
    }

    return this;
}

void RankingScreen::onRetryClicked() {
    this->setVisible(false);
    if(osu->getSelectedBeatmap()->play()) {
        osu->songBrowser2->setVisible(false);
    }
}

void RankingScreen::onWatchClicked() {
    this->setVisible(false);
    LegacyReplay::load_and_watch(this->score);
}

void RankingScreen::setScore(FinishedScore score) {
    bool is_same_player = !score.playerName.compare(cv::name.getString());

    this->score = score;

    this->retry_btn->bVisible2 = is_same_player && !bancho->is_in_a_multi_room();

    if(!this->score.has_possible_replay()) {  // e.g. mcosu scores will never have replays
        this->watch_btn->setEnabled(false);
        this->watch_btn->setTextColor(0xff888888);
        this->watch_btn->setTextDarkColor(0xff000000);
    } else {
        this->watch_btn->bVisible2 = !bancho->is_in_a_multi_room();
    }

    this->bIsUnranked = false;

    char dateString[64];
    memset(dateString, '\0', 64);
    std::tm *tm = std::localtime((std::time_t *)(&score.unixTimestamp));
    std::strftime(dateString, 63, "%d-%b-%y %H:%M:%S", tm);

    this->songInfo->setDate(dateString);
    this->songInfo->setPlayer(score.playerName);

    this->rankingPanel->setScore(score);
    this->setGrade(score.calculate_grade());
    this->setIndex(-1);

    this->fUnstableRate = score.unstableRate;
    this->fHitErrorAvgMin = score.hitErrorAvgMin;
    this->fHitErrorAvgMax = score.hitErrorAvgMax;

    const UString modsString = ScoreButton::getModsStringForDisplay(score.mods);
    if(modsString.length() > 0) {
        this->sMods = "Mods: ";
        this->sMods.append(modsString);
    } else
        this->sMods = "";

    using namespace ModMasks;
    using namespace Replay::ModFlags;
    this->bModSS = eq(score.mods.flags, Perfect);
    this->bModSD = eq(score.mods.flags, SuddenDeath);
    this->bModEZ = eq(score.mods.flags, Easy);
    this->bModHD = eq(score.mods.flags, Hidden);
    this->bModHR = eq(score.mods.flags, HardRock);
    this->bModNightmare = eq(score.mods.flags, Nightmare);
    this->bModScorev2 = eq(score.mods.flags, ScoreV2);
    this->bModTarget = eq(score.mods.flags, Target);
    this->bModSpunout = eq(score.mods.flags, SpunOut);
    this->bModRelax = eq(score.mods.flags, Relax);
    this->bModNF = eq(score.mods.flags, NoFail);
    this->bModAutopilot = eq(score.mods.flags, Autopilot);
    this->bModAuto = eq(score.mods.flags, Autoplay);
    this->bModTD = eq(score.mods.flags, TouchDevice);

    this->enabledExperimentalMods.clear();
    if(eq(score.mods.flags, FPoSu_Strafing))
        this->enabledExperimentalMods.push_back(&cv::fposu_mod_strafing);
    if(eq(score.mods.flags, Wobble1)) this->enabledExperimentalMods.push_back(&cv::mod_wobble);
    if(eq(score.mods.flags, Wobble2)) this->enabledExperimentalMods.push_back(&cv::mod_wobble2);
    if(eq(score.mods.flags, ARWobble)) this->enabledExperimentalMods.push_back(&cv::mod_arwobble);
    if(eq(score.mods.flags, Timewarp)) this->enabledExperimentalMods.push_back(&cv::mod_timewarp);
    if(eq(score.mods.flags, ARTimewarp)) this->enabledExperimentalMods.push_back(&cv::mod_artimewarp);
    if(eq(score.mods.flags, Minimize)) this->enabledExperimentalMods.push_back(&cv::mod_minimize);
    if(eq(score.mods.flags, FadingCursor))
        this->enabledExperimentalMods.push_back(&cv::mod_fadingcursor);
    if(eq(score.mods.flags, FPS)) this->enabledExperimentalMods.push_back(&cv::mod_fps);
    if(eq(score.mods.flags, Jigsaw1)) this->enabledExperimentalMods.push_back(&cv::mod_jigsaw1);
    if(eq(score.mods.flags, Jigsaw2)) this->enabledExperimentalMods.push_back(&cv::mod_jigsaw2);
    if(eq(score.mods.flags, FullAlternate))
        this->enabledExperimentalMods.push_back(&cv::mod_fullalternate);
    if(eq(score.mods.flags, ReverseSliders))
        this->enabledExperimentalMods.push_back(&cv::mod_reverse_sliders);
    if(eq(score.mods.flags, No50s)) this->enabledExperimentalMods.push_back(&cv::mod_no50s);
    if(eq(score.mods.flags, No100s)) this->enabledExperimentalMods.push_back(&cv::mod_no100s);
    if(eq(score.mods.flags, Ming3012)) this->enabledExperimentalMods.push_back(&cv::mod_ming3012);
    if(eq(score.mods.flags, HalfWindow)) this->enabledExperimentalMods.push_back(&cv::mod_halfwindow);
    if(eq(score.mods.flags, Millhioref)) this->enabledExperimentalMods.push_back(&cv::mod_millhioref);
    if(eq(score.mods.flags, Mafham)) this->enabledExperimentalMods.push_back(&cv::mod_mafham);
    if(eq(score.mods.flags, StrictTracking))
        this->enabledExperimentalMods.push_back(&cv::mod_strict_tracking);
    if(eq(score.mods.flags, MirrorHorizontal))
        this->enabledExperimentalMods.push_back(&cv::playfield_mirror_horizontal);
    if(eq(score.mods.flags, MirrorVertical))
        this->enabledExperimentalMods.push_back(&cv::playfield_mirror_vertical);
    if(eq(score.mods.flags, Shirone)) this->enabledExperimentalMods.push_back(&cv::mod_shirone);
    if(eq(score.mods.flags, ApproachDifferent))
        this->enabledExperimentalMods.push_back(&cv::mod_approach_different);
}

void RankingScreen::setBeatmapInfo(Beatmap *beatmap, DatabaseBeatmap *diff2) {
    this->score.diff2 = diff2;
    this->songInfo->setFromBeatmap(beatmap, diff2);

    const std::string scorePlayer = this->score.playerName.empty() ? cv::name.getString() : this->score.playerName;
    this->songInfo->setPlayer(this->bIsUnranked ? "neosu" : scorePlayer);

    // @PPV3: update m_score.ppv3_score, this->score.ppv3_aim_stars, this->score.ppv3_speed_stars,
    //        m_fHitErrorAvgMin, this->fHitErrorAvgMax, this->fUnstableRate
}

void RankingScreen::updateLayout() {
    ScreenBackable::updateLayout();

    const float uiScale = cv::ui_scale.getFloat();

    this->setSize(osu->getScreenSize());

    this->rankingTitle->setImage(osu->getSkin()->getRankingTitle());
    this->rankingTitle->setScale(Osu::getImageScale(this->rankingTitle->getImage(), 75.0f) * uiScale,
                                 Osu::getImageScale(this->rankingTitle->getImage(), 75.0f) * uiScale);
    this->rankingTitle->setSize(this->rankingTitle->getImage()->getWidth() * this->rankingTitle->getScale().x,
                                this->rankingTitle->getImage()->getHeight() * this->rankingTitle->getScale().y);
    this->rankingTitle->setRelPos(this->getSize().x - this->rankingTitle->getSize().x - osu->getUIScale(20.0f), 0);

    this->songInfo->setSize(osu->getScreenWidth(), std::max(this->songInfo->getMinimumHeight(),
                                                            this->rankingTitle->getSize().y *
                                                                cv::rankingscreen_topbar_height_percent.getFloat()));

    this->rankings->setSize(osu->getScreenSize().x + 2, osu->getScreenSize().y - this->songInfo->getSize().y + 3);
    this->rankings->setRelPosY(this->songInfo->getSize().y - 1);

    float btn_width = 150.f * uiScale;
    float btn_height = 50.f * uiScale;
    this->retry_btn->setSize(btn_width, btn_height);
    this->watch_btn->setSize(btn_width, btn_height);
    Vector2 btn_pos(this->rankings->getSize().x * 0.98f - btn_width, this->rankings->getSize().y * 0.90f - btn_height);
    this->watch_btn->setRelPos(btn_pos);
    btn_pos.y -= btn_height + 5.f * uiScale;
    this->retry_btn->setRelPos(btn_pos);

    this->update_pos();

    // NOTE: no uiScale for rankingPanel and rankingGrade, doesn't really work due to legacy layout expectations
    const Vector2 hardcodedOsuRankingPanelImageSize =
        Vector2(622, 505) * (osu->getSkin()->isRankingPanel2x() ? 2.0f : 1.0f);
    this->rankingPanel->setImage(osu->getSkin()->getRankingPanel());
    this->rankingPanel->setScale(Osu::getImageScale(hardcodedOsuRankingPanelImageSize, 317.0f),
                                 Osu::getImageScale(hardcodedOsuRankingPanelImageSize, 317.0f));
    this->rankingPanel->setSize(
        std::max(hardcodedOsuRankingPanelImageSize.x * this->rankingPanel->getScale().x,
                 this->rankingPanel->getImage()->getWidth() * this->rankingPanel->getScale().x),
        std::max(hardcodedOsuRankingPanelImageSize.y * this->rankingPanel->getScale().y,
                 this->rankingPanel->getImage()->getHeight() * this->rankingPanel->getScale().y));

    this->rankingIndex->setSize(this->rankings->getSize().x + 2, osu->getScreenHeight() * 0.07f * uiScale);
    this->rankingIndex->setBackgroundColor(0xff745e13);
    this->rankingIndex->setRelPosY(this->rankings->getSize().y + 1);

    this->rankingBottom->setSize(this->rankings->getSize().x + 2, osu->getScreenHeight() * 0.2f);
    this->rankingBottom->setRelPosY(this->rankingIndex->getRelPos().y + this->rankingIndex->getSize().y);

    this->setGrade(this->grade);

    this->update_pos();
    this->rankings->getContainer()->update_pos();
    this->rankings->setScrollSizeToContent(0);
}

void RankingScreen::onBack() { this->setVisible(false); }

void RankingScreen::setGrade(FinishedScore::Grade grade) {
    this->grade = grade;

    Vector2 hardcodedOsuRankingGradeImageSize = Vector2(369, 422);
    switch(grade) {
        case FinishedScore::Grade::XH:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingXH2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingXH());
            break;
        case FinishedScore::Grade::SH:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingSH2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingSH());
            break;
        case FinishedScore::Grade::X:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingX2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingX());
            break;
        case FinishedScore::Grade::S:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingS2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingS());
            break;
        case FinishedScore::Grade::A:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingA2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingA());
            break;
        case FinishedScore::Grade::B:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingB2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingB());
            break;
        case FinishedScore::Grade::C:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingC2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingC());
            break;
        default:
            hardcodedOsuRankingGradeImageSize *= (osu->getSkin()->isRankingD2x() ? 2.0f : 1.0f);
            this->rankingGrade->setImage(osu->getSkin()->getRankingD());
            break;
    }

    const float uiScale = /*cv::ui_scale.getFloat()*/ 1.0f;  // NOTE: no uiScale for rankingPanel and rankingGrade,
                                                             // doesn't really work due to legacy layout expectations

    const float rankingGradeImageScale = Osu::getImageScale(hardcodedOsuRankingGradeImageSize, 230.0f) * uiScale;
    this->rankingGrade->setScale(rankingGradeImageScale, rankingGradeImageScale);
    this->rankingGrade->setSize(this->rankingGrade->getImage()->getWidth() * this->rankingGrade->getScale().x,
                                this->rankingGrade->getImage()->getHeight() * this->rankingGrade->getScale().y);
    this->rankingGrade->setRelPos(
        this->rankings->getSize().x - osu->getUIScale(120) -
            this->rankingGrade->getImage()->getWidth() * this->rankingGrade->getScale().x / 2.0f,
        -this->rankings->getRelPos().y + osu->getUIScale(osu->getSkin()->getVersion() > 1.0f ? 200 : 170) -
            this->rankingGrade->getImage()->getHeight() * this->rankingGrade->getScale().x / 2.0f);
}

void RankingScreen::setIndex(int index) {
    if(!cv::scores_enabled.getBool()) index = -1;

    if(index > -1) {
        this->rankingIndex->setText(UString::format("You achieved the #%i score on local rankings!", (index + 1)));
        this->rankingIndex->setVisible2(true);
        this->rankingBottom->setVisible2(true);
    } else {
        this->rankingIndex->setVisible2(false);
        this->rankingBottom->setVisible2(false);
    }
}

UString RankingScreen::getPPString() {
    f32 pp = this->score.get_pp();
    if(pp == -1.0) {
        return UString("??? pp");
    } else {
        return UString::format("%ipp", (int)(std::round(pp)));
    }
}

Vector2 RankingScreen::getPPPosRaw() {
    const UString ppString = this->getPPString();
    float ppStringWidth = osu->getTitleFont()->getStringWidth(ppString);
    return Vector2(this->rankingGrade->getPos().x, cv::ui_scale.getFloat() * 10.f) +
           Vector2(this->rankingGrade->getSize().x / 2 - (ppStringWidth / 2 + cv::ui_scale.getFloat() * 100.f),
                   this->rankings->getRelPosY() + osu->getUIScale(400) + osu->getTitleFont()->getHeight() / 2);
}
