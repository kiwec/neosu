#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include <utility>

#include "CBaseUIElement.h"

class McFont;
class DatabaseBeatmap;

class UIRankingScreenInfoLabel final : public CBaseUIElement {
   public:
    UIRankingScreenInfoLabel(float xPos, float yPos, float xSize, float ySize, UString name);

    void draw() override;

    void setFromBeatmap(DatabaseBeatmap *map);

    void setArtist(std::string artist) { this->sArtist = std::move(artist); }
    void setTitle(std::string title) { this->sTitle = std::move(title); }
    void setDiff(std::string diff) { this->sDiff = std::move(diff); }
    void setMapper(std::string mapper) { this->sMapper = std::move(mapper); }
    void setPlayer(std::string player) { this->sPlayer = std::move(player); }
    void setDate(std::string date) { this->sDate = std::move(date); }

    float getMinimumWidth();
    float getMinimumHeight();

   private:
    UString buildPlayerString();

    McFont *font;

    int iMargin;
    float fSubTitleScale;

    std::string sArtist;
    std::string sTitle;
    std::string sDiff;
    std::string sMapper;
    std::string sPlayer;
    std::string sDate;
};
