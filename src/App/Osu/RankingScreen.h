#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "Database.h"
#include "DifficultyCalculator.h"
#include "ScreenBackable.h"
#include "score.h"
#include "uwu.h"

class CBaseUIContainer;
class CBaseUIScrollView;
class CBaseUIImage;
class CBaseUILabel;
class DatabaseBeatmap;
class SkinImage;
class UIButton;
class UIRankingScreenInfoLabel;
class UIRankingScreenRankingPanel;
class RankingScreenIndexLabel;
class RankingScreenBottomElement;

class ConVar;

class RankingScreen : public ScreenBackable {
   public:
    RankingScreen();

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void onRetryClicked();
    void onWatchClicked();

    void setScore(const FinishedScore &score);
    void setBeatmapInfo(DatabaseBeatmap *map);

   private:
    void updateLayout() override;
    void onBack() override;

    void drawModImage(SkinImage *image, vec2 &pos, vec2 &max);

    void setGrade(FinishedScore::Grade grade);
    void setIndex(int index);

    UString getPPString();
    vec2 getPPPosRaw();

    CBaseUIScrollView *rankings;

    UIRankingScreenInfoLabel *songInfo;
    UIRankingScreenRankingPanel *rankingPanel;
    CBaseUIImage *rankingTitle;
    CBaseUIImage *rankingGrade;
    RankingScreenIndexLabel *rankingIndex;
    RankingScreenBottomElement *rankingBottom;

    UIButton *retry_btn;
    UIButton *watch_btn;

    FinishedScore::Grade grade;
    float fUnstableRate;
    float fHitErrorAvgMin;
    float fHitErrorAvgMax;

    UString sMods;
    bool bModSS;
    bool bModSD;
    bool bModEZ;
    bool bModHD;
    bool bModHR;
    bool bModNightmare;
    bool bModScorev2;
    bool bModTarget;
    bool bModSpunout;
    bool bModRelax;
    bool bModNF;
    bool bModAutopilot;
    bool bModAuto;
    bool bModTD;

    std::vector<ConVar *> enabledExperimentalMods;

    // custom
    //uwu::lazy_promise<std::function<pp_info()>, pp_info> ppv2_calc{pp_info{}};
    FinishedScore score;
    bool bIsUnranked;
};
