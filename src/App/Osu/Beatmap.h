#pragma once
// Copyright (c) 2015, PG, All rights reserved.

#include "BeatmapInterface.h"
#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "HUD.h"
#include "LegacyReplay.h"
#include "score.h"
#include "uwu.h"

class Sound;
class ConVar;
class Skin;
class HitObject;
class DatabaseBeatmap;
class SimulatedBeatmap;
struct LiveReplayFrame;
struct ScoreFrame;

struct Click {
    long click_time;
    vec2 pos{0.f};
};

class Beatmap : public BeatmapInterface {
   public:
    Beatmap();
    ~Beatmap() override;

    void draw();
    void drawDebug();
    void drawBackground();

    void update();
    void update2();  // Used to be Beatmap::update()

    // Potentially Visible Set gate time size, for optimizing draw() and update() when iterating over all hitobjects
    long getPVS();

    // this should make all the necessary internal updates to hitobjects when legacy osu mods or static mods change
    // live (but also on start)
    void onModUpdate(bool rebuildSliderVertexBuffers = true, bool recomputeDrainRate = true);

    // HACK: Updates buffering state and pauses/unpauses the music!
    bool isBuffering();

    // Returns true if we're loading or waiting on other players
    bool isLoading();

    // Returns true if the local player is loading
    bool isActuallyLoading();

    [[nodiscard]] vec2 pixels2OsuCoords(vec2 pixelCoords) const override;  // only used for positional audio atm
    [[nodiscard]] vec2 osuCoords2Pixels(
        vec2 coords) const override;  // hitobjects should use this one (includes lots of special behaviour)
    [[nodiscard]] vec2 osuCoords2RawPixels(vec2 coords)
        const override;  // raw transform from osu!pixels to absolute screen pixels (without any mods whatsoever)
    [[nodiscard]] vec2 osuCoords2LegacyPixels(vec2 coords)
        const override;  // only applies vanilla osu mods and static mods to the coordinates (used for generating
                         // the static slider mesh) centered at (0, 0, 0)

    // cursor
    [[nodiscard]] vec2 getMousePos() const;
    [[nodiscard]] vec2 getCursorPos() const override;
    [[nodiscard]] vec2 getFirstPersonCursorDelta() const;
    [[nodiscard]] inline vec2 getContinueCursorPoint() const { return this->vContinueCursorPoint; }

    // playfield
    [[nodiscard]] inline vec2 getPlayfieldSize() const { return this->vPlayfieldSize; }
    [[nodiscard]] inline vec2 getPlayfieldCenter() const { return this->vPlayfieldCenter; }
    [[nodiscard]] inline f32 getPlayfieldRotation() const { return this->fPlayfieldRotation; }

    // hitobjects
    [[nodiscard]] inline f32 getHitcircleXMultiplier() const {
        return this->fXMultiplier;
    }  // multiply osu!pixels with this to get screen pixels
    [[nodiscard]] inline f32 getNumberScale() const { return this->fNumberScale; }
    [[nodiscard]] inline f32 getHitcircleOverlapScale() const { return this->fHitcircleOverlapScale; }
    [[nodiscard]] inline bool isInMafhamRenderChunk() const { return this->bInMafhamRenderChunk; }

    // score
    [[nodiscard]] inline int getNumHitObjects() const { return this->hitobjects.size(); }
    [[nodiscard]] inline f32 getAimStars() const { return this->fAimStars; }
    [[nodiscard]] inline f32 getAimSliderFactor() const { return this->fAimSliderFactor; }
    [[nodiscard]] inline f32 getSpeedStars() const { return this->fSpeedStars; }
    [[nodiscard]] inline f32 getSpeedNotes() const { return this->fSpeedNotes; }
    [[nodiscard]] f32 getSpeedMultiplier() const override;

    // hud
    [[nodiscard]] inline bool isSpinnerActive() const { return this->bIsSpinnerActive; }

    // callbacks called by the Osu class (osu!standard)
    void skipEmptySection();
    void keyPressed1(bool mouse);
    void keyPressed2(bool mouse);
    void keyReleased1(bool mouse);
    void keyReleased2(bool mouse);

    // songbrowser & player logic
    void select();  // loads the music of the currently selected diff and starts playing from the previewTime (e.g.
                    // clicking on a beatmap)
    void selectDifficulty2(DatabaseBeatmap *difficulty2);
    void deselect();  // stops + unloads the currently loaded music and deletes all hitobjects

    bool play();
    bool watch(FinishedScore score, f64 start_percent);
    bool spectate();

    bool start();
    void restart(bool quick = false);
    void pause(bool quitIfWaiting = true);
    void pausePreviewMusic(bool toggle = true);
    bool isPreviewMusicPlaying();
    void stop(bool quit = true);
    void fail(bool force_death = false);
    void cancelFailing();
    void resetScore();

    // music/sound
    void loadMusic(bool stream = true);
    void unloadMusic();
    f32 getIdealVolume();
    void setSpeed(f32 speed);
    void seekPercent(f64 percent);
    void seekPercentPlayable(f64 percent);

    [[nodiscard]] inline Sound *getMusic() const { return this->music; }
    [[nodiscard]] u32 getTime() const;
    [[nodiscard]] u32 getStartTimePlayable() const;
    [[nodiscard]] u32 getLength() const override;
    [[nodiscard]] u32 getLengthPlayable() const override;
    [[nodiscard]] f32 getPercentFinished() const;
    [[nodiscard]] f32 getPercentFinishedPlayable() const;

    // live statistics
    [[nodiscard]] int getMostCommonBPM() const;
    [[nodiscard]] inline int getNPS() const { return this->iNPS; }
    [[nodiscard]] inline int getND() const { return this->iND; }

    std::vector<f64> aimStrains;
    std::vector<f64> speedStrains;

    // set to false when using non-vanilla mods (disables score submission)
    bool vanilla = true;

    // replay recording
    void write_frame();
    std::vector<LegacyReplay::Frame> live_replay;
    f64 last_event_time = 0.0;
    i32 last_event_ms = 0;
    u8 current_keys = 0;
    u8 last_keys = 0;

    // replay replaying (prerecorded)
    // current_keys, last_keys also reused
    std::vector<LegacyReplay::Frame> spectated_replay;
    vec2 interpolatedMousePos{0.f};
    bool is_watching = false;
    long current_frame_idx = 0;
    SimulatedBeatmap *sim = nullptr;

    // getting spectated (live)
    void broadcast_spectator_frames();
    std::vector<LiveReplayFrame> frame_batch;
    f64 last_spectator_broadcast = 0;
    u16 spectator_sequence = 0;

    // spectating (live)
    std::vector<ScoreFrame> score_frames;
    bool is_buffering = false;
    i32 last_frame_ms = 0;
    bool spectate_pause = false;  // the player we're spectating has paused

    // used by HitObject children and ModSelector
    [[nodiscard]] Skin *getSkin() const;  // maybe use this for beatmap skins, maybe

    [[nodiscard]] inline long getCurMusicPos() const { return this->iCurMusicPos; }
    [[nodiscard]] inline long getCurMusicPosWithOffsets() const { return this->iCurMusicPosWithOffsets; }

    // health
    [[nodiscard]] inline f64 getHealth() const { return this->fHealth; }
    [[nodiscard]] inline bool hasFailed() const { return this->bFailed; }

    // database (legacy)
    [[nodiscard]] inline DatabaseBeatmap *getSelectedDifficulty2() const { return this->selectedDifficulty2; }

    // generic state
    [[nodiscard]] inline bool isPlaying() const override { return this->bIsPlaying; }
    [[nodiscard]] inline bool isPaused() const override { return this->bIsPaused; }
    [[nodiscard]] inline bool isRestartScheduled() const { return this->bIsRestartScheduled; }
    [[nodiscard]] inline bool isContinueScheduled() const override { return this->bContinueScheduled; }
    [[nodiscard]] inline bool isInSkippableSection() const { return this->bIsInSkippableSection; }
    [[nodiscard]] inline bool isInBreak() const { return this->bInBreak; }
    [[nodiscard]] inline bool shouldFlashWarningArrows() const { return this->bShouldFlashWarningArrows; }
    [[nodiscard]] inline f32 shouldFlashSectionPass() const { return this->fShouldFlashSectionPass; }
    [[nodiscard]] inline f32 shouldFlashSectionFail() const { return this->fShouldFlashSectionFail; }
    [[nodiscard]] bool isWaiting() const override { return this->bIsWaiting; }
    [[nodiscard]] bool isKey1Down() const override;
    [[nodiscard]] bool isKey2Down() const override;
    [[nodiscard]] bool isClickHeld() const override;

    [[nodiscard]] std::string getTitle() const;
    [[nodiscard]] std::string getArtist() const;

    [[nodiscard]] inline const std::vector<DatabaseBeatmap::BREAK> &getBreaks() const { return this->breaks; }
    [[nodiscard]] u32 getBreakDurationTotal() const override;
    [[nodiscard]] DatabaseBeatmap::BREAK getBreakForTimeRange(i64 startMS, i64 positionMS, i64 endMS) const;

    // HitObject and other helper functions
    LiveScore::HIT addHitResult(HitObject *hitObject, LiveScore::HIT hit, i32 delta, bool isEndOfCombo = false,
                                bool ignoreOnHitErrorBar = false, bool hitErrorBarOnly = false,
                                bool ignoreCombo = false, bool ignoreScore = false, bool ignoreHealth = false) override;
    void addSliderBreak() override;
    void addScorePoints(int points, bool isSpinner = false) override;
    void addHealth(f64 percent, bool isFromHitResult);
    void updateTimingPoints(long curPos);

    static bool sortHitObjectByStartTimeComp(HitObject const *a, HitObject const *b);
    static bool sortHitObjectByEndTimeComp(HitObject const *a, HitObject const *b);

    // ILLEGAL:
    [[nodiscard]] inline const std::vector<HitObject *> &getHitObjectsPointer() const { return this->hitobjects; }
    [[nodiscard]] inline f32 getBreakBackgroundFadeAnim() const { return this->fBreakBackgroundFade; }

    Sound *music;
    bool bForceStreamPlayback;

    // live pp/stars
    uwu::lazy_promise<std::function<pp_info()>, pp_info> ppv2_calc{pp_info{}};
    i32 last_calculated_hitobject = -1;
    int iCurrentHitObjectIndex;
    int iCurrentNumCircles;
    int iCurrentNumSliders;
    int iCurrentNumSpinners;

    // beatmap state
    bool bIsPlaying;
    bool bIsPaused;
    bool bIsWaiting;
    bool bIsRestartScheduled;
    bool bIsRestartScheduledQuick;

   protected:
    // internal
    bool canDraw();

    void actualRestart();

    void handlePreviewPlay();
    void unloadObjects();

    void resetHitObjects(long curPos = 0);

    void playMissSound();

    u32 getMusicPositionMSInterpolated();

    bool bIsInSkippableSection;
    bool bShouldFlashWarningArrows;
    f32 fShouldFlashSectionPass;
    f32 fShouldFlashSectionFail;
    bool bContinueScheduled;
    u32 iContinueMusicPos;
    f32 fWaitTime;

    // database
    DatabaseBeatmap *selectedDifficulty2;

    // sound
    f32 fMusicFrequencyBackup;
    long iCurMusicPos;
    long iCurMusicPosWithOffsets;
    bool bWasSeekFrame;
    f64 fInterpolatedMusicPos;
    f64 fLastAudioTimeAccurateSet;
    f64 fLastRealTimeForInterpolationDelta;
    int iResourceLoadUpdateDelayHack;
    f32 fAfterMusicIsFinishedVirtualAudioTimeStart;
    bool bIsFirstMissSound;

    // health
    bool bFailed;
    f32 fFailAnim;
    f64 fHealth;
    f32 fHealth2;

    // drain
    f64 fDrainRate;

    // breaks
    std::vector<DatabaseBeatmap::BREAK> breaks;
    f32 fBreakBackgroundFade;
    bool bInBreak;
    HitObject *currentHitObject;
    long iNextHitObjectTime;
    long iPreviousHitObjectTime;
    long iPreviousSectionPassFailTime;

    // player input
    bool bClick1Held;
    bool bClick2Held;
    bool bClickedContinue;
    int iAllowAnyNextKeyForFullAlternateUntilHitObjectIndex;
    std::vector<Click> clicks;

    // hitobjects
    std::vector<HitObject *> hitobjects;
    std::vector<HitObject *> hitobjectsSortedByEndTime;
    std::vector<HitObject *> misaimObjects;

    // statistics
    int iNPS;
    int iND;

    // custom
    int iPreviousFollowPointObjectIndex;  // TODO: this shouldn't be in this class

   private:
    [[nodiscard]] u32 getScoreV1DifficultyMultiplier_full() const override;
    [[nodiscard]] Replay::Mods getMods_full() const override;
    [[nodiscard]] u32 getModsLegacy_full() const override;
    [[nodiscard]] f32 getRawAR_full() const override;
    [[nodiscard]] f32 getAR_full() const override;
    [[nodiscard]] f32 getCS_full() const override;
    [[nodiscard]] f32 getHP_full() const override;
    [[nodiscard]] f32 getRawOD_full() const override;
    [[nodiscard]] f32 getOD_full() const override;
    [[nodiscard]] f32 getApproachTime_full() const override;
    [[nodiscard]] f32 getRawApproachTime_full() const override;

    static inline vec2 mapNormalizedCoordsOntoUnitCircle(const vec2 &in) {
        return vec2(in.x * std::sqrt(1.0f - in.y * in.y / 2.0f), in.y * std::sqrt(1.0f - in.x * in.x / 2.0f));
    }

    static f32 quadLerp3f(f32 left, f32 center, f32 right, f32 percent) {
        if(percent >= 0.5f) {
            percent = (percent - 0.5f) / 0.5f;
            percent *= percent;
            return std::lerp(center, right, percent);
        } else {
            percent = percent / 0.5f;
            percent = 1.0f - (1.0f - percent) * (1.0f - percent);
            return std::lerp(left, center, percent);
        }
    }

    FinishedScore saveAndSubmitScore(bool quit);

    void drawFollowPoints();
    void drawHitObjects();
    void drawSmoke();

    void updateAutoCursorPos();
    void updatePlayfieldMetrics();
    void updateHitobjectMetrics();
    void updateSliderVertexBuffers();

    void calculateStacks();
    void computeDrainRate();

    // beatmap
    bool bIsSpinnerActive;
    vec2 vContinueCursorPoint{0.f};

    // playfield
    f32 fPlayfieldRotation;
    f32 fScaleFactor;
    vec2 vPlayfieldCenter{0.f};
    vec2 vPlayfieldOffset{0.f};
    vec2 vPlayfieldSize{0.f};

    // hitobject scaling
    f32 fXMultiplier;
    f32 fNumberScale;
    f32 fHitcircleOverlapScale;

    // auto
    vec2 vAutoCursorPos{0.f};
    int iAutoCursorDanceIndex;

    // live pp/stars
    void resetLiveStarsTasks();

    // pp calculation buffer (only needs to be recalculated in onModUpdate(), instead of on every hit)
    f32 fAimStars;
    f32 fAimSliderFactor;
    f32 fSpeedStars;
    f32 fSpeedNotes;

    // dynamic slider vertex buffer and other recalculation checks (for live mod switching)
    f32 fPrevHitCircleDiameter;
    bool bWasHorizontalMirrorEnabled;
    bool bWasVerticalMirrorEnabled;
    bool bWasEZEnabled;
    bool bWasMafhamEnabled;
    f32 fPrevPlayfieldRotationFromConVar;

    // custom
    bool bIsPreLoading;
    int iPreLoadingIndex;
    bool bWasHREnabled;  // dynamic stack recalculation

    RenderTarget *mafhamActiveRenderTarget;
    RenderTarget *mafhamFinishedRenderTarget;
    bool bMafhamRenderScheduled;
    int iMafhamHitObjectRenderIndex;  // scene buffering for rendering entire beatmaps at once with an acceptable
                                      // framerate
    int iMafhamPrevHitObjectIndex;
    int iMafhamActiveRenderHitObjectIndex;
    int iMafhamFinishedRenderHitObjectIndex;
    bool bInMafhamRenderChunk;  // used by Slider to not animate the reverse arrow, and by Circle to not animate
                                // note blocking shaking, while being rendered into the scene buffer

    struct SMOKETRAIL {
        vec2 pos{0.f};
        i64 time;
    };
    std::vector<SMOKETRAIL> smoke_trail;
};
