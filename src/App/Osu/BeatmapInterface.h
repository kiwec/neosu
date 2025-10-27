#pragma once
// Copyright (c) 2015, PG, All rights reserved.

#include "AbstractBeatmapInterface.h"
#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "HUD.h"
#include "LeaderboardPPCalcThread.h"
#include "LegacyReplay.h"
#include "PlaybackInterpolator.h"
#include "score.h"
#include "uwu.h"

class Sound;
class ConVar;
class Skin;
class HitObject;
class DatabaseBeatmap;
class SimulatedBeatmapInterface;
struct LiveReplayFrame;
struct ScoreFrame;

struct Click {
    i32 timestamp;  // current music position when the click happened
    vec2 pos{0.f};
};

class BeatmapInterface final : public AbstractBeatmapInterface {
    NOCOPY_NOMOVE(BeatmapInterface)
   public:
    BeatmapInterface();
    ~BeatmapInterface() override;

    void draw();
    void drawDebug();
    void drawBackground();

    void update();
    void update2();  // Used to be Playfield::update()

    // Potentially Visible Set gate time size, for optimizing draw() and update() when iterating over all hitobjects
    i32 getPVS();

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

    const pp_info &getWholeMapPPInfo();

    // hud
    [[nodiscard]] inline bool isSpinnerActive() const { return this->bIsSpinnerActive; }

    // callbacks called by the Osu class (osu!standard)
    void skipEmptySection();
    void keyPressed1(bool mouse);
    void keyPressed2(bool mouse);
    void keyReleased1(bool mouse);
    void keyReleased2(bool mouse);

    // loads the music of the currently selected diff and starts playing from the previewTime (e.g. clicking on a beatmap)
    void selectBeatmap();
    void selectBeatmap(DatabaseBeatmap *map);
    [[nodiscard]] inline DatabaseBeatmap *getBeatmap() const { return this->beatmap; }

    // stops + unloads the currently loaded music and deletes all hitobjects
    void deselectBeatmap();

    bool play();
    bool watch(const FinishedScore &score, u32 start_ms);
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
    inline void reloadMusicNow() { this->loadMusic(true, false); }
    void loadMusic(bool reload = false, bool async = false);
    void unloadMusic();
    f32 getIdealVolume();
    void setSpeed(f32 speed);
    void seekMS(u32 ms);
    [[nodiscard]] inline DatabaseBeatmap::TIMING_INFO getTimingPoint() const { return this->current_timing_point; }
    [[nodiscard]] inline i32 getDefaultSampleSet() const { return this->default_sample_set; }

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

    // set to false when using protected cvars
    bool is_submittable = true;

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
    i32 current_frame_idx = 0;
    SimulatedBeatmapInterface *sim = nullptr;

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
    [[nodiscard]] const std::unique_ptr<Skin> &getSkin() const;  // maybe use this for beatmap skins, maybe

    [[nodiscard]] inline i32 getCurMusicPos() const { return this->iCurMusicPos; }
    [[nodiscard]] inline i32 getCurMusicPosWithOffsets() const { return this->iCurMusicPosWithOffsets; }

    // health
    [[nodiscard]] inline f64 getHealth() const { return this->fHealth; }
    [[nodiscard]] inline bool hasFailed() const { return this->bFailed; }

    // generic state
    [[nodiscard]] u8 getKeys() const override { return this->current_keys; }
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

    static bool sortHitObjectByStartTimeComp(HitObject const *a, HitObject const *b);
    static bool sortHitObjectByEndTimeComp(HitObject const *a, HitObject const *b);

    // ILLEGAL:
    [[nodiscard]] inline const std::vector<HitObject *> &getHitObjectsPointer() const { return this->hitobjects; }
    [[nodiscard]] inline f32 getBreakBackgroundFadeAnim() const { return this->fBreakBackgroundFade; }

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
    bool bWasSeekFrame;
    bool bTempSeekNF{false};

   protected:
    // internal
    bool canDraw();

    void actualRestart();

    void handlePreviewPlay();
    void unloadObjects();

    void resetHitObjects(i32 curPos = 0);

    void playMissSound();

    bool bIsInSkippableSection;
    bool bShouldFlashWarningArrows;
    f32 fShouldFlashSectionPass;
    f32 fShouldFlashSectionFail;
    bool bContinueScheduled;
    u32 iContinueMusicPos;
    f32 fWaitTime = 0.f;

    // sound
    f32 fMusicFrequencyBackup;
    i32 iCurMusicPos;
    i32 iCurMusicPosWithOffsets;
    McOsuInterpolator musicInterp;
    f32 fAfterMusicIsFinishedVirtualAudioTimeStart;
    bool bIsFirstMissSound;
    DatabaseBeatmap::TIMING_INFO current_timing_point{};
    i32 default_sample_set{1};

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
    i32 iNextHitObjectTime;
    i32 iPreviousHitObjectTime;
    i32 iPreviousSectionPassFailTime;

    // player input
    bool bClickedContinue;
    int iAllowAnyNextKeyUntilHitObjectIndex;
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
    DatabaseBeatmap *beatmap{nullptr};
    Sound *music;

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

    // live and precomputed pp/stars
    void resetLiveStarsTasks();
    void invalidateWholeMapPPInfo();

    pp_info full_ppinfo;
    pp_calc_request full_calc_req_params;

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
