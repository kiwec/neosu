// Copyright (c) 2016, PG, All rights reserved.
#include "InfoLabel.h"

#include <algorithm>
#include <utility>

#include "Keyboard.h"
#include "SongBrowser.h"
// ---

#include "Playfield.h"
#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "LeaderboardPPCalcThread.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "TooltipOverlay.h"

InfoLabel::InfoLabel(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), "") {
    this->font = osu->getSubTitleFont();

    this->iMargin = 8;

    this->fGlobalScale = 1.f;
    this->fTitleScale = 1.f * fGlobalScale;
    this->fSubTitleScale = 0.65f * fGlobalScale;
    this->fSongInfoScale = 0.8f * fGlobalScale;
    this->fDiffInfoScale = 0.65f * fGlobalScale;
    this->fOffsetInfoScale = 0.65f * fGlobalScale;

    this->sArtist = "Artist";
    this->sTitle = "Title";
    this->sDiff = "Difficulty";
    this->sMapper = "Mapper";

    this->iLengthMS = 0;
    this->iMinBPM = 0;
    this->iMaxBPM = 0;
    this->iMostCommonBPM = 0;
    this->iNumObjects = 0;

    this->fCS = 5.0f;
    this->fAR = 5.0f;
    this->fOD = 5.0f;
    this->fHP = 5.0f;
    this->fStars = 5.0f;

    this->iLocalOffset = 0;
    this->iOnlineOffset = 0;

    this->iBeatmapId = -1;
}

void InfoLabel::draw() {
    // debug bounding box
    if(cv::debug_osu.getBool()) {
        g->setColor(0xffff0000);
        g->drawLine(this->vPos.x, this->vPos.y, this->vPos.x + this->vSize.x, this->vPos.y);
        g->drawLine(this->vPos.x, this->vPos.y, this->vPos.x, this->vPos.y + this->vSize.y);
        g->drawLine(this->vPos.x, this->vPos.y + this->vSize.y, this->vPos.x + this->vSize.x,
                    this->vPos.y + this->vSize.y);
        g->drawLine(this->vPos.x + this->vSize.x, this->vPos.y, this->vPos.x + this->vSize.x,
                    this->vPos.y + this->vSize.y);
    }

    // build strings
    UString titleText = this->sArtist.c_str();
    titleText.append(" - ");
    titleText.append(this->sTitle.c_str());
    titleText.append(" [");
    titleText.append(this->sDiff.c_str());
    titleText.append("]");
    UString subTitleText = "Mapped by ";
    subTitleText.append(this->sMapper.c_str());
    const UString songInfoText = this->buildSongInfoString();
    const UString diffInfoText = this->buildDiffInfoString();
    const UString offsetInfoText = this->buildOffsetInfoString();

    const float globalScale = std::max((this->vSize.y / this->getMinimumHeight()) * 0.91f, 1.0f);

    const int shadowOffset = std::round(1.0f * ((float)this->font->getDPI() / 96.0f));  // NOTE: abusing font dpi

    int yCounter = this->vPos.y;

    // draw title
    g->pushTransform();
    {
        const float scale = this->fTitleScale * globalScale;

        yCounter += this->font->getHeight() * scale;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x), yCounter);

        g->translate(shadowOffset, shadowOffset);
        g->setColor(0xff000000);
        g->drawString(this->font, titleText);
        g->translate(-shadowOffset, -shadowOffset);
        g->setColor(0xffffffff);
        g->drawString(this->font, titleText);
    }
    g->popTransform();

    // draw subtitle (mapped by)
    g->pushTransform();
    {
        const float scale = this->fSubTitleScale * globalScale;

        yCounter += this->font->getHeight() * scale + (this->iMargin / 2) * globalScale * 1.0f;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x), yCounter);

        g->translate(shadowOffset, shadowOffset);
        g->setColor(0xff000000);
        g->drawString(this->font, subTitleText);
        g->translate(-shadowOffset, -shadowOffset);
        g->setColor(0xffffffff);
        g->drawString(this->font, subTitleText);
    }
    g->popTransform();

    // draw song info (length, bpm, objects)
    const Color songInfoColor =
        (osu->playfield->getSpeedMultiplier() != 1.0f
             ? (osu->playfield->getSpeedMultiplier() > 1.0f ? 0xffff7f7f : 0xffadd8e6)
             : 0xffffffff);
    g->pushTransform();
    {
        const float scale = this->fSongInfoScale * globalScale * 0.9f;

        yCounter += this->font->getHeight() * scale + (this->iMargin / 2) * globalScale * 1.0f;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x), yCounter);

        g->translate(shadowOffset, shadowOffset);
        g->setColor(0xff000000);
        g->drawString(this->font, songInfoText);
        g->translate(-shadowOffset, -shadowOffset);
        g->setColor(songInfoColor);
        g->drawString(this->font, songInfoText);
    }
    g->popTransform();

    // draw diff info (CS, AR, OD, HP, Stars)
    const Color diffInfoColor = osu->getModEZ() ? 0xffadd8e6 : (osu->getModHR() ? 0xffff7f7f : 0xffffffff);
    g->pushTransform();
    {
        const float scale = this->fDiffInfoScale * globalScale * 0.9f;

        yCounter += this->font->getHeight() * scale + this->iMargin * globalScale * 0.85f;

        g->scale(scale, scale);
        g->translate((int)(this->vPos.x), yCounter);

        g->translate(shadowOffset, shadowOffset);
        g->setColor(0xff000000);
        g->drawString(this->font, diffInfoText);
        g->translate(-shadowOffset, -shadowOffset);
        g->setColor(diffInfoColor);
        g->drawString(this->font, diffInfoText);
    }
    g->popTransform();

    // draw offset (local, online)
    if(this->iLocalOffset != 0 || this->iOnlineOffset != 0) {
        g->pushTransform();
        {
            const float scale = this->fOffsetInfoScale * globalScale * 0.8f;

            yCounter += this->font->getHeight() * scale + this->iMargin * globalScale * 0.85f;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x), yCounter);

            g->translate(shadowOffset, shadowOffset);
            g->setColor(0xff000000);
            g->drawString(this->font, offsetInfoText);
            g->translate(-shadowOffset, -shadowOffset);
            g->setColor(0xffffffff);
            g->drawString(this->font, offsetInfoText);
        }
        g->popTransform();
    }
}

void InfoLabel::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    auto screen = osu->getScreenSize();
    bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);
    this->fGlobalScale = screen.x / (is_widescreen ? 1366.f : 1024.f);
    this->fTitleScale = 1.f * fGlobalScale;
    this->fSubTitleScale = 0.65f * fGlobalScale;
    this->fSongInfoScale = 0.8f * fGlobalScale;
    this->fDiffInfoScale = 0.65f * fGlobalScale;
    this->fOffsetInfoScale = 0.65f * fGlobalScale;

    CBaseUIButton::mouse_update(propagate_clicks);

    // detail info tooltip when hovering over diff info
    if(this->isMouseInside() && !osu->getOptionsMenu()->isMouseInside()) {
        Playfield *pf = osu->playfield;

        const float speedMultiplierInv = (1.0f / pf->getSpeedMultiplier());

        const float approachTimeRoundedCompensated = ((int)pf->getApproachTime()) * speedMultiplierInv;
        const float hitWindow300RoundedCompensated = ((int)pf->getHitWindow300() - 0.5f) * speedMultiplierInv;
        const float hitWindow100RoundedCompensated = ((int)pf->getHitWindow100() - 0.5f) * speedMultiplierInv;
        const float hitWindow50RoundedCompensated = ((int)pf->getHitWindow50() - 0.5f) * speedMultiplierInv;
        const float hitobjectRadiusRoundedCompensated =
            (GameRules::getRawHitCircleDiameter(pf->getCS()) / 2.0f);

        const auto &bmDiff2{pf->getSelectedDifficulty2()};
        const auto &tooltipOverlay{osu->getTooltipOverlay()};
        tooltipOverlay->begin();
        {
            tooltipOverlay->addLine(UString::fmt("Approach time: {:.2f}ms", approachTimeRoundedCompensated));
            tooltipOverlay->addLine(UString::fmt("300: +-{:.2f}ms", hitWindow300RoundedCompensated));
            tooltipOverlay->addLine(UString::fmt("100: +-{:.2f}ms", hitWindow100RoundedCompensated));
            tooltipOverlay->addLine(UString::fmt(" 50: +-{:.2f}ms", hitWindow50RoundedCompensated));
            tooltipOverlay->addLine(
                UString::fmt("Spinner difficulty: {:.2f}", GameRules::getSpinnerSpinsPerSecond(pf)));
            tooltipOverlay->addLine(UString::fmt("Hit object radius: {:.2f}", hitobjectRadiusRoundedCompensated));

            if(bmDiff2 != nullptr) {
                int numObjects{bmDiff2->getNumObjects()};
                int numCircles{bmDiff2->getNumCircles()};
                int numSliders{bmDiff2->getNumSliders()};
                unsigned long lengthMS{bmDiff2->getLengthMS()};

                float opm{0.f}, cpm{0.f}, spm{0.f};
                if(lengthMS > 0) {
                    const float durMinutes{(static_cast<float>(lengthMS) / 1000.0f / 60.0f) /
                                           pf->getSpeedMultiplier()};

                    opm = static_cast<float>(numObjects) / durMinutes;
                    cpm = static_cast<float>(numCircles) / durMinutes;
                    spm = static_cast<float>(numSliders) / durMinutes;
                }

                tooltipOverlay->addLine(UString::fmt("Circles: {:d}, Sliders: {:d}, Spinners: {:d}", numCircles,
                                                     numSliders,
                                                     std::max(0, numObjects - numCircles - numSliders)));
                tooltipOverlay->addLine(
                    UString::fmt("OPM: {:d}, CPM: {:d}, SPM: {:d}", (int)opm, (int)cpm, (int)spm));
                tooltipOverlay->addLine(
                    UString::fmt("ID: {:d}, SetID: {:d}", bmDiff2->getID(), bmDiff2->getSetID()));
                tooltipOverlay->addLine(UString::fmt("MD5: {:s}", bmDiff2->getMD5Hash().hash.data()));
                // mostly for debugging
                if(keyboard->isShiftDown()) {
                    tooltipOverlay->addLine(UString::fmt("Title: {:s}", bmDiff2->getTitleLatin()));
                    tooltipOverlay->addLine(UString::fmt("TitleUnicode: {:s}", bmDiff2->getTitleUnicode()));
                    tooltipOverlay->addLine(UString::fmt("Artist: {:s}", bmDiff2->getArtistLatin()));
                    tooltipOverlay->addLine(UString::fmt("ArtistUnicode: {:s}", bmDiff2->getArtistUnicode()));
                }
            }
        }
        tooltipOverlay->end();
    }
}

void InfoLabel::setFromBeatmap(DatabaseBeatmap *diff2) {
    this->iBeatmapId = diff2->getID();

    this->setArtist(diff2->getArtist());
    this->setTitle(diff2->getTitle());
    this->setDiff(diff2->getDifficultyName());
    this->setMapper(diff2->getCreator());

    this->setLengthMS(diff2->getLengthMS());
    this->setBPM(diff2->getMinBPM(), diff2->getMaxBPM(), diff2->getMostCommonBPM());
    this->setNumObjects(diff2->getNumObjects());

    this->setCS(diff2->getCS());
    this->setAR(diff2->getAR());
    this->setOD(diff2->getOD());
    this->setHP(diff2->getHP());
    this->setStars(diff2->getStarsNomod());

    this->setLocalOffset(diff2->getLocalOffset());
    this->setOnlineOffset(diff2->getOnlineOffset());
}

UString InfoLabel::buildSongInfoString() {
    unsigned long lengthMS = this->iLengthMS;
    auto speed = osu->playfield->getSpeedMultiplier();

    const unsigned long fullSeconds = (lengthMS * (1.0 / speed)) / 1000.0;
    const int minutes = fullSeconds / 60;
    const int seconds = fullSeconds % 60;

    const int minBPM = this->iMinBPM * speed;
    const int maxBPM = this->iMaxBPM * speed;
    const int mostCommonBPM = this->iMostCommonBPM * speed;

    int numObjects = this->iNumObjects;
    if(this->iMinBPM == this->iMaxBPM) {
        return UString::format("Length: %02i:%02i BPM: %i Objects: %i", minutes, seconds, maxBPM, numObjects);
    } else {
        return UString::format("Length: %02i:%02i BPM: %i-%i (%i) Objects: %i", minutes, seconds, minBPM, maxBPM,
                               mostCommonBPM, numObjects);
    }
}

UString InfoLabel::buildDiffInfoString() {
    auto *pf = osu->playfield;
    auto diff2 = pf->getSelectedDifficulty2();
    if(!diff2) return "";

    bool pp_available = false;
    float CS = this->fCS;
    float AR = this->fAR;
    float OD = this->fOD;
    float HP = this->fHP;
    float stars = this->fStars;
    float modStars = 0.f;
    float modPp = 0.f;

    // pp calc for currently selected mods
    {
        lct_set_map(diff2);

        auto mods = osu->getScore()->mods;
        pp_calc_request request;
        request.mods_legacy = mods.to_legacy();
        request.speed = mods.speed;
        request.AR = mods.get_naive_ar(diff2);
        request.CS = mods.get_naive_cs(diff2);
        request.OD = mods.get_naive_od(diff2);
        request.rx = ModMasks::eq(mods.flags, Replay::ModFlags::Relax);
        request.td = ModMasks::eq(mods.flags, Replay::ModFlags::TouchDevice);
        request.comboMax = -1;
        request.numMisses = 0;
        request.num300s = diff2->getNumObjects();
        request.num100s = 0;
        request.num50s = 0;

        auto pp = lct_get_pp(request);
        if(pp.pp != -1.0) {
            modStars = pp.total_stars;
            modPp = pp.pp;
            pp_available = true;
        }

        CS = pf->getCS();
        AR = pf->getApproachRateForSpeedMultiplier();
        OD = pf->getOverallDifficultyForSpeedMultiplier();
        HP = pf->getHP();
    }

    const float starComparisonEpsilon = 0.01f;
    const bool starsAndModStarsAreEqual = (std::abs(stars - modStars) < starComparisonEpsilon);

    UString finalString;
    if(pp_available) {
        const int clampedModPp = static_cast<int>(
            std::round<int>((std::isfinite(modPp) && modPp >= static_cast<float>(std::numeric_limits<int>::min()) &&
                             modPp <= static_cast<float>(std::numeric_limits<int>::max()))
                                ? static_cast<int>(modPp)
                                : 0));
        if(starsAndModStarsAreEqual) {
            finalString = UString::format("CS:%.3g AR:%.3g OD:%.3g HP:%.3g Stars:%.3g (%ipp)", CS, AR, OD, HP, stars,
                                          clampedModPp);
        } else {
            finalString = UString::format("CS:%.3g AR:%.3g OD:%.3g HP:%.3g Stars:%.3g -> %.3g (%ipp)", CS, AR, OD, HP,
                                          stars, modStars, clampedModPp);
        }
    } else {
        finalString = UString::format("CS:%.3g AR:%.3g OD:%.3g HP:%.3g Stars:%.3g * (??? pp)", CS, AR, OD, HP, stars);
    }

    return finalString;
}

UString InfoLabel::buildOffsetInfoString() {
    return UString::format("Your Offset: %ld ms / Online Offset: %ld ms", this->iLocalOffset, this->iOnlineOffset);
}

float InfoLabel::getMinimumWidth() {
    float titleWidth = 0;
    float subTitleWidth = 0;
    float songInfoWidth = this->font->getStringWidth(this->buildSongInfoString()) * this->fSongInfoScale;
    float diffInfoWidth = this->font->getStringWidth(this->buildDiffInfoString()) * this->fDiffInfoScale;
    float offsetInfoWidth = this->font->getStringWidth(this->buildOffsetInfoString()) * this->fOffsetInfoScale;

    return std::max({titleWidth, subTitleWidth, songInfoWidth, diffInfoWidth, offsetInfoWidth});
}

float InfoLabel::getMinimumHeight() {
    f32 titleHeight = this->font->getHeight() * this->fTitleScale;
    f32 subTitleHeight = this->font->getHeight() * this->fSubTitleScale;
    f32 songInfoHeight = this->font->getHeight() * this->fSongInfoScale;
    f32 diffInfoHeight = this->font->getHeight() * this->fDiffInfoScale;
    f32 offsetInfoHeight = this->font->getHeight() * this->fOffsetInfoScale;
    return titleHeight + subTitleHeight + songInfoHeight + diffInfoHeight + offsetInfoHeight + this->iMargin * 6;
}
