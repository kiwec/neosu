// Copyright (c) 2016, PG, All rights reserved.
#include "InfoLabel.h"

#include <algorithm>
#include <utility>

#include "Keyboard.h"
#include "SongBrowser.h"
// ---

#include "BeatmapInterface.h"
#include "OsuConVars.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "AsyncPPCalculator.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Font.h"
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
    const Color songInfoColor = (osu->getMapInterface()->getSpeedMultiplier() != 1.0f
                                     ? (osu->getMapInterface()->getSpeedMultiplier() > 1.0f ? 0xffff7f7f : 0xffadd8e6)
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

    auto screen = osu->getVirtScreenSize();
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
        const auto &pf = osu->getMapInterface();

        const float speedMultiplierInv = (1.0f / pf->getSpeedMultiplier());

        const float approachTimeRoundedCompensated = ((int)pf->getApproachTime()) * speedMultiplierInv;
        const float hitWindow300RoundedCompensated = ((int)pf->getHitWindow300() - 0.5f) * speedMultiplierInv;
        const float hitWindow100RoundedCompensated = ((int)pf->getHitWindow100() - 0.5f) * speedMultiplierInv;
        const float hitWindow50RoundedCompensated = ((int)pf->getHitWindow50() - 0.5f) * speedMultiplierInv;
        const float hitobjectRadiusRoundedCompensated = (GameRules::getRawHitCircleDiameter(pf->getCS()) / 2.0f);

        const auto *bmDiff2{pf->getBeatmap()};
        const auto &tooltipOverlay{osu->getTooltipOverlay()};
        tooltipOverlay->begin();
        {
            tooltipOverlay->addLine(fmt::format("Approach time: {:.2f}ms"_cf, approachTimeRoundedCompensated));
            tooltipOverlay->addLine(fmt::format("300: +-{:.2f}ms"_cf, hitWindow300RoundedCompensated));
            tooltipOverlay->addLine(fmt::format("100: +-{:.2f}ms"_cf, hitWindow100RoundedCompensated));
            tooltipOverlay->addLine(fmt::format(" 50: +-{:.2f}ms"_cf, hitWindow50RoundedCompensated));
            tooltipOverlay->addLine(
                fmt::format("Spinner difficulty: {:.2f}"_cf, GameRules::getSpinnerSpinsPerSecond(pf.get())));
            tooltipOverlay->addLine(fmt::format("Hit object radius: {:.2f}"_cf, hitobjectRadiusRoundedCompensated));

            if(bmDiff2 != nullptr) {
                int numObjects{bmDiff2->getNumObjects()};
                int numCircles{bmDiff2->getNumCircles()};
                int numSliders{bmDiff2->getNumSliders()};
                u32 lengthMS{bmDiff2->getLengthMS()};

                float opm{0.f}, cpm{0.f}, spm{0.f};
                if(lengthMS > 0) {
                    const float durMinutes{(static_cast<float>(lengthMS) / 1000.0f / 60.0f) / pf->getSpeedMultiplier()};

                    opm = static_cast<float>(numObjects) / durMinutes;
                    cpm = static_cast<float>(numCircles) / durMinutes;
                    spm = static_cast<float>(numSliders) / durMinutes;
                }

                tooltipOverlay->addLine(fmt::format("Circles: {:d}, Sliders: {:d}, Spinners: {:d}"_cf, numCircles,
                                                    numSliders, std::max(0, numObjects - numCircles - numSliders)));
                tooltipOverlay->addLine(
                    fmt::format("OPM: {:d}, CPM: {:d}, SPM: {:d}"_cf, (int)opm, (int)cpm, (int)spm));
                tooltipOverlay->addLine(fmt::format("ID: {:d}, SetID: {:d}"_cf, bmDiff2->getID(), bmDiff2->getSetID()));
                tooltipOverlay->addLine(fmt::format("MD5: {:s}"_cf, bmDiff2->getMD5().string()));
                // mostly for debugging
                if(keyboard->isShiftDown()) {
                    tooltipOverlay->addLine(fmt::format("Title: {:s}"_cf, bmDiff2->getTitleLatin()));
                    tooltipOverlay->addLine(fmt::format("TitleUnicode: {:s}"_cf, bmDiff2->getTitleUnicode()));
                    tooltipOverlay->addLine(fmt::format("Artist: {:s}"_cf, bmDiff2->getArtistLatin()));
                    tooltipOverlay->addLine(fmt::format("ArtistUnicode: {:s}"_cf, bmDiff2->getArtistUnicode()));
                }
            }
        }
        tooltipOverlay->end();
    }
}

void InfoLabel::setFromBeatmap(const DatabaseBeatmap *map) {
    this->iBeatmapId = map->getID();

    this->setArtist(map->getArtist());
    this->setTitle(map->getTitle());
    this->setDiff(map->getDifficultyName());
    this->setMapper(map->getCreator());

    this->setLengthMS(map->getLengthMS());
    this->setBPM(map->getMinBPM(), map->getMaxBPM(), map->getMostCommonBPM());
    this->setNumObjects(map->getNumObjects());

    this->setCS(map->getCS());
    this->setAR(map->getAR());
    this->setOD(map->getOD());
    this->setHP(map->getHP());
    this->setStars(map->getStarsNomod());

    this->setLocalOffset(map->getLocalOffset());
    this->setOnlineOffset(map->getOnlineOffset());
}

UString InfoLabel::buildSongInfoString() {
    u32 lengthMS = this->iLengthMS;
    auto speed = osu->getMapInterface()->getSpeedMultiplier();

    const u32 fullSeconds = (lengthMS * (1.0 / speed)) / 1000.0;
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
    const auto &pf = osu->getMapInterface();
    const auto *map = pf->getBeatmap();
    if(!map) return "";

    float CS = pf->getCS();
    float AR = pf->getApproachRateForSpeedMultiplier();
    float OD = pf->getOverallDifficultyForSpeedMultiplier();
    float HP = pf->getHP();

    float stars = this->fStars;
    float modStars = 0.f;
    float modPp = 0.f;

    bool pp_available = false;

    // pp calc for currently selected mods
    {
        const auto &pp = pf->getWholeMapPPInfo();
        if(pp.pp != -1.0) {
            modStars = pp.total_stars;
            modPp = pp.pp;
            pp_available = true;
        }
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
    return fmt::format("Your Offset: {:d} ms / Online Offset: {:d} ms", this->iLocalOffset, this->iOnlineOffset);
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
