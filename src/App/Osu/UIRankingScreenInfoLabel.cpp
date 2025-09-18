// Copyright (c) 2016, PG, All rights reserved.
#include "UIRankingScreenInfoLabel.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "BeatmapInterface.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Osu.h"
#include "SString.h"
#include "Font.h"

UIRankingScreenInfoLabel::UIRankingScreenInfoLabel(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
    this->font = osu->getSubTitleFont();

    this->iMargin = 10;

    float globalScaler = 1.3f;
    this->fSubTitleScale = 0.6f * globalScaler;

    this->sArtist = "Artist";
    this->sTitle = "Title";
    this->sDiff = "Difficulty";
    this->sMapper = "Mapper";
    this->sPlayer = "Guest";
    this->sDate = "?";
}

void UIRankingScreenInfoLabel::draw() {
    // build strings
    UString titleText{this->sArtist};
    titleText.append({" - "});
    titleText.append(UString{this->sTitle});
    titleText.append({" ["});
    titleText.append(UString{this->sDiff});
    titleText.append({"]"});
    titleText = titleText.trim();
    UString subTitleText{"Beatmap by "};
    subTitleText.append(UString{this->sMapper});
    subTitleText = subTitleText.trim();
    const UString playerText{this->buildPlayerString()};

    const float globalScale = std::max((this->vSize.y / this->getMinimumHeight()) * 0.741f, 1.0f);

    // draw title
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        const float scale = globalScale;

        g->scale(scale, scale);
        g->translate(this->vPos.x, this->vPos.y + this->font->getHeight() * scale);
        g->drawString(this->font, titleText);
    }
    g->popTransform();

    // draw subtitle
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        const float scale = this->fSubTitleScale * globalScale;

        const float subTitleStringWidth = this->font->getStringWidth(subTitleText);

        g->translate((int)(-subTitleStringWidth / 2), (int)(this->font->getHeight() / 2));
        g->scale(scale, scale);
        g->translate(
            (int)(this->vPos.x + (subTitleStringWidth / 2) * scale),
            (int)(this->vPos.y + this->font->getHeight() * globalScale + (this->font->getHeight() / 2) * scale + this->iMargin));
        g->drawString(this->font, subTitleText);
    }
    g->popTransform();

    // draw subsubtitle (player text + datetime)
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        const float scale = this->fSubTitleScale * globalScale;

        const float playerStringWidth = this->font->getStringWidth(playerText);

        g->translate((int)(-playerStringWidth / 2), (int)(this->font->getHeight() / 2));
        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + (playerStringWidth / 2) * scale),
                     (int)(this->vPos.y + this->font->getHeight() * globalScale + this->font->getHeight() * scale +
                           (this->font->getHeight() / 2) * scale + this->iMargin * 2));
        g->drawString(this->font, playerText);
    }
    g->popTransform();
}

void UIRankingScreenInfoLabel::setFromBeatmap(DatabaseBeatmap *map) {
    this->setArtist(map->getArtist());
    this->setTitle(map->getTitle());
    this->setDiff(map->getDifficultyName());
    this->setMapper(map->getCreator());

    std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    this->sDate = std::ctime(&now_c);
    SString::trim(&this->sDate);
}

UString UIRankingScreenInfoLabel::buildPlayerString() {
    UString playerString{"Played by "};
    playerString.append(UString{this->sPlayer});
    playerString.append({" on "});
    playerString.append(UString{this->sDate});

    return playerString.trim();
}

float UIRankingScreenInfoLabel::getMinimumWidth() {
    float titleWidth = 0;
    float subTitleWidth = 0;
    float playerWidth = this->font->getStringWidth(this->buildPlayerString()) * this->fSubTitleScale;

    return std::max({titleWidth, subTitleWidth, playerWidth});
}

float UIRankingScreenInfoLabel::getMinimumHeight() {
    float titleHeight = this->font->getHeight();
    float subTitleHeight = this->font->getHeight() * this->fSubTitleScale;
    float playerHeight = this->font->getHeight() * this->fSubTitleScale;

    return titleHeight + subTitleHeight + playerHeight + this->iMargin * 2;
}
