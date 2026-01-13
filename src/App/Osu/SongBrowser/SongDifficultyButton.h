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
    SongDifficultyButton(float xPos, float yPos, float xSize, float ySize, UString name, BeatmapDifficulty *diff,
                         SongButton *parentSongButton);

   public:
    SongDifficultyButton() = delete;
    ~SongDifficultyButton() override;

    void draw() override;
    void update(CBaseUIEventCtx &c) override;
    void onClicked(bool left = true, bool right = false) override;

    void resetAnimations() override;

    void updateGrade() override;

    [[nodiscard]] Color getInactiveBackgroundColor() const override;

    [[nodiscard]] inline SongButton *getParentSongButton() const { return this->parentSongButton; }
    [[nodiscard]] inline const std::vector<SongDifficultyButton *> &getSiblingsAndSelf() const {
        return reinterpret_cast<const std::vector<SongDifficultyButton *> &>(this->parentSongButton->getChildren());
    }

    [[nodiscard]] bool isIndependentDiffButton() const;

   private:
    void onSelected(bool wasSelected, SelOpts opts) override;
    void updateOffsetAnimation();

    SongButton *parentSongButton;

    float fDiffScale;
    float fOffsetPercentAnim;
    float fVisibleFor{0.f};

    enum class OffsetState : u8 { UNINITIALIZED, SELECTED, DESELECTED };
    OffsetState lastOffsetState{OffsetState::UNINITIALIZED};
    bool bUpdateGradeScheduled;
};
