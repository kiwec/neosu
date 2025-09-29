#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include <utility>

#include "CBaseUIButton.h"

class McFont;
class DatabaseBeatmap;

class InfoLabel final : public CBaseUIButton {
   public:
    InfoLabel(float xPos, float yPos, float xSize, float ySize, UString name);

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void setFromBeatmap(DatabaseBeatmap *map);

    void setArtist(std::string artist) { this->sArtist = std::move(artist); }
    void setTitle(std::string title) { this->sTitle = std::move(title); }
    void setDiff(std::string diff) { this->sDiff = std::move(diff); }
    void setMapper(std::string mapper) { this->sMapper = std::move(mapper); }

    void setLengthMS(unsigned long lengthMS) { this->iLengthMS = lengthMS; }
    void setBPM(int minBPM, int maxBPM, int mostCommonBPM) {
        this->iMinBPM = minBPM;
        this->iMaxBPM = maxBPM;
        this->iMostCommonBPM = mostCommonBPM;
    }
    void setNumObjects(int numObjects) { this->iNumObjects = numObjects; }

    void setCS(float CS) { this->fCS = CS; }
    void setAR(float AR) { this->fAR = AR; }
    void setOD(float OD) { this->fOD = OD; }
    void setHP(float HP) { this->fHP = HP; }
    void setStars(float stars) { this->fStars = stars; }

    void setLocalOffset(long localOffset) { this->iLocalOffset = localOffset; }
    void setOnlineOffset(long onlineOffset) { this->iOnlineOffset = onlineOffset; }

    float getMinimumWidth();
    float getMinimumHeight();

    [[nodiscard]] long getBeatmapID() const { return this->iBeatmapId; }

   private:
    UString buildSongInfoString();
    UString buildDiffInfoString();
    UString buildOffsetInfoString();

    McFont *font;

    int iMargin;
    f32 fGlobalScale;
    f32 fTitleScale;
    f32 fSubTitleScale;
    f32 fSongInfoScale;
    f32 fDiffInfoScale;
    f32 fOffsetInfoScale;

    std::string sArtist;
    std::string sTitle;
    std::string sDiff;
    std::string sMapper;

    unsigned long iLengthMS;
    int iMinBPM;
    int iMaxBPM;
    int iMostCommonBPM;
    int iNumObjects;

    float fCS;
    float fAR;
    float fOD;
    float fHP;
    float fStars;

    long iLocalOffset;
    long iOnlineOffset;

    // custom
    long iBeatmapId;
};
