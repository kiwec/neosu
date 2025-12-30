#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "SongButton.h"

class ConVar;

class SongDifficultyButton final : public SongButton {
    NOCOPY_NOMOVE(SongDifficultyButton)
   public:
    DEF_BUTTON_TYPE(SongDifficultyButton, SongDifficultyButton_, SongButton)
   private:
    // only allow construction through parent song button (as child)
    friend class SongButton;
    SongDifficultyButton(UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize, UString name,
                         BeatmapDifficulty *diff, SongButton *parentSongButton, int numSiblings);

   public:
    SongDifficultyButton() = delete;
    ~SongDifficultyButton() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;
    void onClicked(bool left = true, bool right = false) override;

    void updateGrade() override;

    [[nodiscard]] Color getInactiveBackgroundColor() const override;

    [[nodiscard]] inline SongButton *getParentSongButton() const { return this->parentSongButton; }
    [[nodiscard]] inline const std::vector<SongDifficultyButton *> &getSiblingsAndSelf() const {
        return this->siblings;
    }

    [[nodiscard]] bool isIndependentDiffButton() const;

   private:
    void onSelected(bool wasSelected, SelOpts opts) override;

    SongButton *parentSongButton;
    const std::vector<SongDifficultyButton *> &siblings;  // ref to parent's children

    float fDiffScale;
    float fOffsetPercentAnim;
    float fVisibleFor{0.f};

    bool bPrevOffsetPercentSelectionState;
    bool bUpdateGradeScheduled;
};
