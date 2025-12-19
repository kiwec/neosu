#pragma once
#include "BeatmapInterface.h"

class ConVar;
class ModFPoSu;
class SkinImage;
class SliderCurve;
class VertexArrayObject;
class Image;

enum class HitObjectType : uint8_t {
    CIRCLE,
    SLIDER,
    SPINNER,
};

namespace PpyHitObjectType {
enum {
    CIRCLE = (1 << 0),
    SLIDER = (1 << 1),
    NEW_COMBO = (1 << 2),
    SPINNER = (1 << 3),
    // 4, 5, 6: 3-bit integer specifying how many combo colors to skip (if NEW_COMBO is set)
    MANIA_HOLD_NOTE = (1 << 7),
};
}

class HitObject {
   public:
    static void drawHitResult(BeatmapInterface *pf, vec2 rawPos, LiveScore::HIT result, float animPercentInv,
                              float hitDeltaRangePercent);
    static void drawHitResult(const Skin *skin, float hitcircleDiameter, float rawHitcircleDiameter, vec2 rawPos,
                              LiveScore::HIT result, float animPercentInv, float hitDeltaRangePercent);

   public:
    HitObject(i32 time, HitSamples samples, int comboNumber, bool isEndOfCombo, int colorCounter, int colorOffset,
              AbstractBeatmapInterface *beatmap);
    virtual ~HitObject() = default;

    virtual void draw() { ; }
    virtual void draw2();
    virtual void update(i32 curPos, f64 frame_time);

    virtual void updateStackPosition(float stackOffset) = 0;
    virtual void miss(i32 curPos) = 0;  // only used by notelock

    // [[nodiscard]] virtual constexpr forceinline int getCombo() const {
    //     return 1;
    // }  // how much combo this hitobject is "worth"

    // Gameplay logic
    HitObjectType type{HitObjectType::CIRCLE};
    i32 click_time;
    i32 duration;

    // Visual
    i32 combo_number;
    bool is_end_of_combo = false;

    void addHitResult(LiveScore::HIT result, i32 delta, bool isEndOfCombo, vec2 posRaw, float targetDelta = 0.0f,
                      float targetAngle = 0.0f, bool ignoreOnHitErrorBar = false, bool ignoreCombo = false,
                      bool ignoreHealth = false, bool addObjectDurationToSkinAnimationTimeStartOffset = true);
    void misAimed() { this->bMisAim = true; }

    void setStack(int stack) { this->iStack = stack; }
    void setForceDrawApproachCircle(bool firstNote) { this->bOverrideHDApproachCircle = firstNote; }
    void setAutopilotDelta(i32 delta) { this->iAutopilotDelta = delta; }
    void setBlocked(bool blocked) { this->bBlocked = blocked; }

    [[nodiscard]] virtual vec2 getRawPosAt(i32 pos) const = 0;          // with stack calculation modifications
    [[nodiscard]] virtual vec2 getOriginalRawPosAt(i32 pos) const = 0;  // without stack calculations
    [[nodiscard]] virtual vec2 getAutoCursorPos(i32 curPos) const = 0;

    [[nodiscard]] inline int getStack() const { return this->iStack; }
    [[nodiscard]] inline int getColorCounter() const { return this->iColorCounter; }
    [[nodiscard]] inline int getColorOffset() const { return this->iColorOffset; }
    [[nodiscard]] inline float getApproachScale() const { return this->fApproachScale; }
    [[nodiscard]] inline i32 getDelta() const { return this->iDelta; }
    [[nodiscard]] inline i32 getApproachTime() const { return this->iApproachTime; }
    [[nodiscard]] inline i32 getAutopilotDelta() const { return this->iAutopilotDelta; }

    [[nodiscard]] inline bool isVisible() const { return this->bVisible; }
    [[nodiscard]] inline bool isFinished() const { return this->bFinished; }
    [[nodiscard]] inline bool isBlocked() const { return this->bBlocked; }
    [[nodiscard]] inline bool hasMisAimed() const { return this->bMisAim; }

    virtual void onClickEvent(std::vector<Click> & /*clicks*/) { ; }
    virtual void onReset(i32 curPos);

   private:
   private:
    static float lerp3f(float a, float b, float c, float percent);

    struct HITRESULTANIM {
        vec2 rawPos{0.f};
        i32 delta;
        float time;
        LiveScore::HIT result;
        bool addObjectDurationToSkinAnimationTimeStartOffset;
    };

    void drawHitResultAnim(const HITRESULTANIM &hitresultanim);

    HITRESULTANIM hitresultanim1;
    HITRESULTANIM hitresultanim2;

   protected:
    AbstractBeatmapInterface *pi = nullptr;
    BeatmapInterface *pf = nullptr;  // NULL when simulating

    i32 iDelta;  // this must be signed
    i32 iApproachTime;
    i32 iFadeInTime;  // extra time added before the approachTime to let the object smoothly become visible
    i32 iAutopilotDelta;

    HitSamples samples;
    int iColorCounter;
    int iColorOffset;

    int iStack;

    float fAlpha;
    float fAlphaWithoutHidden;
    float fAlphaForApproachCircle;
    float fApproachScale;
    float fHittableDimRGBColorMultiplierPercent;

    unsigned bBlocked : 1;
    unsigned bOverrideHDApproachCircle : 1;
    unsigned bMisAim : 1;
    unsigned bUseFadeInTimeAsApproachTime : 1;
    unsigned bVisible : 1;
    unsigned bFinished : 1;
};

class Circle final : public HitObject {
   public:
    // main
    static void drawApproachCircle(BeatmapInterface *pf, vec2 rawPos, int number, int colorCounter, int colorOffset,
                                   float colorRGBMultiplier, float approachScale, float alpha,
                                   bool overrideHDApproachCircle = false);
    static void drawCircle(BeatmapInterface *pf, vec2 rawPos, int number, int colorCounter, int colorOffset,
                           float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha,
                           bool drawNumber = true, bool overrideHDApproachCircle = false);
    static void drawCircle(const Skin *skin, vec2 pos, float hitcircleDiameter, float numberScale, float overlapScale,
                           int number, int colorCounter, int colorOffset, float colorRGBMultiplier, float approachScale,
                           float alpha, float numberAlpha, bool drawNumber = true,
                           bool overrideHDApproachCircle = false);
    static void drawCircle(const Skin *skin, vec2 pos, float hitcircleDiameter, Color color, float alpha = 1.0f);
    static void drawSliderStartCircle(BeatmapInterface *pf, vec2 rawPos, int number, int colorCounter, int colorOffset,
                                      float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha,
                                      bool drawNumber = true, bool overrideHDApproachCircle = false);
    static void drawSliderStartCircle(const Skin *skin, vec2 pos, float hitcircleDiameter, float numberScale,
                                      float hitcircleOverlapScale, int number, int colorCounter = 0,
                                      int colorOffset = 0, float colorRGBMultiplier = 1.0f, float approachScale = 1.0f,
                                      float alpha = 1.0f, float numberAlpha = 1.0f, bool drawNumber = true,
                                      bool overrideHDApproachCircle = false);
    static void drawSliderEndCircle(BeatmapInterface *pf, vec2 rawPos, int number, int colorCounter, int colorOffset,
                                    float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha,
                                    bool drawNumber = true, bool overrideHDApproachCircle = false);
    static void drawSliderEndCircle(const Skin *skin, vec2 pos, float hitcircleDiameter, float numberScale,
                                    float overlapScale, int number = 0, int colorCounter = 0, int colorOffset = 0,
                                    float colorRGBMultiplier = 1.0f, float approachScale = 1.0f, float alpha = 1.0f,
                                    float numberAlpha = 1.0f, bool drawNumber = true,
                                    bool overrideHDApproachCircle = false);

    // split helper functions
    static void drawApproachCircle(const Skin *skin, vec2 pos, Color comboColor, float hitcircleDiameter,
                                   float approachScale, float alpha, bool modHD, bool overrideHDApproachCircle);
    static void drawHitCircleOverlay(SkinImage *hitCircleOverlayImage, vec2 pos, float circleOverlayImageScale,
                                     float alpha, float colorRGBMultiplier);
    static void drawHitCircle(Image *hitCircleImage, vec2 pos, Color comboColor, float circleImageScale, float alpha);
    static void drawHitCircleNumber(const Skin *skin, float numberScale, float overlapScale, vec2 pos, int number,
                                    float numberAlpha, float colorRGBMultiplier);

   public:
    Circle(int x, int y, i32 time, HitSamples samples, int comboNumber, bool isEndOfCombo, int colorCounter,
           int colorOffset, AbstractBeatmapInterface *beatmap);
    ~Circle() override;

    void draw() override;
    void draw2() override;
    void update(i32 curPos, f64 frame_time) override;

    void updateStackPosition(float stackOffset) override;
    void miss(i32 curPos) override;

    [[nodiscard]] vec2 getRawPosAt(i32 /*pos*/) const override { return this->vRawPos; }
    [[nodiscard]] vec2 getOriginalRawPosAt(i32 /*pos*/) const override { return this->vOriginalRawPos; }
    [[nodiscard]] vec2 getAutoCursorPos(i32 curPos) const override;

    void onClickEvent(std::vector<Click> &clicks) override;
    void onReset(i32 curPos) override;

   private:
    // necessary due to the static draw functions
    static int rainbowNumber;
    static int rainbowColorCounter;

    void onHit(LiveScore::HIT result, i32 delta, float targetDelta = 0.0f, float targetAngle = 0.0f);

    bool bWaiting;

    vec2 vRawPos{0.f};
    vec2 vOriginalRawPos{0.f};  // for live mod changing

    float fHitAnimation;
    float fShakeAnimation;
};

class Slider final : public HitObject {
   public:
    struct SLIDERCLICK {
        i32 time;
        int type;
        int tickIndex;
        bool finished;
        bool successful;
        bool sliderend;
    };

   public:
    using SLIDERCURVETYPE = char;
    Slider(SLIDERCURVETYPE stype, int repeat, float pixelLength, std::vector<vec2> points,
           const std::vector<float> &ticks, float sliderTime, float sliderTimeWithoutRepeats, i32 time,
           HitSamples hoverSamples, std::vector<HitSamples> edgeSamples, int comboNumber, bool isEndOfCombo,
           int colorCounter, int colorOffset, AbstractBeatmapInterface *beatmap);
    ~Slider() override;

    void draw() override;
    inline void draw2() override { this->draw2(true, false); }
    void draw2(bool drawApproachCircle, bool drawOnlyApproachCircle);
    void update(i32 curPos, f64 frame_time) override;

    void updateStackPosition(float stackOffset) override;
    void miss(i32 curPos) override;
    // [[nodiscard]] constexpr forceinline int getCombo() const override {
    //     return 2 + std::max((this->iRepeat - 1), 0) + (std::max((this->iRepeat - 1), 0) + 1) * this->ticks.size();
    // }

    [[nodiscard]] vec2 getRawPosAt(i32 pos) const override;
    [[nodiscard]] vec2 getOriginalRawPosAt(i32 pos) const override;
    [[nodiscard]] inline vec2 getAutoCursorPos(i32 /*curPos*/) const override { return this->vCurPoint; }

    void onClickEvent(std::vector<Click> &clicks) override;
    void onReset(i32 curPos) override;

    void rebuildVertexBuffer(bool useRawCoords = false);

    [[nodiscard]] inline bool isStartCircleFinished() const { return this->bStartFinished; }
    [[nodiscard]] inline int getRepeat() const { return this->iRepeat; }
    [[nodiscard]] inline std::vector<vec2> getRawPoints() const { return this->points; }
    [[nodiscard]] inline float getPixelLength() const { return this->fPixelLength; }
    [[nodiscard]] inline const std::vector<SLIDERCLICK> &getClicks() const { return this->clicks; }

   private:
    void drawStartCircle(float alpha);
    void drawEndCircle(float alpha, float sliderSnake);
    void drawBody(float alpha, float from, float to);

    void updateAnimations(i32 curPos);

    void onHit(LiveScore::HIT result, i32 delta, bool startOrEnd, float targetDelta = 0.0f, float targetAngle = 0.0f,
               bool isEndResultFromStrictTrackingMod = false);
    void onRepeatHit(const SLIDERCLICK &click);
    void onTickHit(const SLIDERCLICK &click);
    void onSliderBreak();

    [[nodiscard]] float getT(i32 pos, bool raw) const;

    bool isClickHeldSlider();  // special logic to disallow hold tapping

    struct SLIDERTICK {
        float percent;
        bool finished;
    };

    std::vector<vec2> points;
    std::vector<HitSamples> edgeSamples;
    std::vector<HitSamples::Set_Slider_Hit> lastSliderSampleSets{};

    std::vector<SLIDERTICK> ticks;  // ticks (drawing)

    // TEMP: auto cursordance
    std::vector<SLIDERCLICK> clicks;  // repeats (type 0) + ticks (type 1)

    std::unique_ptr<SliderCurve> curve;
    std::unique_ptr<VertexArrayObject> vao{nullptr};

    vec2 vCurPoint{0.f};
    vec2 vCurPointRaw{0.f};

    i32 iStrictTrackingModLastClickHeldTime;

    float fPixelLength;

    float fSliderTime;
    float fSliderTimeWithoutRepeats;

    float fSlidePercent;        // 0.0f - 1.0f - 0.0f - 1.0f - etc.
    float fActualSlidePercent;  // 0.0f - 1.0f
    float fSliderSnakePercent;
    float fReverseArrowAlpha;
    float fBodyAlpha;

    float fStartHitAnimation;
    float fEndHitAnimation;
    float fEndSliderBodyFadeAnimation;

    float fFollowCircleTickAnimationScale;
    float fFollowCircleAnimationScale;
    float fFollowCircleAnimationAlpha;

    int iRepeat;
    int iKeyFlags;
    int iReverseArrowPos;
    int iCurRepeat;
    int iCurRepeatCounterForHitSounds;

    SLIDERCURVETYPE cType;

    LiveScore::HIT startResult : 4;
    LiveScore::HIT endResult : 4;

    unsigned bStartFinished : 1;
    unsigned bEndFinished : 1;
    unsigned bCursorLeft : 1;
    unsigned bCursorInside : 1;
    unsigned bHeldTillEnd : 1;
    unsigned bHeldTillEndForLenienceHack : 1;
    unsigned bHeldTillEndForLenienceHackCheck : 1;
    unsigned bInReverse : 1;
    unsigned bHideNumberAfterFirstRepeatHit : 1;
};

class Spinner final : public HitObject {
   public:
    Spinner(int x, int y, i32 time, HitSamples samples, bool isEndOfCombo, i32 endTime,
            AbstractBeatmapInterface *beatmap);
    ~Spinner() override;

    void draw() override;
    void update(i32 curPos, f64 frame_time) override;

    void updateStackPosition(float /*stackOffset*/) override { ; }
    void miss(i32 /*curPos*/) override { ; }

    [[nodiscard]] vec2 getRawPosAt(i32 /*pos*/) const override { return this->vRawPos; }
    [[nodiscard]] vec2 getOriginalRawPosAt(i32 /*pos*/) const override { return this->vOriginalRawPos; }
    [[nodiscard]] vec2 getAutoCursorPos(i32 curPos) const override;

    void onReset(i32 curPos) override;

   private:
    void onHit();
    void rotate(float rad);

    vec2 vRawPos{0.f};
    vec2 vOriginalRawPos{0.f};

    std::unique_ptr<float[]> storedDeltaAngles{nullptr};

    // bool bClickedOnce;
    float fPercent;

    float fDrawRot;
    float fRotations;
    float fRotationsNeeded;
    float fDeltaOverflow;
    float fSumDeltaAngle;

    int iMaxStoredDeltaAngles;
    int iDeltaAngleIndex;
    float fDeltaAngleOverflow;

    float fRPM;

    float fLastMouseAngle;
    float fRatio;
};
