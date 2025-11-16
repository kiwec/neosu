#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "SongButton.h"

class ConVar;

class SongDifficultyButton final : public SongButton {
    NOCOPY_NOMOVE(SongDifficultyButton)
   public:
    DEF_BUTTON_TYPE(SongDifficultyButton, SongDifficultyButton_, SongButton)
   public:
    SongDifficultyButton(SongBrowser *songBrowser, UIContextMenu *contextMenu, float xPos, float yPos, float xSize,
                         float ySize, UString name, DatabaseBeatmap *map, SongButton *parentSongButton);
    ~SongDifficultyButton() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;
    void onClicked(bool left = true, bool right = false) override;

    void updateGrade() override;

    [[nodiscard]] Color getInactiveBackgroundColor() const override;

    [[nodiscard]] inline SongButton *getParentSongButton() const { return this->parentSongButton; }

    [[nodiscard]] bool isIndependentDiffButton() const;

   private:
    void onSelected(bool wasSelected, bool autoSelectBottomMostChild, bool wasParentSelected) override;

    std::string sDiff;

    float fDiffScale;
    float fOffsetPercentAnim;

    SongButton *parentSongButton;

    bool bUpdateGradeScheduled;
    bool bPrevOffsetPercentSelectionState;
};
