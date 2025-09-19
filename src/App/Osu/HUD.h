#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "OsuScreen.h"
#include "MD5Hash.h"

class UIAvatar;
struct ScoreboardSlot;
class McFont;
class ConVar;
class Image;
class BeatmapInterface;
class Shader;
class VertexArrayObject;

class CBaseUIContainer;

struct SCORE_ENTRY {
    UString name;
    i32 entry_id = 0;
    i32 player_id = 0;

    int combo;
    unsigned long long score;
    float accuracy;
    bool dead;
    bool highlight;
};

class HUD : public OsuScreen {
   public:
    HUD();
    ~HUD() override;

    void draw() override;
    void drawDummy();

    void drawCursor(vec2 pos, float alphaMultiplier = 1.0f, bool secondTrail = false, bool updateAndDrawTrail = true);
    void drawCursorTrail(
        vec2 pos, float alphaMultiplier = 1.0f,
        bool secondTrail = false);  // NOTE: only use if drawCursor() with updateAndDrawTrail = false (FPoSu)
    void drawCursorRipples();
    void drawFps();
    void drawHitErrorBar(BeatmapInterface *pf);
    void drawPlayfieldBorder(vec2 playfieldCenter, vec2 playfieldSize, float hitcircleDiameter);
    void drawPlayfieldBorder(vec2 playfieldCenter, vec2 playfieldSize, float hitcircleDiameter, float borderSize);
    void drawLoadingSmall(const UString &text);
    void drawBeatmapImportSpinner();
    void drawScoreNumber(unsigned long long number, float scale = 1.0f, bool drawLeadingZeroes = false);
    void drawComboNumber(unsigned long long number, float scale = 1.0f, bool drawLeadingZeroes = false);
    void drawComboSimple(int combo, float scale = 1.0f);          // used by RankingScreen
    void drawAccuracySimple(float accuracy, float scale = 1.0f);  // used by RankingScreen
    void drawWarningArrow(vec2 pos, bool flipVertically, bool originLeft = true);

    std::vector<SCORE_ENTRY> getCurrentScores();
    void resetScoreboard();
    void updateScoreboard(bool animate);
    void drawFancyScoreboard();

    void drawScorebarBg(float alpha, float breakAnim);
    void drawSectionPass(float alpha);
    void drawSectionFail(float alpha);

    void animateCombo();
    void addHitError(long delta, bool miss = false, bool misaim = false);
    void addTarget(float delta, float angle);
    void animateInputoverlay(int key, bool down);

    void addCursorRipple(vec2 pos);
    void animateCursorExpand();
    void animateCursorShrink();
    void animateKiBulge();
    void animateKiExplode();

    void resetHitErrorBar();

    McRect getSkipClickRect();

    void drawSkip();

    // ILLEGAL:
    [[nodiscard]] inline float getScoreBarBreakAnim() const { return this->fScoreBarBreakAnim; }

    std::shared_ptr<ScoreboardSlot> player_slot{nullptr};
    std::vector<std::shared_ptr<ScoreboardSlot>> slots;
    MD5Hash beatmap_md5;

    f32 live_pp = 0.0;
    f32 live_stars = 0.0;

    struct CURSORTRAIL {
        vec2 pos{0.f};
        float time;
        float alpha;
        float scale;
    };

    struct CURSORRIPPLE {
        vec2 pos{0.f};
        float time;
    };

    float getCursorScaleFactor();
    void addCursorTrailPosition(std::vector<CURSORTRAIL> &trail, vec2 pos);

   private:
    struct HITERROR {
        float time;
        long delta;
        bool miss;
        bool misaim;
    };

    struct TARGET {
        float time;
        float delta;
        float angle;
    };

    struct BREAK {
        float startPercent;
        float endPercent;
    };

    void drawCursorTrailInt(Shader *trailShader, std::vector<CURSORTRAIL> &trail, vec2 pos,
                            float alphaMultiplier = 1.0f, bool emptyTrailFrame = false);
    void drawCursorTrailRaw(float alpha, vec2 pos);
    void drawFps(McFont *font, float fps);
    void drawAccuracy(float accuracy);
    void drawCombo(int combo);
    void drawScore(unsigned long long score);
    void drawHPBar(double health, float alpha, float breakAnim);

    void drawWarningArrows(float hitcircleDiameter = 0.0f);
    void drawContinue(vec2 cursor, float hitcircleDiameter = 0.0f);
    void drawHitErrorBar(float hitWindow300, float hitWindow100, float hitWindow50, float hitWindowMiss, int ur);
    void drawHitErrorBarInt(float hitWindow300, float hitWindow100, float hitWindow50, float hitWindowMiss);
    void drawHitErrorBarInt2(vec2 center, int ur);
    void drawProgressBar(float percent, bool waiting);
    void drawStatistics(int misses, int sliderbreaks, int maxPossibleCombo, float liveStars, float totalStars, int bpm,
                        float ar, float cs, float od, float hp, int nps, int nd, int ur, float pp, float ppfc,
                        float hitWindow300, int hitdeltaMin, int hitdeltaMax);
    void drawTargetHeatmap(float hitcircleDiameter);
    void drawScrubbingTimeline(u32 beatmapTime, u32 beatmapLengthPlayable, u32 beatmapStartTimePlayable,
                               f32 beatmapPercentFinishedPlayable, const std::vector<BREAK> &breaks);
    void drawInputOverlay(int numK1, int numK2, int numM1, int numM2);

    float getCursorTrailScaleFactor();

    float getScoreScale();

    McFont *tempFont;

    // shit code
    float fAccuracyXOffset;
    float fAccuracyYOffset;
    float fScoreHeight;

    float fComboAnim1;
    float fComboAnim2;

    // fps counter
    float fCurFps;
    float fCurFpsSmooth;
    float fFpsUpdate;

    // hit error bar
    std::vector<HITERROR> hiterrors;

    // inputoverlay / key overlay
    float fInputoverlayK1AnimScale;
    float fInputoverlayK2AnimScale;
    float fInputoverlayM1AnimScale;
    float fInputoverlayM2AnimScale;

    float fInputoverlayK1AnimColor;
    float fInputoverlayK2AnimColor;
    float fInputoverlayM1AnimColor;
    float fInputoverlayM2AnimColor;

    // cursor & trail & ripples
    float fCursorExpandAnim;
    std::vector<CURSORTRAIL> cursorTrail;
    std::vector<CURSORTRAIL> cursorTrail2;
    std::vector<CURSORTRAIL> cursorTrailSpectator1;
    std::vector<CURSORTRAIL> cursorTrailSpectator2;
    Shader *cursorTrailShader;
    VertexArrayObject *cursorTrailVAO;
    std::vector<CURSORRIPPLE> cursorRipples;

    // target heatmap
    std::vector<TARGET> targets;

    std::vector<UIAvatar *> avatars;

    // health
    double fHealth;
    float fScoreBarBreakAnim;
    float fKiScaleAnim;
};
