#pragma once
// Copyright (c) 2017, PG, All rights reserved.
#include <utility>

#include "CBaseUIElement.h"

class UISearchOverlay final : public CBaseUIElement {
   public:
    UISearchOverlay(float xPos, float yPos, float xSize, float ySize, UString name);

    void draw() override;

    void setDrawNumResults(bool drawNumResults) { this->bDrawNumResults = drawNumResults; }
    void setOffsetRight(int offsetRight) { this->iOffsetRight = offsetRight; }

    inline void setSearchString(std::string searchString, std::string hardcodedSearchString = "") {
        this->sSearchString = std::move(searchString);
        this->sHardcodedSearchString = std::move(hardcodedSearchString);
    }
    void setNumFoundResults(int numFoundResults) { this->iNumFoundResults = numFoundResults; }

    void setSearching(bool searching) { this->bSearching = searching; }

   private:
    McFont *font;

    int iOffsetRight;
    bool bDrawNumResults;

    std::string sSearchString;
    std::string sHardcodedSearchString;
    int iNumFoundResults;

    bool bSearching;
};
