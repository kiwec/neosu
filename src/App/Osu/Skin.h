#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#include "noinclude.h"
#include "Color.h"
#include "Vectors.h"
#include "Image.h"

#include <array>
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

    // for lazy-loading "is @2x" checks, for non-animated skin images (which belong to the SkinImage class)
    struct BasicSkinImage {
        BasicSkinImage() = default;
        BasicSkinImage(Image *img) : img(img) {}

        Image *img{MISSING_TEXTURE};
        [[nodiscard]] bool is2x() const;

        inline Image *operator->() const noexcept { return img; }
        inline operator Image *() const noexcept { return img; }
        inline explicit operator bool() const noexcept { return !!img; }
        inline bool operator==(Image *other) const noexcept { return img == other; }

       private:
        mutable bool file_2x{false};
        mutable bool checked_2x{false};
    };

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
    [[nodiscard]] inline const BasicSkinImage &getHitCircle() const { return this->hitCircle; }
    [[nodiscard]] inline SkinImage *getHitCircleOverlay2() const { return this->hitCircleOverlay2; }
    [[nodiscard]] inline const BasicSkinImage &getApproachCircle() const { return this->approachCircle; }
    [[nodiscard]] inline const BasicSkinImage &getReverseArrow() const { return this->reverseArrow; }
    [[nodiscard]] inline SkinImage *getFollowPoint2() const { return this->followPoint2; }

    [[nodiscard]] inline const BasicSkinImage &getDefault0() const { return this->defaultNumImgs[0]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault1() const { return this->defaultNumImgs[1]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault2() const { return this->defaultNumImgs[2]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault3() const { return this->defaultNumImgs[3]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault4() const { return this->defaultNumImgs[4]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault5() const { return this->defaultNumImgs[5]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault6() const { return this->defaultNumImgs[6]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault7() const { return this->defaultNumImgs[7]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault8() const { return this->defaultNumImgs[8]; }
    [[nodiscard]] inline const BasicSkinImage &getDefault9() const { return this->defaultNumImgs[9]; }
    [[nodiscard]] inline const std::array<BasicSkinImage, 10> &getDefaultNumImgs() const {
        return this->defaultNumImgs;
    }

    [[nodiscard]] inline const BasicSkinImage &getScore0() const { return this->scoreNumImgs[0]; }
    [[nodiscard]] inline const BasicSkinImage &getScore1() const { return this->scoreNumImgs[1]; }
    [[nodiscard]] inline const BasicSkinImage &getScore2() const { return this->scoreNumImgs[2]; }
    [[nodiscard]] inline const BasicSkinImage &getScore3() const { return this->scoreNumImgs[3]; }
    [[nodiscard]] inline const BasicSkinImage &getScore4() const { return this->scoreNumImgs[4]; }
    [[nodiscard]] inline const BasicSkinImage &getScore5() const { return this->scoreNumImgs[5]; }
    [[nodiscard]] inline const BasicSkinImage &getScore6() const { return this->scoreNumImgs[6]; }
    [[nodiscard]] inline const BasicSkinImage &getScore7() const { return this->scoreNumImgs[7]; }
    [[nodiscard]] inline const BasicSkinImage &getScore8() const { return this->scoreNumImgs[8]; }
    [[nodiscard]] inline const BasicSkinImage &getScore9() const { return this->scoreNumImgs[9]; }
    [[nodiscard]] inline const BasicSkinImage &getScoreX() const { return this->scoreX; }
    [[nodiscard]] inline const BasicSkinImage &getScorePercent() const { return this->scorePercent; }
    [[nodiscard]] inline const BasicSkinImage &getScoreDot() const { return this->scoreDot; }
    [[nodiscard]] inline const std::array<BasicSkinImage, 10> &getScoreNumImgs() const { return this->scoreNumImgs; }

    [[nodiscard]] inline const BasicSkinImage &getCombo0() const { return this->comboNumImgs[0]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo1() const { return this->comboNumImgs[1]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo2() const { return this->comboNumImgs[2]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo3() const { return this->comboNumImgs[3]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo4() const { return this->comboNumImgs[4]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo5() const { return this->comboNumImgs[5]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo6() const { return this->comboNumImgs[6]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo7() const { return this->comboNumImgs[7]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo8() const { return this->comboNumImgs[8]; }
    [[nodiscard]] inline const BasicSkinImage &getCombo9() const { return this->comboNumImgs[9]; }
    [[nodiscard]] inline const BasicSkinImage &getComboX() const { return this->comboX; }
    [[nodiscard]] inline const std::array<BasicSkinImage, 10> &getComboNumImgs() const { return this->comboNumImgs; }

    [[nodiscard]] inline SkinImage *getPlaySkip() const { return this->playSkip; }
    [[nodiscard]] inline const BasicSkinImage &getPlayWarningArrow() const { return this->playWarningArrow; }
    [[nodiscard]] inline SkinImage *getPlayWarningArrow2() const { return this->playWarningArrow2; }
    [[nodiscard]] inline const BasicSkinImage &getCircularmetre() const { return this->circularmetre; }
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

    [[nodiscard]] inline const BasicSkinImage &getParticle50() const { return this->particle50; }
    [[nodiscard]] inline const BasicSkinImage &getParticle100() const { return this->particle100; }
    [[nodiscard]] inline const BasicSkinImage &getParticle300() const { return this->particle300; }

    [[nodiscard]] inline const BasicSkinImage &getSliderGradient() const { return this->sliderGradient; }
    [[nodiscard]] inline SkinImage *getSliderb() const { return this->sliderb; }
    [[nodiscard]] inline SkinImage *getSliderFollowCircle2() const { return this->sliderFollowCircle2; }
    [[nodiscard]] inline const BasicSkinImage &getSliderScorePoint() const { return this->sliderScorePoint; }
    [[nodiscard]] inline const BasicSkinImage &getSliderStartCircle() const { return this->sliderStartCircle; }
    [[nodiscard]] inline SkinImage *getSliderStartCircle2() const { return this->sliderStartCircle2; }
    [[nodiscard]] inline const BasicSkinImage &getSliderStartCircleOverlay() const {
        return this->sliderStartCircleOverlay;
    }
    [[nodiscard]] inline SkinImage *getSliderStartCircleOverlay2() const { return this->sliderStartCircleOverlay2; }
    [[nodiscard]] inline const BasicSkinImage &getSliderEndCircle() const { return this->sliderEndCircle; }
    [[nodiscard]] inline SkinImage *getSliderEndCircle2() const { return this->sliderEndCircle2; }
    [[nodiscard]] inline const BasicSkinImage &getSliderEndCircleOverlay() const {
        return this->sliderEndCircleOverlay;
    }
    [[nodiscard]] inline SkinImage *getSliderEndCircleOverlay2() const { return this->sliderEndCircleOverlay2; }

    [[nodiscard]] inline const BasicSkinImage &getSpinnerApproachCircle() const { return this->spinnerApproachCircle; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerBackground() const { return this->spinnerBackground; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerCircle() const { return this->spinnerCircle; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerClear() const { return this->spinnerClear; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerBottom() const { return this->spinnerBottom; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerGlow() const { return this->spinnerGlow; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerMetre() const { return this->spinnerMetre; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerMiddle() const { return this->spinnerMiddle; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerMiddle2() const { return this->spinnerMiddle2; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerOsu() const { return this->spinnerOsu; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerRpm() const { return this->spinnerRpm; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerSpin() const { return this->spinnerSpin; }
    [[nodiscard]] inline const BasicSkinImage &getSpinnerTop() const { return this->spinnerTop; }

    [[nodiscard]] inline const BasicSkinImage &getDefaultCursor() const { return this->defaultCursor; }
    [[nodiscard]] inline const BasicSkinImage &getCursor() const { return this->cursor; }
    [[nodiscard]] inline const BasicSkinImage &getCursorMiddle() const { return this->cursorMiddle; }
    [[nodiscard]] inline const BasicSkinImage &getCursorTrail() const { return this->cursorTrail; }
    [[nodiscard]] inline const BasicSkinImage &getCursorRipple() const { return this->cursorRipple; }
    [[nodiscard]] inline const BasicSkinImage &getCursorSmoke() const { return this->cursorSmoke; }

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

    [[nodiscard]] inline const BasicSkinImage &getPauseContinue() const { return this->pauseContinue; }
    [[nodiscard]] inline const BasicSkinImage &getPauseRetry() const { return this->pauseRetry; }
    [[nodiscard]] inline const BasicSkinImage &getPauseBack() const { return this->pauseBack; }
    [[nodiscard]] inline const BasicSkinImage &getPauseOverlay() const { return this->pauseOverlay; }
    [[nodiscard]] inline const BasicSkinImage &getFailBackground() const { return this->failBackground; }
    [[nodiscard]] inline const BasicSkinImage &getUnpause() const { return this->unpause; }

    [[nodiscard]] inline const BasicSkinImage &getButtonLeft() const { return this->buttonLeft; }
    [[nodiscard]] inline const BasicSkinImage &getButtonMiddle() const { return this->buttonMiddle; }
    [[nodiscard]] inline const BasicSkinImage &getButtonRight() const { return this->buttonRight; }
    [[nodiscard]] inline const BasicSkinImage &getDefaultButtonLeft() const { return this->defaultButtonLeft; }
    [[nodiscard]] inline const BasicSkinImage &getDefaultButtonMiddle() const { return this->defaultButtonMiddle; }
    [[nodiscard]] inline const BasicSkinImage &getDefaultButtonRight() const { return this->defaultButtonRight; }
    [[nodiscard]] inline SkinImage *getMenuBack2() const { return this->menuBackImg; }

    [[nodiscard]] inline const BasicSkinImage &getMenuButtonBackground() const { return this->menuButtonBackground; }
    [[nodiscard]] inline SkinImage *getMenuButtonBackground2() const { return this->menuButtonBackground2; }
    [[nodiscard]] inline const BasicSkinImage &getStar() const { return this->star; }
    [[nodiscard]] inline const BasicSkinImage &getRankingPanel() const { return this->rankingPanel; }
    [[nodiscard]] inline const BasicSkinImage &getRankingGraph() const { return this->rankingGraph; }
    [[nodiscard]] inline const BasicSkinImage &getRankingTitle() const { return this->rankingTitle; }
    [[nodiscard]] inline const BasicSkinImage &getRankingMaxCombo() const { return this->rankingMaxCombo; }
    [[nodiscard]] inline const BasicSkinImage &getRankingAccuracy() const { return this->rankingAccuracy; }
    [[nodiscard]] inline const BasicSkinImage &getRankingA() const { return this->rankingA; }
    [[nodiscard]] inline const BasicSkinImage &getRankingB() const { return this->rankingB; }
    [[nodiscard]] inline const BasicSkinImage &getRankingC() const { return this->rankingC; }
    [[nodiscard]] inline const BasicSkinImage &getRankingD() const { return this->rankingD; }
    [[nodiscard]] inline const BasicSkinImage &getRankingS() const { return this->rankingS; }
    [[nodiscard]] inline const BasicSkinImage &getRankingSH() const { return this->rankingSH; }
    [[nodiscard]] inline const BasicSkinImage &getRankingX() const { return this->rankingX; }
    [[nodiscard]] inline const BasicSkinImage &getRankingXH() const { return this->rankingXH; }
    [[nodiscard]] inline SkinImage *getRankingAsmall() const { return this->rankingAsmall; }
    [[nodiscard]] inline SkinImage *getRankingBsmall() const { return this->rankingBsmall; }
    [[nodiscard]] inline SkinImage *getRankingCsmall() const { return this->rankingCsmall; }
    [[nodiscard]] inline SkinImage *getRankingDsmall() const { return this->rankingDsmall; }
    [[nodiscard]] inline SkinImage *getRankingSsmall() const { return this->rankingSsmall; }
    [[nodiscard]] inline SkinImage *getRankingSHsmall() const { return this->rankingSHsmall; }
    [[nodiscard]] inline SkinImage *getRankingXsmall() const { return this->rankingXsmall; }
    [[nodiscard]] inline SkinImage *getRankingXHsmall() const { return this->rankingXHsmall; }
    [[nodiscard]] inline SkinImage *getRankingPerfect() const { return this->rankingPerfect; }

    [[nodiscard]] inline const BasicSkinImage &getBeatmapImportSpinner() const { return this->beatmapImportSpinner; }
    [[nodiscard]] inline const BasicSkinImage &getLoadingSpinner() const { return this->loadingSpinner; }
    [[nodiscard]] inline const BasicSkinImage &getCircleEmpty() const { return this->circleEmpty; }
    [[nodiscard]] inline const BasicSkinImage &getCircleFull() const { return this->circleFull; }
    [[nodiscard]] inline const BasicSkinImage &getSeekTriangle() const { return this->seekTriangle; }
    [[nodiscard]] inline const BasicSkinImage &getUserIcon() const { return this->userIcon; }
    [[nodiscard]] inline const BasicSkinImage &getBackgroundCube() const { return this->backgroundCube; }
    [[nodiscard]] inline const BasicSkinImage &getMenuBackground() const { return this->menuBackground; }
    [[nodiscard]] inline const BasicSkinImage &getSkybox() const { return this->skybox; }

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
    [[nodiscard]] inline bool useSmoothCursorTrail() const { return this->cursorMiddle.img != MISSING_TEXTURE; }
    [[nodiscard]] inline bool isDefaultSkin() const { return this->bIsDefaultSkin; }

    bool parseSkinINI(std::string filepath);

    SkinImage *createSkinImage(const std::string &skinElementName, vec2 baseSizeForScaling2x, float osuSize,
                               bool ignoreDefaultSkin = false, const std::string &animationSeparator = "-");
    void checkLoadImage(BasicSkinImage &imgRef, const std::string &skinElementName, const std::string &resourceName,
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
    BasicSkinImage hitCircle{};
    SkinImage *hitCircleOverlay2{nullptr};
    BasicSkinImage approachCircle{};
    BasicSkinImage reverseArrow{};
    SkinImage *followPoint2{nullptr};

    std::array<BasicSkinImage, 10> defaultNumImgs{};
    std::array<BasicSkinImage, 10> scoreNumImgs{};

    BasicSkinImage scoreX{};
    BasicSkinImage scorePercent{};
    BasicSkinImage scoreDot{};

    std::array<BasicSkinImage, 10> comboNumImgs{};

    BasicSkinImage comboX{};

    SkinImage *playSkip{nullptr};
    BasicSkinImage playWarningArrow{};
    SkinImage *playWarningArrow2{nullptr};
    BasicSkinImage circularmetre{};
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

    BasicSkinImage particle50{};
    BasicSkinImage particle100{};
    BasicSkinImage particle300{};

    BasicSkinImage sliderGradient{};
    SkinImage *sliderb{nullptr};
    SkinImage *sliderFollowCircle2{nullptr};
    BasicSkinImage sliderScorePoint{};
    BasicSkinImage sliderStartCircle{};
    SkinImage *sliderStartCircle2{nullptr};
    BasicSkinImage sliderStartCircleOverlay{};
    SkinImage *sliderStartCircleOverlay2{nullptr};
    BasicSkinImage sliderEndCircle{};
    SkinImage *sliderEndCircle2{nullptr};
    BasicSkinImage sliderEndCircleOverlay{};
    SkinImage *sliderEndCircleOverlay2{nullptr};

    BasicSkinImage spinnerApproachCircle{};
    BasicSkinImage spinnerBackground{};
    BasicSkinImage spinnerCircle{};
    BasicSkinImage spinnerClear{};
    BasicSkinImage spinnerBottom{};
    BasicSkinImage spinnerGlow{};
    BasicSkinImage spinnerMetre{};
    BasicSkinImage spinnerMiddle{};
    BasicSkinImage spinnerMiddle2{};
    BasicSkinImage spinnerOsu{};
    BasicSkinImage spinnerTop{};
    BasicSkinImage spinnerRpm{};
    BasicSkinImage spinnerSpin{};

    BasicSkinImage defaultCursor{};
    BasicSkinImage cursor{};
    BasicSkinImage cursorMiddle{};
    BasicSkinImage cursorTrail{};
    BasicSkinImage cursorRipple{};
    BasicSkinImage cursorSmoke{};

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

    BasicSkinImage pauseContinue{};
    BasicSkinImage pauseReplay{};
    BasicSkinImage pauseRetry{};
    BasicSkinImage pauseBack{};
    BasicSkinImage pauseOverlay{};
    BasicSkinImage failBackground{};
    BasicSkinImage unpause{};

    BasicSkinImage buttonLeft{};
    BasicSkinImage buttonMiddle{};
    BasicSkinImage buttonRight{};
    BasicSkinImage defaultButtonLeft{};
    BasicSkinImage defaultButtonMiddle{};
    BasicSkinImage defaultButtonRight{};
    SkinImage *menuBackImg{nullptr};
    SkinImage *selectionMode{nullptr};
    SkinImage *selectionModeOver{nullptr};
    SkinImage *selectionMods{nullptr};
    SkinImage *selectionModsOver{nullptr};
    SkinImage *selectionRandom{nullptr};
    SkinImage *selectionRandomOver{nullptr};
    SkinImage *selectionOptions{nullptr};
    SkinImage *selectionOptionsOver{nullptr};

    BasicSkinImage songSelectTop{};
    BasicSkinImage songSelectBottom{};
    BasicSkinImage menuButtonBackground{};
    SkinImage *menuButtonBackground2{nullptr};
    BasicSkinImage star{};
    BasicSkinImage rankingPanel{};
    BasicSkinImage rankingGraph{};
    BasicSkinImage rankingTitle{};
    BasicSkinImage rankingMaxCombo{};
    BasicSkinImage rankingAccuracy{};
    BasicSkinImage rankingA{};
    BasicSkinImage rankingB{};
    BasicSkinImage rankingC{};
    BasicSkinImage rankingD{};
    BasicSkinImage rankingS{};
    BasicSkinImage rankingSH{};
    BasicSkinImage rankingX{};
    BasicSkinImage rankingXH{};
    SkinImage *rankingAsmall{nullptr};
    SkinImage *rankingBsmall{nullptr};
    SkinImage *rankingCsmall{nullptr};
    SkinImage *rankingDsmall{nullptr};
    SkinImage *rankingSsmall{nullptr};
    SkinImage *rankingSHsmall{nullptr};
    SkinImage *rankingXsmall{nullptr};
    SkinImage *rankingXHsmall{nullptr};
    SkinImage *rankingPerfect{nullptr};

    BasicSkinImage beatmapImportSpinner{};
    BasicSkinImage loadingSpinner{};
    BasicSkinImage circleEmpty{};
    BasicSkinImage circleFull{};
    BasicSkinImage seekTriangle{};
    BasicSkinImage userIcon{};
    BasicSkinImage backgroundCube{};
    BasicSkinImage menuBackground{};
    BasicSkinImage skybox{};

    // colors
    std::vector<Color> comboColors;
    std::vector<Color> beatmapComboColors;
    Color spinnerApproachCircleColor;
    Color spinnerBackgroundColor;
    Color sliderBorderColor;
    Color sliderTrackOverride;
    Color sliderBallColor;

    Color songSelectInactiveText;
    Color songSelectActiveText;

    Color inputOverlayText;

    // skin.ini
    float fVersion{1.f};
    float fAnimationFramerate{0.f};
    bool bCursorCenter{true};
    bool bCursorRotate{true};
    bool bCursorExpand{true};
    bool bLayeredHitSounds{true};  // when true, hitnormal sounds must always be played regardless of map hitsound flags
    bool bSpinnerFadePlayfield{false};     // Should the spinner add black bars during spins
    bool bSpinnerFrequencyModulate{true};  // Should the spinnerspin sound pitch up the longer the spinner goes
    bool bSpinnerNoBlink{false};           // Should the highest bar of the metre stay visible all the time

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
