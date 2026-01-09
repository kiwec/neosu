#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "UIOverlay.h"
#include "MD5Hash.h"
#include "types.h"
#include "UString.h"

#include <memory>

class UIAvatar;
class ScoreboardSlot;
class McFont;
class ConVar;
class Image;
class BeatmapInterface;
class Shader;
class VertexArrayObject;

class CBaseUIContainer;
namespace LegacyReplay {
enum KeyFlags : uint8_t;
}
using GameplayKeys = LegacyReplay::KeyFlags;

enum class WinCondition : uint8_t;

struct SCORE_ENTRY {
    UString name;
    i32 entry_id = 0;
    i32 player_id = 0;

    f32 accuracy;
    f64 pp{0.f};
    u64 score;
    int currentCombo;
    int maxCombo;
    int misses;
    bool dead;
    bool highlight;
};

class HUD final : public UIOverlay {
    NOCOPY_NOMOVE(HUD)
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
    inline void drawScoreNumber(u64 number, float scale = 1.0f, bool drawLeadingZeroes = false) {
        return this->drawComboOrScoreDigits(number, scale, drawLeadingZeroes, false);
    }
    inline void drawComboNumber(u64 number, float scale = 1.0f, bool drawLeadingZeroes = false) {
        return this->drawComboOrScoreDigits(number, scale, drawLeadingZeroes, true);
    }
    void drawComboSimple(int combo, float scale = 1.0f);          // used by RankingScreen
    void drawAccuracySimple(float accuracy, float scale = 1.0f);  // used by RankingScreen
    void drawWarningArrow(vec2 pos, bool flipVertically, bool originLeft = true);

    [[nodiscard]] bool shouldDrawScoreboard() const;

    [[nodiscard]] inline WinCondition getScoringMetric() const { return this->scoring_metric; }
    void updateScoringMetric();

    void resetScoreboard();
    void updateScoreboard(bool animate);
    void drawFancyScoreboard();

    void drawScorebarBg(float alpha, float breakAnim);
    void drawSectionPass(float alpha);
    void drawSectionFail(float alpha);

    void animateCombo();
    void addHitError(i32 delta, bool miss = false, bool misaim = false);
    void addTarget(float delta, float angle);
    void animateInputOverlay(GameplayKeys key_flag, bool down);

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

    std::vector<std::unique_ptr<ScoreboardSlot>> slots;
    ScoreboardSlot *player_slot{nullptr};  // pointer to an entry inside "slots"

    MD5Hash beatmap_md5;

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
    std::vector<SCORE_ENTRY> getCurrentScores();
    WinCondition scoring_metric{};

    struct HITERROR {
        float time;
        i32 delta;
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
    void drawAccuracy(float accuracy);
    void drawCombo(int combo);
    void drawScore(u64 score);
    void drawHPBar(double health, float alpha, float breakAnim);

    void drawComboOrScoreDigits(u64 number, float scale, bool drawLeadingZeroes, bool combo /* false for score */);
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
    const f64 fScoreboardCacheRefreshTime{0.250f};  // only update every 250ms instead of every frame
    f64 fScoreboardLastUpdateTime{0.f};

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
    std::unique_ptr<VertexArrayObject> cursorTrailVAO;
    std::vector<CURSORRIPPLE> cursorRipples;

    // target heatmap
    std::vector<TARGET> targets;

    std::vector<UIAvatar *> avatars;

    // health
    double fHealth;
    float fScoreBarBreakAnim;
    float fKiScaleAnim;
};
