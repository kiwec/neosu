#pragma once
// Copyright (c) 2018, PG, All rights reserved.
#include "CBaseUIButton.h"
#include "Database.h"
#include "score.h"

class SkinImage;

class UIAvatar;
class UIContextMenu;

class ScoreButton final : public CBaseUIButton {
    NOCOPY_NOMOVE(ScoreButton)
   public:
    static SkinImage *getGradeImage(FinishedScore::Grade grade);
    static UString getModsStringForDisplay(Replay::Mods mods);

    enum class STYLE : uint8_t { SONG_BROWSER, TOP_RANKS };

    ScoreButton(UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize,
                STYLE style = STYLE::SONG_BROWSER);
    ~ScoreButton() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void highlight();
    void resetHighlight();

    void setScore(const FinishedScore &score, DatabaseBeatmap *map, int index = 1, const UString &titleString = "",
                  float weight = 1.0f);
    void setIndex(int index) { this->iScoreIndexNumber = index; }

    [[nodiscard]] inline FinishedScore getScore() const { return this->score; }
    [[nodiscard]] inline u64 getScoreUnixTimestamp() const { return this->score.unixTimestamp; }
    [[nodiscard]] inline u64 getScoreScore() const { return this->score.score; }

    [[nodiscard]] inline UString getDateTime() const { return this->sScoreDateTime; }
    [[nodiscard]] inline int getIndex() const { return this->iScoreIndexNumber; }

    bool is_friend = false;
    UIAvatar *avatar = nullptr;
    MD5Hash map_hash;

    void onMouseInside() override;
    void onMouseOutside() override;

    void onFocusStolen() override;

   protected:
    void onClicked(bool left = true, bool right = false) override;

   private:
    static UString recentScoreIconString;

    void updateElapsedTimeString();

    void onRightMouseUpInside();
    void onContextMenu(const UString &text, int id = -1);
    void onUseModsClicked();
    void onDeleteScoreClicked();
    void onDeleteScoreConfirmed(const UString &text, int id);

    bool isContextMenuVisible();

    UIContextMenu *contextMenu;

    // STYLE::SCORE_BROWSER
    UString sScoreTime;
    UString sScoreUsername;
    UString sScoreScore;
    UString sScoreScorePP;
    UString sScoreAccuracy;
    UString sScoreAccuracyFC;
    UString sScoreMods;
    UString sCustom;

    // STYLE::TOP_RANKS
    UString sScoreTitle;
    UString sScoreScorePPWeightedPP;
    UString sScoreScorePPWeightedWeight;
    UString sScoreWeight;

    std::vector<UString> tooltipLines;
    UString sScoreDateTime;

    // score data
    FinishedScore score;

    int iScoreIndexNumber{1};
    u64 iScoreUnixTimestamp{0};

    FinishedScore::Grade scoreGrade{FinishedScore::Grade::D};

    STYLE style;
    float fIndexNumberAnim{0.0f};
    bool bIsPulseAnim{false};

    bool bRightClick{false};
    bool bRightClickCheck{false};
};
