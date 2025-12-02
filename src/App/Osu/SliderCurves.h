#pragma once
// Copyright (c) 2015, PG & Jeffrey Han (opsu!), All rights reserved.

#include "config.h"
#include "noinclude.h"
#include "Vectors.h"

#include <vector>
#include <memory>

using SLIDERCURVETYPE = char;

//**********************//
//	 Curve Base Class	//
//**********************//

class SliderCurve {
   public:
    static std::unique_ptr<SliderCurve> createCurve(SLIDERCURVETYPE type, std::vector<vec2> controlPoints,
                                                    float pixelLength);
    static std::unique_ptr<SliderCurve> createCurve(SLIDERCURVETYPE type, std::vector<vec2> controlPoints,
                                                    float pixelLength, float curvePointsSeparation);

   public:
    SliderCurve(std::vector<vec2> controlPoints, float pixelLength);

    SliderCurve(const SliderCurve &) = default;
    SliderCurve &operator=(const SliderCurve &) = default;
    SliderCurve(SliderCurve &&) = default;
    SliderCurve &operator=(SliderCurve &&) = default;
    virtual ~SliderCurve() = default;

    virtual void updateStackPosition(float stackMulStackOffset, bool HR);

    virtual vec2 pointAt(float t) = 0;          // with stacking
    virtual vec2 originalPointAt(float t) = 0;  // without stacking

    [[nodiscard]] inline float getStartAngle() const { return this->fStartAngle; }
    [[nodiscard]] inline float getEndAngle() const { return this->fEndAngle; }

    [[nodiscard]] inline const std::vector<vec2> &getPoints() const { return this->curvePoints; }
    [[nodiscard]] inline const std::vector<std::vector<vec2>> &getPointSegments() const {
        return this->curvePointSegments;
    }

    [[nodiscard]] inline float getPixelLength() const { return this->fPixelLength; }

   protected:
    // original input values
    std::vector<vec2> controlPoints;

    // these must be explicitly calculated/set in one of the subclasses
    std::vector<std::vector<vec2>> curvePointSegments;
    std::vector<std::vector<vec2>> originalCurvePointSegments;
    std::vector<vec2> curvePoints;
    std::vector<vec2> originalCurvePoints;
    float fStartAngle;
    float fEndAngle;
    float fPixelLength;
};

//******************************************//
//	 Curve Type Base Class & Curve Types	//
//******************************************//

class SliderCurveType {
   public:
    SliderCurveType();

    SliderCurveType(const SliderCurveType &) = default;
    SliderCurveType &operator=(const SliderCurveType &) = default;
    SliderCurveType(SliderCurveType &&) = default;
    SliderCurveType &operator=(SliderCurveType &&) = default;
    virtual ~SliderCurveType() = default;

    virtual vec2 pointAt(float t) = 0;

    [[nodiscard]] inline int getNumPoints() const { return this->points.size(); }

    [[nodiscard]] inline const std::vector<vec2> &getCurvePoints() const { return this->points; }
    [[nodiscard]] inline const std::vector<float> &getCurveDistances() const { return this->curveDistances; }

   protected:
    // either one must be called from one of the subclasses
    void init(
        float approxLength);  // subdivide the curve by calling virtual pointAt() to create all intermediary points
    void initCustom(std::vector<vec2> points);  // assume that the input vector = all intermediary points (already
                                                // precalculated somewhere else)

   private:
    void calculateIntermediaryPoints(float approxLength);
    void calculateCurveDistances();

    std::vector<vec2> points;
    std::vector<float> curveDistances;
    float fTotalDistance;
};

class SliderCurveTypeBezier2 final : public SliderCurveType {
   public:
    SliderCurveTypeBezier2(const std::vector<vec2> &points);

    SliderCurveTypeBezier2(const SliderCurveTypeBezier2 &) = default;
    SliderCurveTypeBezier2 &operator=(const SliderCurveTypeBezier2 &) = default;
    SliderCurveTypeBezier2(SliderCurveTypeBezier2 &&) = default;
    SliderCurveTypeBezier2 &operator=(SliderCurveTypeBezier2 &&) = default;
    ~SliderCurveTypeBezier2() override = default;

    vec2 pointAt(float /*t*/) override { return {0.f, 0.f}; }  // unused
};

class SliderCurveTypeCentripetalCatmullRom final : public SliderCurveType {
   public:
    SliderCurveTypeCentripetalCatmullRom(const std::vector<vec2> &points);

    SliderCurveTypeCentripetalCatmullRom(const SliderCurveTypeCentripetalCatmullRom &) = default;
    SliderCurveTypeCentripetalCatmullRom &operator=(const SliderCurveTypeCentripetalCatmullRom &) = default;
    SliderCurveTypeCentripetalCatmullRom(SliderCurveTypeCentripetalCatmullRom &&) = default;
    SliderCurveTypeCentripetalCatmullRom &operator=(SliderCurveTypeCentripetalCatmullRom &&) = default;
    ~SliderCurveTypeCentripetalCatmullRom() override = default;

    vec2 pointAt(float t) override;

   private:
    std::vector<vec2> points;
    float time[4];
};

//*******************//
//	 Curve Classes	 //
//*******************//

class SliderCurveEqualDistanceMulti : public SliderCurve {
   public:
    SliderCurveEqualDistanceMulti(std::vector<vec2> controlPoints, float pixelLength, float curvePointsSeparation);

    SliderCurveEqualDistanceMulti(const SliderCurveEqualDistanceMulti &) = default;
    SliderCurveEqualDistanceMulti &operator=(const SliderCurveEqualDistanceMulti &) = default;
    SliderCurveEqualDistanceMulti(SliderCurveEqualDistanceMulti &&) = default;
    SliderCurveEqualDistanceMulti &operator=(SliderCurveEqualDistanceMulti &&) = default;
    ~SliderCurveEqualDistanceMulti() override = default;

    vec2 pointAt(float t) override;
    vec2 originalPointAt(float t) override;

   protected:
    void init(std::vector<std::unique_ptr<SliderCurveType>> curvesList);

   private:
    int iNCurve;
};

class SliderCurveLinearBezier final : public SliderCurveEqualDistanceMulti {
   public:
    SliderCurveLinearBezier(std::vector<vec2> controlPoints, float pixelLength, bool line, float curvePointsSeparation);

    SliderCurveLinearBezier(const SliderCurveLinearBezier &) = default;
    SliderCurveLinearBezier &operator=(const SliderCurveLinearBezier &) = default;
    SliderCurveLinearBezier(SliderCurveLinearBezier &&) = default;
    SliderCurveLinearBezier &operator=(SliderCurveLinearBezier &&) = default;
    ~SliderCurveLinearBezier() override = default;
};

class SliderCurveCatmull final : public SliderCurveEqualDistanceMulti {
   public:
    SliderCurveCatmull(std::vector<vec2> controlPoints, float pixelLength, float curvePointsSeparation);

    SliderCurveCatmull(const SliderCurveCatmull &) = default;
    SliderCurveCatmull &operator=(const SliderCurveCatmull &) = default;
    SliderCurveCatmull(SliderCurveCatmull &&) = default;
    SliderCurveCatmull &operator=(SliderCurveCatmull &&) = default;
    ~SliderCurveCatmull() override = default;
};

class SliderCurveCircumscribedCircle final : public SliderCurve {
   public:
    SliderCurveCircumscribedCircle(std::vector<vec2> controlPoints, float pixelLength, float curvePointsSeparation);

    SliderCurveCircumscribedCircle(const SliderCurveCircumscribedCircle &) = default;
    SliderCurveCircumscribedCircle &operator=(const SliderCurveCircumscribedCircle &) = default;
    SliderCurveCircumscribedCircle(SliderCurveCircumscribedCircle &&) = default;
    SliderCurveCircumscribedCircle &operator=(SliderCurveCircumscribedCircle &&) = default;
    ~SliderCurveCircumscribedCircle() override = default;

    vec2 pointAt(float t) override;
    vec2 originalPointAt(float t) override;

    void updateStackPosition(float stackMulStackOffset,
                             bool HR) override;  // must also override this, due to the custom pointAt() function!

   private:
    static vec2 intersect(vec2 a, vec2 ta, vec2 b, vec2 tb);

    static forceinline bool isIn(float a, float b, float c) { return ((b > a && b < c) || (b < a && b > c)); }

    vec2 vCircleCenter{0.f};
    vec2 vOriginalCircleCenter{0.f};
    float fRadius;
    float fCalculationStartAngle;
    float fCalculationEndAngle;
};

//********************//
//	 Helper Classes	  //
//********************//

// https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Objects/BezierApproximator.cs
// https://github.com/ppy/osu-framework/blob/master/osu.Framework/MathUtils/PathApproximator.cs
// Copyright (c) ppy Pty Ltd <contact@ppy.sh>. Licensed under the MIT Licence.

class SliderBezierApproximator {
   public:
    std::vector<vec2> createBezier(const std::vector<vec2> &controlPoints);

   private:
    static constexpr const double TOLERANCE_SQ{0.25 * 0.25};

    bool isFlatEnough(const std::vector<vec2> &controlPoints);
    void subdivide(std::vector<vec2> &controlPoints, std::vector<vec2> &l, std::vector<vec2> &r);
    void approximate(std::vector<vec2> &controlPoints, std::vector<vec2> &output);

    std::vector<vec2> subdivisionBuffer1;
    std::vector<vec2> subdivisionBuffer2;
    int iCount{0};
};
