#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "noinclude.h"
#include "Color.h"
#include "Vectors.h"

#include <utility>
#include <vector>

class Image;
class Sound;
class Resource;
class ConVar;
class UString;

class SkinImage;

extern Image *MISSING_TEXTURE;

class Skin final {
    NOCOPY_NOMOVE(Skin)

   public:
    static void unpack(const char *filepath);

    Skin(const UString &name, std::string filepath, bool isDefaultSkin = false);
    ~Skin();

    void update();

    bool isReady();

    void load();
    void loadBeatmapOverride(const std::string &filepath);
    void reloadSounds();

    void setAnimationSpeed(float animationSpeed) { this->animationSpeedMultiplier = animationSpeed; }
    float getAnimationSpeed() { return this->animationSpeedMultiplier; }

    void playSpinnerSpinSound();
    void playSpinnerBonusSound();
    void stopSpinnerSpinSound();

    // custom
    void randomizeFilePath();

    // drawable helpers
    [[nodiscard]] inline std::string getName() const { return this->sName; }
    [[nodiscard]] inline std::string getFilePath() const { return this->sFilePath; }

    // raw
    [[nodiscard]] inline Image *getMissingTexture() const { return MISSING_TEXTURE; }

    [[nodiscard]] inline Image *getHitCircle() const { return this->hitCircle; }
    [[nodiscard]] inline SkinImage *getHitCircleOverlay2() const { return this->hitCircleOverlay2; }
    [[nodiscard]] inline Image *getApproachCircle() const { return this->approachCircle; }
    [[nodiscard]] inline Image *getReverseArrow() const { return this->reverseArrow; }
    [[nodiscard]] inline SkinImage *getFollowPoint2() const { return this->followPoint2; }

    [[nodiscard]] inline Image *getDefault0() const { return this->default0; }
    [[nodiscard]] inline Image *getDefault1() const { return this->default1; }
    [[nodiscard]] inline Image *getDefault2() const { return this->default2; }
    [[nodiscard]] inline Image *getDefault3() const { return this->default3; }
    [[nodiscard]] inline Image *getDefault4() const { return this->default4; }
    [[nodiscard]] inline Image *getDefault5() const { return this->default5; }
    [[nodiscard]] inline Image *getDefault6() const { return this->default6; }
    [[nodiscard]] inline Image *getDefault7() const { return this->default7; }
    [[nodiscard]] inline Image *getDefault8() const { return this->default8; }
    [[nodiscard]] inline Image *getDefault9() const { return this->default9; }
    [[nodiscard]] inline Image *getDefaultN(uSz n) const {
        assert(n < 10);  // don't be stupid
        switch(n) {
                // clang-format off
            case 0: return this->default0;
            case 1: return this->default1;
            case 2: return this->default2;
            case 3: return this->default3;
            case 4: return this->default4;
            case 5: return this->default5;
            case 6: return this->default6;
            case 7: return this->default7;
            case 8: return this->default8;
            case 9: return this->default9;
            default: std::unreachable();
                // clang-format on
        }
        std::unreachable();
    }

    [[nodiscard]] inline Image *getScore0() const { return this->score0; }
    [[nodiscard]] inline Image *getScore1() const { return this->score1; }
    [[nodiscard]] inline Image *getScore2() const { return this->score2; }
    [[nodiscard]] inline Image *getScore3() const { return this->score3; }
    [[nodiscard]] inline Image *getScore4() const { return this->score4; }
    [[nodiscard]] inline Image *getScore5() const { return this->score5; }
    [[nodiscard]] inline Image *getScore6() const { return this->score6; }
    [[nodiscard]] inline Image *getScore7() const { return this->score7; }
    [[nodiscard]] inline Image *getScore8() const { return this->score8; }
    [[nodiscard]] inline Image *getScore9() const { return this->score9; }
    [[nodiscard]] inline Image *getScoreX() const { return this->scoreX; }
    [[nodiscard]] inline Image *getScorePercent() const { return this->scorePercent; }
    [[nodiscard]] inline Image *getScoreDot() const { return this->scoreDot; }
    [[nodiscard]] inline Image *getScoreN(uSz n) const {
        assert(n < 10);
        switch(n) {
                // clang-format off
            case 0: return this->score0;
            case 1: return this->score1;
            case 2: return this->score2;
            case 3: return this->score3;
            case 4: return this->score4;
            case 5: return this->score5;
            case 6: return this->score6;
            case 7: return this->score7;
            case 8: return this->score8;
            case 9: return this->score9;
            default: std::unreachable();
                // clang-format on
        }
        std::unreachable();
    }

    [[nodiscard]] inline Image *getCombo0() const { return this->combo0; }
    [[nodiscard]] inline Image *getCombo1() const { return this->combo1; }
    [[nodiscard]] inline Image *getCombo2() const { return this->combo2; }
    [[nodiscard]] inline Image *getCombo3() const { return this->combo3; }
    [[nodiscard]] inline Image *getCombo4() const { return this->combo4; }
    [[nodiscard]] inline Image *getCombo5() const { return this->combo5; }
    [[nodiscard]] inline Image *getCombo6() const { return this->combo6; }
    [[nodiscard]] inline Image *getCombo7() const { return this->combo7; }
    [[nodiscard]] inline Image *getCombo8() const { return this->combo8; }
    [[nodiscard]] inline Image *getCombo9() const { return this->combo9; }
    [[nodiscard]] inline Image *getComboX() const { return this->comboX; }
    [[nodiscard]] inline Image *getComboN(uSz n) const {
        assert(n < 10);
        switch(n) {
                // clang-format off
            case 0: return this->combo0;
            case 1: return this->combo1;
            case 2: return this->combo2;
            case 3: return this->combo3;
            case 4: return this->combo4;
            case 5: return this->combo5;
            case 6: return this->combo6;
            case 7: return this->combo7;
            case 8: return this->combo8;
            case 9: return this->combo9;
            default: std::unreachable();
                // clang-format on
        }
        std::unreachable();
    }

    [[nodiscard]] inline SkinImage *getPlaySkip() const { return this->playSkip; }
    [[nodiscard]] inline Image *getPlayWarningArrow() const { return this->playWarningArrow; }
    [[nodiscard]] inline SkinImage *getPlayWarningArrow2() const { return this->playWarningArrow2; }
    [[nodiscard]] inline Image *getCircularmetre() const { return this->circularmetre; }
    [[nodiscard]] inline SkinImage *getScorebarBg() const { return this->scorebarBg; }
    [[nodiscard]] inline SkinImage *getScorebarColour() const { return this->scorebarColour; }
    [[nodiscard]] inline SkinImage *getScorebarMarker() const { return this->scorebarMarker; }
    [[nodiscard]] inline SkinImage *getScorebarKi() const { return this->scorebarKi; }
    [[nodiscard]] inline SkinImage *getScorebarKiDanger() const { return this->scorebarKiDanger; }
    [[nodiscard]] inline SkinImage *getScorebarKiDanger2() const { return this->scorebarKiDanger2; }
    [[nodiscard]] inline SkinImage *getSectionPassImage() const { return this->sectionPassImage; }
    [[nodiscard]] inline SkinImage *getSectionFailImage() const { return this->sectionFailImage; }
    [[nodiscard]] inline SkinImage *getInputoverlayBackground() const { return this->inputoverlayBackground; }
    [[nodiscard]] inline SkinImage *getInputoverlayKey() const { return this->inputoverlayKey; }

    [[nodiscard]] inline SkinImage *getHit0() const { return this->hit0; }
    [[nodiscard]] inline SkinImage *getHit50() const { return this->hit50; }
    [[nodiscard]] inline SkinImage *getHit50g() const { return this->hit50g; }
    [[nodiscard]] inline SkinImage *getHit50k() const { return this->hit50k; }
    [[nodiscard]] inline SkinImage *getHit100() const { return this->hit100; }
    [[nodiscard]] inline SkinImage *getHit100g() const { return this->hit100g; }
    [[nodiscard]] inline SkinImage *getHit100k() const { return this->hit100k; }
    [[nodiscard]] inline SkinImage *getHit300() const { return this->hit300; }
    [[nodiscard]] inline SkinImage *getHit300g() const { return this->hit300g; }
    [[nodiscard]] inline SkinImage *getHit300k() const { return this->hit300k; }

    [[nodiscard]] inline Image *getParticle50() const { return this->particle50; }
    [[nodiscard]] inline Image *getParticle100() const { return this->particle100; }
    [[nodiscard]] inline Image *getParticle300() const { return this->particle300; }

    [[nodiscard]] inline Image *getSliderGradient() const { return this->sliderGradient; }
    [[nodiscard]] inline SkinImage *getSliderb() const { return this->sliderb; }
    [[nodiscard]] inline SkinImage *getSliderFollowCircle2() const { return this->sliderFollowCircle2; }
    [[nodiscard]] inline Image *getSliderScorePoint() const { return this->sliderScorePoint; }
    [[nodiscard]] inline Image *getSliderStartCircle() const { return this->sliderStartCircle; }
    [[nodiscard]] inline SkinImage *getSliderStartCircle2() const { return this->sliderStartCircle2; }
    [[nodiscard]] inline Image *getSliderStartCircleOverlay() const { return this->sliderStartCircleOverlay; }
    [[nodiscard]] inline SkinImage *getSliderStartCircleOverlay2() const { return this->sliderStartCircleOverlay2; }
    [[nodiscard]] inline Image *getSliderEndCircle() const { return this->sliderEndCircle; }
    [[nodiscard]] inline SkinImage *getSliderEndCircle2() const { return this->sliderEndCircle2; }
    [[nodiscard]] inline Image *getSliderEndCircleOverlay() const { return this->sliderEndCircleOverlay; }
    [[nodiscard]] inline SkinImage *getSliderEndCircleOverlay2() const { return this->sliderEndCircleOverlay2; }

    [[nodiscard]] inline Image *getSpinnerBackground() const { return this->spinnerBackground; }
    [[nodiscard]] inline Image *getSpinnerCircle() const { return this->spinnerCircle; }
    [[nodiscard]] inline Image *getSpinnerApproachCircle() const { return this->spinnerApproachCircle; }
    [[nodiscard]] inline Image *getSpinnerBottom() const { return this->spinnerBottom; }
    [[nodiscard]] inline Image *getSpinnerMiddle() const { return this->spinnerMiddle; }
    [[nodiscard]] inline Image *getSpinnerMiddle2() const { return this->spinnerMiddle2; }
    [[nodiscard]] inline Image *getSpinnerTop() const { return this->spinnerTop; }
    [[nodiscard]] inline Image *getSpinnerSpin() const { return this->spinnerSpin; }
    [[nodiscard]] inline Image *getSpinnerClear() const { return this->spinnerClear; }

    [[nodiscard]] inline Image *getDefaultCursor() const { return this->defaultCursor; }
    [[nodiscard]] inline Image *getCursor() const { return this->cursor; }
    [[nodiscard]] inline Image *getCursorMiddle() const { return this->cursorMiddle; }
    [[nodiscard]] inline Image *getCursorTrail() const { return this->cursorTrail; }
    [[nodiscard]] inline Image *getCursorRipple() const { return this->cursorRipple; }
    [[nodiscard]] inline Image *getCursorSmoke() const { return this->cursorSmoke; }

    [[nodiscard]] inline SkinImage *getSelectionModEasy() const { return this->selectionModEasy; }
    [[nodiscard]] inline SkinImage *getSelectionModNoFail() const { return this->selectionModNoFail; }
    [[nodiscard]] inline SkinImage *getSelectionModHalfTime() const { return this->selectionModHalfTime; }
    [[nodiscard]] inline SkinImage *getSelectionModDayCore() const { return this->selectionModDayCore; }
    [[nodiscard]] inline SkinImage *getSelectionModHardRock() const { return this->selectionModHardRock; }
    [[nodiscard]] inline SkinImage *getSelectionModSuddenDeath() const { return this->selectionModSuddenDeath; }
    [[nodiscard]] inline SkinImage *getSelectionModPerfect() const { return this->selectionModPerfect; }
    [[nodiscard]] inline SkinImage *getSelectionModDoubleTime() const { return this->selectionModDoubleTime; }
    [[nodiscard]] inline SkinImage *getSelectionModNightCore() const { return this->selectionModNightCore; }
    [[nodiscard]] inline SkinImage *getSelectionModHidden() const { return this->selectionModHidden; }
    [[nodiscard]] inline SkinImage *getSelectionModFlashlight() const { return this->selectionModFlashlight; }
    [[nodiscard]] inline SkinImage *getSelectionModRelax() const { return this->selectionModRelax; }
    [[nodiscard]] inline SkinImage *getSelectionModAutopilot() const { return this->selectionModAutopilot; }
    [[nodiscard]] inline SkinImage *getSelectionModSpunOut() const { return this->selectionModSpunOut; }
    [[nodiscard]] inline SkinImage *getSelectionModAutoplay() const { return this->selectionModAutoplay; }
    [[nodiscard]] inline SkinImage *getSelectionModNightmare() const { return this->selectionModNightmare; }
    [[nodiscard]] inline SkinImage *getSelectionModTarget() const { return this->selectionModTarget; }
    [[nodiscard]] inline SkinImage *getSelectionModScorev2() const { return this->selectionModScorev2; }
    [[nodiscard]] inline SkinImage *getSelectionModTD() const { return this->selectionModTD; }

    [[nodiscard]] inline Image *getPauseContinue() const { return this->pauseContinue; }
    [[nodiscard]] inline Image *getPauseRetry() const { return this->pauseRetry; }
    [[nodiscard]] inline Image *getPauseBack() const { return this->pauseBack; }
    [[nodiscard]] inline Image *getPauseOverlay() const { return this->pauseOverlay; }
    [[nodiscard]] inline Image *getFailBackground() const { return this->failBackground; }
    [[nodiscard]] inline Image *getUnpause() const { return this->unpause; }

    [[nodiscard]] inline Image *getButtonLeft() const { return this->buttonLeft; }
    [[nodiscard]] inline Image *getButtonMiddle() const { return this->buttonMiddle; }
    [[nodiscard]] inline Image *getButtonRight() const { return this->buttonRight; }
    [[nodiscard]] inline Image *getDefaultButtonLeft() const { return this->defaultButtonLeft; }
    [[nodiscard]] inline Image *getDefaultButtonMiddle() const { return this->defaultButtonMiddle; }
    [[nodiscard]] inline Image *getDefaultButtonRight() const { return this->defaultButtonRight; }
    [[nodiscard]] inline SkinImage *getMenuBack2() const { return this->menuBackImg; }

    [[nodiscard]] inline Image *getMenuButtonBackground() const { return this->menuButtonBackground; }
    [[nodiscard]] inline SkinImage *getMenuButtonBackground2() const { return this->menuButtonBackground2; }
    [[nodiscard]] inline Image *getStar() const { return this->star; }
    [[nodiscard]] inline Image *getRankingPanel() const { return this->rankingPanel; }
    [[nodiscard]] inline Image *getRankingGraph() const { return this->rankingGraph; }
    [[nodiscard]] inline Image *getRankingTitle() const { return this->rankingTitle; }
    [[nodiscard]] inline Image *getRankingMaxCombo() const { return this->rankingMaxCombo; }
    [[nodiscard]] inline Image *getRankingAccuracy() const { return this->rankingAccuracy; }
    [[nodiscard]] inline Image *getRankingA() const { return this->rankingA; }
    [[nodiscard]] inline Image *getRankingB() const { return this->rankingB; }
    [[nodiscard]] inline Image *getRankingC() const { return this->rankingC; }
    [[nodiscard]] inline Image *getRankingD() const { return this->rankingD; }
    [[nodiscard]] inline Image *getRankingS() const { return this->rankingS; }
    [[nodiscard]] inline Image *getRankingSH() const { return this->rankingSH; }
    [[nodiscard]] inline Image *getRankingX() const { return this->rankingX; }
    [[nodiscard]] inline Image *getRankingXH() const { return this->rankingXH; }
    [[nodiscard]] inline SkinImage *getRankingAsmall() const { return this->rankingAsmall; }
    [[nodiscard]] inline SkinImage *getRankingBsmall() const { return this->rankingBsmall; }
    [[nodiscard]] inline SkinImage *getRankingCsmall() const { return this->rankingCsmall; }
    [[nodiscard]] inline SkinImage *getRankingDsmall() const { return this->rankingDsmall; }
    [[nodiscard]] inline SkinImage *getRankingSsmall() const { return this->rankingSsmall; }
    [[nodiscard]] inline SkinImage *getRankingSHsmall() const { return this->rankingSHsmall; }
    [[nodiscard]] inline SkinImage *getRankingXsmall() const { return this->rankingXsmall; }
    [[nodiscard]] inline SkinImage *getRankingXHsmall() const { return this->rankingXHsmall; }
    [[nodiscard]] inline SkinImage *getRankingPerfect() const { return this->rankingPerfect; }

    [[nodiscard]] inline Image *getBeatmapImportSpinner() const { return this->beatmapImportSpinner; }
    [[nodiscard]] inline Image *getLoadingSpinner() const { return this->loadingSpinner; }
    [[nodiscard]] inline Image *getCircleEmpty() const { return this->circleEmpty; }
    [[nodiscard]] inline Image *getCircleFull() const { return this->circleFull; }
    [[nodiscard]] inline Image *getSeekTriangle() const { return this->seekTriangle; }
    [[nodiscard]] inline Image *getUserIcon() const { return this->userIcon; }
    [[nodiscard]] inline Image *getBackgroundCube() const { return this->backgroundCube; }
    [[nodiscard]] inline Image *getMenuBackground() const { return this->menuBackground; }
    [[nodiscard]] inline Image *getSkybox() const { return this->skybox; }

    [[nodiscard]] inline Sound *getSpinnerBonus() const { return this->spinnerBonus; }
    [[nodiscard]] inline Sound *getSpinnerSpinSound() const { return this->spinnerSpinSound; }
    [[nodiscard]] inline Sound *getCombobreak() const { return this->combobreak; }
    [[nodiscard]] inline Sound *getFailsound() const { return this->failsound; }
    [[nodiscard]] inline Sound *getApplause() const { return this->applause; }
    [[nodiscard]] inline Sound *getMenuHit() const { return this->menuHit; }
    [[nodiscard]] inline Sound *getMenuHover() const { return this->menuHover; }
    [[nodiscard]] inline Sound *getCheckOn() const { return this->checkOn; }
    [[nodiscard]] inline Sound *getCheckOff() const { return this->checkOff; }
    [[nodiscard]] inline Sound *getShutter() const { return this->shutter; }
    [[nodiscard]] inline Sound *getSectionPassSound() const { return this->sectionPassSound; }
    [[nodiscard]] inline Sound *getSectionFailSound() const { return this->sectionFailSound; }
    [[nodiscard]] inline Sound *getExpandSound() const { return this->expand; }
    [[nodiscard]] inline Sound *getMessageSentSound() const { return this->messageSent; }
    [[nodiscard]] inline Sound *getDeletingTextSound() const { return this->deletingText; }
    [[nodiscard]] inline Sound *getMovingTextCursorSound() const { return this->movingTextCursor; }
    [[nodiscard]] inline Sound *getTyping1Sound() const { return this->typing1; }
    [[nodiscard]] inline Sound *getTyping2Sound() const { return this->typing2; }
    [[nodiscard]] inline Sound *getTyping3Sound() const { return this->typing3; }
    [[nodiscard]] inline Sound *getTyping4Sound() const { return this->typing4; }
    [[nodiscard]] inline Sound *getMenuBackSound() const { return this->menuBack; }
    [[nodiscard]] inline Sound *getCloseChatTabSound() const { return this->closeChatTab; }
    [[nodiscard]] inline Sound *getHoverButtonSound() const { return this->hoverButton; }
    [[nodiscard]] inline Sound *getClickButtonSound() const { return this->clickButton; }
    [[nodiscard]] inline Sound *getClickMainMenuCubeSound() const { return this->clickMainMenuCube; }
    [[nodiscard]] inline Sound *getHoverMainMenuCubeSound() const { return this->hoverMainMenuCube; }
    [[nodiscard]] inline Sound *getClickSingleplayerSound() const { return this->clickSingleplayer; }
    [[nodiscard]] inline Sound *getHoverSingleplayerSound() const { return this->hoverSingleplayer; }
    [[nodiscard]] inline Sound *getClickMultiplayerSound() const { return this->clickMultiplayer; }
    [[nodiscard]] inline Sound *getHoverMultiplayerSound() const { return this->hoverMultiplayer; }
    [[nodiscard]] inline Sound *getClickOptionsSound() const { return this->clickOptions; }
    [[nodiscard]] inline Sound *getHoverOptionsSound() const { return this->hoverOptions; }
    [[nodiscard]] inline Sound *getClickExitSound() const { return this->clickExit; }
    [[nodiscard]] inline Sound *getHoverExitSound() const { return this->hoverExit; }
    [[nodiscard]] inline Sound *getPauseLoopSound() const { return this->pauseLoop; }
    [[nodiscard]] inline Sound *getPauseHoverSound() const { return this->pauseHover; }
    [[nodiscard]] inline Sound *getClickPauseBackSound() const { return this->clickPauseBack; }
    [[nodiscard]] inline Sound *getHoverPauseBackSound() const { return this->hoverPauseBack; }
    [[nodiscard]] inline Sound *getClickPauseContinueSound() const { return this->clickPauseContinue; }
    [[nodiscard]] inline Sound *getHoverPauseContinueSound() const { return this->hoverPauseContinue; }
    [[nodiscard]] inline Sound *getClickPauseRetrySound() const { return this->clickPauseRetry; }
    [[nodiscard]] inline Sound *getHoverPauseRetrySound() const { return this->hoverPauseRetry; }
    [[nodiscard]] inline Sound *getBackButtonClickSound() const { return this->backButtonClick; }
    [[nodiscard]] inline Sound *getBackButtonHoverSound() const { return this->backButtonHover; }
    [[nodiscard]] inline Sound *getSelectDifficultySound() const { return this->selectDifficulty; }
    [[nodiscard]] inline Sound *getSliderbarSound() const { return this->sliderbar; }
    [[nodiscard]] inline Sound *getMatchConfirmSound() const { return this->matchConfirm; }
    [[nodiscard]] inline Sound *getRoomJoinedSound() const { return this->roomJoined; }
    [[nodiscard]] inline Sound *getRoomQuitSound() const { return this->roomQuit; }
    [[nodiscard]] inline Sound *getRoomNotReadySound() const { return this->roomNotReady; }
    [[nodiscard]] inline Sound *getRoomReadySound() const { return this->roomReady; }
    [[nodiscard]] inline Sound *getMatchStartSound() const { return this->matchStart; }

    [[nodiscard]] inline bool isCursor2x() const { return this->bCursor2x; }
    [[nodiscard]] inline bool isCursorTrail2x() const { return this->bCursorTrail2x; }
    [[nodiscard]] inline bool isCursorRipple2x() const { return this->bCursorRipple2x; }
    [[nodiscard]] inline bool isCursorSmoke2x() const { return this->bCursorSmoke2x; }
    [[nodiscard]] inline bool isApproachCircle2x() const { return this->bApproachCircle2x; }
    [[nodiscard]] inline bool isReverseArrow2x() const { return this->bReverseArrow2x; }
    [[nodiscard]] inline bool isHitCircle2x() const { return this->bHitCircle2x; }
    [[nodiscard]] inline bool isDefault02x() const { return this->bIsDefault02x; }
    [[nodiscard]] inline bool isDefault12x() const { return this->bIsDefault12x; }
    [[nodiscard]] inline bool isScore02x() const { return this->bIsScore02x; }
    [[nodiscard]] inline bool isCombo02x() const { return this->bIsCombo02x; }
    [[nodiscard]] inline bool isSpinnerApproachCircle2x() const { return this->bSpinnerApproachCircle2x; }
    [[nodiscard]] inline bool isSpinnerBottom2x() const { return this->bSpinnerBottom2x; }
    [[nodiscard]] inline bool isSpinnerCircle2x() const { return this->bSpinnerCircle2x; }
    [[nodiscard]] inline bool isSpinnerTop2x() const { return this->bSpinnerTop2x; }
    [[nodiscard]] inline bool isSpinnerMiddle2x() const { return this->bSpinnerMiddle2x; }
    [[nodiscard]] inline bool isSpinnerMiddle22x() const { return this->bSpinnerMiddle22x; }
    [[nodiscard]] inline bool isSliderScorePoint2x() const { return this->bSliderScorePoint2x; }
    [[nodiscard]] inline bool isSliderStartCircle2x() const { return this->bSliderStartCircle2x; }
    [[nodiscard]] inline bool isSliderEndCircle2x() const { return this->bSliderEndCircle2x; }

    [[nodiscard]] inline bool isCircularmetre2x() const { return this->bCircularmetre2x; }

    [[nodiscard]] inline bool isPauseContinue2x() const { return this->bPauseContinue2x; }

    [[nodiscard]] inline bool isMenuButtonBackground2x() const { return this->bMenuButtonBackground2x; }
    [[nodiscard]] inline bool isStar2x() const { return this->bStar2x; }
    [[nodiscard]] inline bool isRankingPanel2x() const { return this->bRankingPanel2x; }
    [[nodiscard]] inline bool isRankingMaxCombo2x() const { return this->bRankingMaxCombo2x; }
    [[nodiscard]] inline bool isRankingAccuracy2x() const { return this->bRankingAccuracy2x; }
    [[nodiscard]] inline bool isRankingA2x() const { return this->bRankingA2x; }
    [[nodiscard]] inline bool isRankingB2x() const { return this->bRankingB2x; }
    [[nodiscard]] inline bool isRankingC2x() const { return this->bRankingC2x; }
    [[nodiscard]] inline bool isRankingD2x() const { return this->bRankingD2x; }
    [[nodiscard]] inline bool isRankingS2x() const { return this->bRankingS2x; }
    [[nodiscard]] inline bool isRankingSH2x() const { return this->bRankingSH2x; }
    [[nodiscard]] inline bool isRankingX2x() const { return this->bRankingX2x; }
    [[nodiscard]] inline bool isRankingXH2x() const { return this->bRankingXH2x; }

    // skin.ini
    [[nodiscard]] inline float getVersion() const { return this->fVersion; }
    [[nodiscard]] inline float getAnimationFramerate() const { return this->fAnimationFramerate; }
    Color getComboColorForCounter(int i, int offset);
    void setBeatmapComboColors(std::vector<Color> colors);
    [[nodiscard]] inline Color getSpinnerApproachCircleColor() const { return this->spinnerApproachCircleColor; }
    [[nodiscard]] inline Color getSliderBorderColor() const { return this->sliderBorderColor; }
    [[nodiscard]] inline Color getSliderTrackOverride() const { return this->sliderTrackOverride; }
    [[nodiscard]] inline Color getSliderBallColor() const { return this->sliderBallColor; }

    [[nodiscard]] inline Color getSongSelectActiveText() const { return this->songSelectActiveText; }
    [[nodiscard]] inline Color getSongSelectInactiveText() const { return this->songSelectInactiveText; }

    [[nodiscard]] inline Color getInputOverlayText() const { return this->inputOverlayText; }

    [[nodiscard]] inline bool getCursorCenter() const { return this->bCursorCenter; }
    [[nodiscard]] inline bool getCursorRotate() const { return this->bCursorRotate; }
    [[nodiscard]] inline bool getCursorExpand() const { return this->bCursorExpand; }
    [[nodiscard]] inline bool getLayeredHitSounds() const { return this->bLayeredHitSounds; }

    [[nodiscard]] inline bool getSliderBallFlip() const { return this->bSliderBallFlip; }
    [[nodiscard]] inline bool getAllowSliderBallTint() const { return this->bAllowSliderBallTint; }
    [[nodiscard]] inline int getSliderStyle() const { return this->iSliderStyle; }
    [[nodiscard]] inline bool getHitCircleOverlayAboveNumber() const { return this->bHitCircleOverlayAboveNumber; }
    [[nodiscard]] inline bool isSliderTrackOverridden() const { return this->bSliderTrackOverride; }

    [[nodiscard]] inline std::string getComboPrefix() const { return this->sComboPrefix; }
    [[nodiscard]] inline int getComboOverlap() const { return this->iComboOverlap; }

    [[nodiscard]] inline std::string getScorePrefix() const { return this->sScorePrefix; }
    [[nodiscard]] inline int getScoreOverlap() const { return this->iScoreOverlap; }

    [[nodiscard]] inline std::string getHitCirclePrefix() const { return this->sHitCirclePrefix; }
    [[nodiscard]] inline int getHitCircleOverlap() const { return this->iHitCircleOverlap; }

    // custom
    [[nodiscard]] inline bool useSmoothCursorTrail() const { return this->cursorMiddle != MISSING_TEXTURE; }
    [[nodiscard]] inline bool isDefaultSkin() const { return this->bIsDefaultSkin; }

    bool parseSkinINI(std::string filepath);

    bool compareFilenameWithSkinElementName(const std::string &filename, const std::string &skinElementName);

    SkinImage *createSkinImage(const std::string &skinElementName, vec2 baseSizeForScaling2x, float osuSize,
                               bool ignoreDefaultSkin = false, const std::string &animationSeparator = "-");
    void checkLoadImage(Image **addressOfPointer, const std::string &skinElementName, const std::string &resourceName,
                        bool ignoreDefaultSkin = false, const std::string &fileExtension = "png",
                        bool forceLoadMipmaps = false);

    void loadSound(Sound *&sndRef, const std::string &skinElementName, const std::string &resourceName,
                   bool isOverlayable = false, bool isSample = false, bool loop = false,
                   bool fallback_to_default = true);

    bool bReady{false};
    bool bIsDefaultSkin;
    f32 animationSpeedMultiplier{1.f};
    std::string sName;
    std::string sFilePath;
    std::string sSkinIniFilePath;
    std::vector<Resource *> resources;
    std::vector<Sound *> sounds;
    std::vector<SkinImage *> images;

    // images
    Image *hitCircle{MISSING_TEXTURE};
    SkinImage *hitCircleOverlay2{nullptr};
    Image *approachCircle{MISSING_TEXTURE};
    Image *reverseArrow{MISSING_TEXTURE};
    SkinImage *followPoint2{nullptr};

    Image *default0{MISSING_TEXTURE};
    Image *default1{MISSING_TEXTURE};
    Image *default2{MISSING_TEXTURE};
    Image *default3{MISSING_TEXTURE};
    Image *default4{MISSING_TEXTURE};
    Image *default5{MISSING_TEXTURE};
    Image *default6{MISSING_TEXTURE};
    Image *default7{MISSING_TEXTURE};
    Image *default8{MISSING_TEXTURE};
    Image *default9{MISSING_TEXTURE};

    Image *score0{MISSING_TEXTURE};
    Image *score1{MISSING_TEXTURE};
    Image *score2{MISSING_TEXTURE};
    Image *score3{MISSING_TEXTURE};
    Image *score4{MISSING_TEXTURE};
    Image *score5{MISSING_TEXTURE};
    Image *score6{MISSING_TEXTURE};
    Image *score7{MISSING_TEXTURE};
    Image *score8{MISSING_TEXTURE};
    Image *score9{MISSING_TEXTURE};
    Image *scoreX{MISSING_TEXTURE};
    Image *scorePercent{MISSING_TEXTURE};
    Image *scoreDot{MISSING_TEXTURE};

    Image *combo0{MISSING_TEXTURE};
    Image *combo1{MISSING_TEXTURE};
    Image *combo2{MISSING_TEXTURE};
    Image *combo3{MISSING_TEXTURE};
    Image *combo4{MISSING_TEXTURE};
    Image *combo5{MISSING_TEXTURE};
    Image *combo6{MISSING_TEXTURE};
    Image *combo7{MISSING_TEXTURE};
    Image *combo8{MISSING_TEXTURE};
    Image *combo9{MISSING_TEXTURE};
    Image *comboX{MISSING_TEXTURE};

    SkinImage *playSkip{nullptr};
    Image *playWarningArrow{MISSING_TEXTURE};
    SkinImage *playWarningArrow2{nullptr};
    Image *circularmetre{MISSING_TEXTURE};
    SkinImage *scorebarBg{nullptr};
    SkinImage *scorebarColour{nullptr};
    SkinImage *scorebarMarker{nullptr};
    SkinImage *scorebarKi{nullptr};
    SkinImage *scorebarKiDanger{nullptr};
    SkinImage *scorebarKiDanger2{nullptr};
    SkinImage *sectionPassImage{nullptr};
    SkinImage *sectionFailImage{nullptr};
    SkinImage *inputoverlayBackground{nullptr};
    SkinImage *inputoverlayKey{nullptr};

    SkinImage *hit0{nullptr};
    SkinImage *hit50{nullptr};
    SkinImage *hit50g{nullptr};
    SkinImage *hit50k{nullptr};
    SkinImage *hit100{nullptr};
    SkinImage *hit100g{nullptr};
    SkinImage *hit100k{nullptr};
    SkinImage *hit300{nullptr};
    SkinImage *hit300g{nullptr};
    SkinImage *hit300k{nullptr};

    Image *particle50{MISSING_TEXTURE};
    Image *particle100{MISSING_TEXTURE};
    Image *particle300{MISSING_TEXTURE};

    Image *sliderGradient{MISSING_TEXTURE};
    SkinImage *sliderb{nullptr};
    SkinImage *sliderFollowCircle2{nullptr};
    Image *sliderScorePoint{MISSING_TEXTURE};
    Image *sliderStartCircle{MISSING_TEXTURE};
    SkinImage *sliderStartCircle2{nullptr};
    Image *sliderStartCircleOverlay{MISSING_TEXTURE};
    SkinImage *sliderStartCircleOverlay2{nullptr};
    Image *sliderEndCircle{MISSING_TEXTURE};
    SkinImage *sliderEndCircle2{nullptr};
    Image *sliderEndCircleOverlay{MISSING_TEXTURE};
    SkinImage *sliderEndCircleOverlay2{nullptr};

    Image *spinnerBackground{MISSING_TEXTURE};
    Image *spinnerCircle{MISSING_TEXTURE};
    Image *spinnerApproachCircle{MISSING_TEXTURE};
    Image *spinnerBottom{MISSING_TEXTURE};
    Image *spinnerMiddle{MISSING_TEXTURE};
    Image *spinnerMiddle2{MISSING_TEXTURE};
    Image *spinnerTop{MISSING_TEXTURE};
    Image *spinnerSpin{MISSING_TEXTURE};
    Image *spinnerClear{MISSING_TEXTURE};

    Image *defaultCursor{MISSING_TEXTURE};
    Image *cursor{MISSING_TEXTURE};
    Image *cursorMiddle{MISSING_TEXTURE};
    Image *cursorTrail{MISSING_TEXTURE};
    Image *cursorRipple{MISSING_TEXTURE};
    Image *cursorSmoke{MISSING_TEXTURE};

    SkinImage *selectionModEasy{nullptr};
    SkinImage *selectionModNoFail{nullptr};
    SkinImage *selectionModHalfTime{nullptr};
    SkinImage *selectionModDayCore{nullptr};
    SkinImage *selectionModHardRock{nullptr};
    SkinImage *selectionModSuddenDeath{nullptr};
    SkinImage *selectionModPerfect{nullptr};
    SkinImage *selectionModDoubleTime{nullptr};
    SkinImage *selectionModNightCore{nullptr};
    SkinImage *selectionModHidden{nullptr};
    SkinImage *selectionModFlashlight{nullptr};
    SkinImage *selectionModRelax{nullptr};
    SkinImage *selectionModAutopilot{nullptr};
    SkinImage *selectionModSpunOut{nullptr};
    SkinImage *selectionModAutoplay{nullptr};
    SkinImage *selectionModNightmare{nullptr};
    SkinImage *selectionModTarget{nullptr};
    SkinImage *selectionModScorev2{nullptr};
    SkinImage *selectionModTD{nullptr};
    SkinImage *selectionModCinema{nullptr};

    SkinImage *mode_osu{nullptr};
    SkinImage *mode_osu_small{nullptr};

    Image *pauseContinue{MISSING_TEXTURE};
    Image *pauseReplay{MISSING_TEXTURE};
    Image *pauseRetry{MISSING_TEXTURE};
    Image *pauseBack{MISSING_TEXTURE};
    Image *pauseOverlay{MISSING_TEXTURE};
    Image *failBackground{MISSING_TEXTURE};
    Image *unpause{MISSING_TEXTURE};

    Image *buttonLeft{MISSING_TEXTURE};
    Image *buttonMiddle{MISSING_TEXTURE};
    Image *buttonRight{MISSING_TEXTURE};
    Image *defaultButtonLeft{MISSING_TEXTURE};
    Image *defaultButtonMiddle{MISSING_TEXTURE};
    Image *defaultButtonRight{MISSING_TEXTURE};
    SkinImage *menuBackImg{nullptr};
    SkinImage *selectionMode{nullptr};
    SkinImage *selectionModeOver{nullptr};
    SkinImage *selectionMods{nullptr};
    SkinImage *selectionModsOver{nullptr};
    SkinImage *selectionRandom{nullptr};
    SkinImage *selectionRandomOver{nullptr};
    SkinImage *selectionOptions{nullptr};
    SkinImage *selectionOptionsOver{nullptr};

    Image *songSelectTop{MISSING_TEXTURE};
    Image *songSelectBottom{MISSING_TEXTURE};
    Image *menuButtonBackground{MISSING_TEXTURE};
    SkinImage *menuButtonBackground2{nullptr};
    Image *star{MISSING_TEXTURE};
    Image *rankingPanel{MISSING_TEXTURE};
    Image *rankingGraph{MISSING_TEXTURE};
    Image *rankingTitle{MISSING_TEXTURE};
    Image *rankingMaxCombo{MISSING_TEXTURE};
    Image *rankingAccuracy{MISSING_TEXTURE};
    Image *rankingA{MISSING_TEXTURE};
    Image *rankingB{MISSING_TEXTURE};
    Image *rankingC{MISSING_TEXTURE};
    Image *rankingD{MISSING_TEXTURE};
    Image *rankingS{MISSING_TEXTURE};
    Image *rankingSH{MISSING_TEXTURE};
    Image *rankingX{MISSING_TEXTURE};
    Image *rankingXH{MISSING_TEXTURE};
    SkinImage *rankingAsmall{nullptr};
    SkinImage *rankingBsmall{nullptr};
    SkinImage *rankingCsmall{nullptr};
    SkinImage *rankingDsmall{nullptr};
    SkinImage *rankingSsmall{nullptr};
    SkinImage *rankingSHsmall{nullptr};
    SkinImage *rankingXsmall{nullptr};
    SkinImage *rankingXHsmall{nullptr};
    SkinImage *rankingPerfect{nullptr};

    Image *beatmapImportSpinner{MISSING_TEXTURE};
    Image *loadingSpinner{MISSING_TEXTURE};
    Image *circleEmpty{MISSING_TEXTURE};
    Image *circleFull{MISSING_TEXTURE};
    Image *seekTriangle{MISSING_TEXTURE};
    Image *userIcon{MISSING_TEXTURE};
    Image *backgroundCube{MISSING_TEXTURE};
    Image *menuBackground{MISSING_TEXTURE};
    Image *skybox{MISSING_TEXTURE};

    // colors
    std::vector<Color> comboColors;
    std::vector<Color> beatmapComboColors;
    Color spinnerApproachCircleColor;
    Color sliderBorderColor;
    Color sliderTrackOverride;
    Color sliderBallColor;

    Color songSelectInactiveText;
    Color songSelectActiveText;

    Color inputOverlayText;

    // scaling
    bool bCursor2x{false};
    bool bCursorTrail2x{false};
    bool bCursorRipple2x{false};
    bool bCursorSmoke2x{false};
    bool bApproachCircle2x{false};
    bool bReverseArrow2x{false};
    bool bHitCircle2x{false};
    bool bIsDefault02x{false};
    bool bIsDefault12x{false};
    bool bIsScore02x{false};
    bool bIsCombo02x{false};
    bool bSpinnerApproachCircle2x{false};
    bool bSpinnerBottom2x{false};
    bool bSpinnerCircle2x{false};
    bool bSpinnerTop2x{false};
    bool bSpinnerMiddle2x{false};
    bool bSpinnerMiddle22x{false};
    bool bSliderScorePoint2x{false};
    bool bSliderStartCircle2x{false};
    bool bSliderEndCircle2x{false};

    bool bCircularmetre2x{false};

    bool bPauseContinue2x{false};

    bool bMenuButtonBackground2x{false};
    bool bStar2x{false};
    bool bRankingPanel2x{false};
    bool bRankingMaxCombo2x{false};
    bool bRankingAccuracy2x{false};
    bool bRankingA2x{false};
    bool bRankingB2x{false};
    bool bRankingC2x{false};
    bool bRankingD2x{false};
    bool bRankingS2x{false};
    bool bRankingSH2x{false};
    bool bRankingX2x{false};
    bool bRankingXH2x{false};

    // skin.ini
    float fVersion{1.f};
    float fAnimationFramerate{0.f};
    bool bCursorCenter{true};
    bool bCursorRotate{true};
    bool bCursorExpand{true};
    bool bLayeredHitSounds{true};  // when true, hitnormal sounds must always be played regardless of map hitsound flags

    bool bSliderBallFlip{true};
    bool bAllowSliderBallTint{false};
    int iSliderStyle{2};
    bool bHitCircleOverlayAboveNumber{true};
    bool bSliderTrackOverride{false};

    std::string sComboPrefix;
    int iComboOverlap{0};

    std::string sScorePrefix;
    int iScoreOverlap{0};

    std::string sHitCirclePrefix;
    int iHitCircleOverlap{0};

    // custom
    std::vector<std::string> filepathsForRandomSkin;
    bool bIsRandom;
    bool bIsRandomElements;

    std::vector<std::string> filepathsForExport;

   private:
    // sounds
    Sound *normalHitNormal{nullptr};
    Sound *normalHitWhistle{nullptr};
    Sound *normalHitFinish{nullptr};
    Sound *normalHitClap{nullptr};

    Sound *normalSliderTick{nullptr};
    Sound *normalSliderSlide{nullptr};
    Sound *normalSliderWhistle{nullptr};

    Sound *softHitNormal{nullptr};
    Sound *softHitWhistle{nullptr};
    Sound *softHitFinish{nullptr};
    Sound *softHitClap{nullptr};

    Sound *softSliderTick{nullptr};
    Sound *softSliderSlide{nullptr};
    Sound *softSliderWhistle{nullptr};

    Sound *drumHitNormal{nullptr};
    Sound *drumHitWhistle{nullptr};
    Sound *drumHitFinish{nullptr};
    Sound *drumHitClap{nullptr};

    Sound *drumSliderTick{nullptr};
    Sound *drumSliderSlide{nullptr};
    Sound *drumSliderWhistle{nullptr};

    Sound *spinnerBonus{nullptr};
    Sound *spinnerSpinSound{nullptr};

    // Plays when sending a message in chat
    Sound *messageSent{nullptr};

    // Plays when deleting text in a message in chat
    Sound *deletingText{nullptr};

    // Plays when changing the text cursor position
    Sound *movingTextCursor{nullptr};

    // Plays when pressing a key for chat, search, edit, etc
    Sound *typing1{nullptr};
    Sound *typing2{nullptr};
    Sound *typing3{nullptr};
    Sound *typing4{nullptr};

    // Plays when returning to the previous screen
    Sound *menuBack{nullptr};

    // Plays when closing a chat tab
    Sound *closeChatTab{nullptr};

    // Plays when hovering above all selectable boxes except beatmaps or main screen buttons
    Sound *hoverButton{nullptr};

    // Plays when clicking to confirm a button or dropdown option, opening or
    // closing chat, switching between chat tabs, or switching groups
    Sound *clickButton{nullptr};

    // Main menu sounds
    Sound *clickMainMenuCube{nullptr};
    Sound *hoverMainMenuCube{nullptr};
    Sound *clickSingleplayer{nullptr};
    Sound *hoverSingleplayer{nullptr};
    Sound *clickMultiplayer{nullptr};
    Sound *hoverMultiplayer{nullptr};
    Sound *clickOptions{nullptr};
    Sound *hoverOptions{nullptr};
    Sound *clickExit{nullptr};
    Sound *hoverExit{nullptr};

    // Pause menu sounds
    Sound *pauseLoop{nullptr};
    Sound *pauseHover{nullptr};
    Sound *clickPauseBack{nullptr};
    Sound *hoverPauseBack{nullptr};
    Sound *clickPauseContinue{nullptr};
    Sound *hoverPauseContinue{nullptr};
    Sound *clickPauseRetry{nullptr};
    Sound *hoverPauseRetry{nullptr};

    // Back button sounds
    Sound *backButtonClick{nullptr};
    Sound *backButtonHover{nullptr};

    // Plays when switching into song selection, selecting a beatmap, opening dropdown boxes, opening chat tabs
    Sound *expand{nullptr};

    // Plays when selecting a difficulty of a beatmap
    Sound *selectDifficulty{nullptr};

    // Plays when changing the options via a slider
    Sound *sliderbar{nullptr};

    // Multiplayer sounds
    Sound *matchConfirm{nullptr};  // all players are ready
    Sound *roomJoined{nullptr};    // a player joined
    Sound *roomQuit{nullptr};      // a player left
    Sound *roomNotReady{nullptr};  // a player is no longer ready
    Sound *roomReady{nullptr};     // a player is now ready
    Sound *matchStart{nullptr};    // match started

    Sound *combobreak{nullptr};
    Sound *failsound{nullptr};
    Sound *applause{nullptr};
    Sound *menuHit{nullptr};
    Sound *menuHover{nullptr};
    Sound *checkOn{nullptr};
    Sound *checkOff{nullptr};
    Sound *shutter{nullptr};
    Sound *sectionPassSound{nullptr};
    Sound *sectionFailSound{nullptr};
};
