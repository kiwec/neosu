#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CarouselButton.h"
#include "score.h"

class SongBrowser;
class DatabaseBeatmap;
class BeatmapCarousel;

class SongButton : public CarouselButton {
    NOCOPY_NOMOVE(SongButton)
   public:
    DEF_BUTTON_TYPE(SongButton, SongButton_, CarouselButton)
   public:
    SongButton(SongBrowser *songBrowser, UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize,
               UString name, DatabaseBeatmap *databaseBeatmap);
    ~SongButton() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void triggerContextMenu(vec2 pos);

    void sortChildren();

    void updateLayoutEx() override;
    virtual void updateGrade() { ; }

    [[nodiscard]] DatabaseBeatmap *getDatabaseBeatmap() const override { return this->databaseBeatmap; }
    FinishedScore::Grade grade = FinishedScore::Grade::N;

   protected:
    void onSelected(bool wasSelected, bool autoSelectBottomMostChild, bool wasParentSelected) override;
    void onRightMouseUpInside() override;

    void onContextMenu(const UString &text, int id = -1);
    void onAddToCollectionConfirmed(const UString &text, int id = -1);
    void onCreateNewCollectionConfirmed(const UString &text, int id = -1);

    void drawBeatmapBackgroundThumbnail(const Image *image);
    void drawGrade();
    void drawTitle(float deselectedAlpha = 1.0f, bool forceSelectedStyle = false);
    void drawSubTitle(float deselectedAlpha = 1.0f, bool forceSelectedStyle = false);

    float calculateGradeScale();
    float calculateGradeWidth();

    DatabaseBeatmap *databaseBeatmap;

    std::string sTitle;
    std::string sArtist;
    std::string sMapper;

    float fTextOffset;
    float fGradeOffset;
    float fTextSpacingScale;
    float fTextMarginScale;
    float fTitleScale;
    float fSubTitleScale;
    float fGradeScale;

   private:
    DatabaseBeatmap *representativeBeatmap{nullptr};

    float fVisibleFor{0.f};
    float fThumbnailFadeInTime;
    void onOpenBeatmapFolderClicked();
};
