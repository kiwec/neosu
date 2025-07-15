#pragma once

#include "cbase.h"

//**********************//
//	 Curve Base Class	//
//**********************//

class SliderCurve {
   public:
    enum OSUSLIDERCURVETYPE : uint8_t {
        OSUSLIDERCURVETYPE_CATMULL = 'C',
        OSUSLIDERCURVETYPE_BEZIER = 'B',
        OSUSLIDERCURVETYPE_LINEAR = 'L',
        OSUSLIDERCURVETYPE_PASSTHROUGH = 'P'
    };

    static SliderCurve *createCurve(char osuSliderCurveType, std::vector<Vector2> controlPoints, float pixelLength);
    static SliderCurve *createCurve(char osuSliderCurveType, std::vector<Vector2> controlPoints, float pixelLength,
                                    float curvePointsSeparation);

   public:
    SliderCurve(std::vector<Vector2> controlPoints, float pixelLength);
    virtual ~SliderCurve() { ; }

    virtual void updateStackPosition(float stackMulStackOffset, bool HR);

    virtual Vector2 pointAt(float t) = 0;          // with stacking
    virtual Vector2 originalPointAt(float t) = 0;  // without stacking

    [[nodiscard]] inline float getStartAngle() const { return this->fStartAngle; }
    [[nodiscard]] inline float getEndAngle() const { return this->fEndAngle; }

    [[nodiscard]] inline const std::vector<Vector2> &getPoints() const { return this->curvePoints; }
    [[nodiscard]] inline const std::vector<std::vector<Vector2>> &getPointSegments() const {
        return this->curvePointSegments;
    }

    [[nodiscard]] inline float getPixelLength() const { return this->fPixelLength; }

   protected:
    // original input values
    float fPixelLength;
    std::vector<Vector2> controlPoints;

    // these must be explicitly calculated/set in one of the subclasses
    std::vector<std::vector<Vector2>> curvePointSegments;
    std::vector<std::vector<Vector2>> originalCurvePointSegments;
    std::vector<Vector2> curvePoints;
    std::vector<Vector2> originalCurvePoints;
    float fStartAngle;
    float fEndAngle;
};

//******************************************//
//	 Curve Type Base Class & Curve Types	//
//******************************************//

class SliderCurveType {
   public:
    SliderCurveType();
    virtual ~SliderCurveType() { ; }

    virtual Vector2 pointAt(float t) = 0;

    [[nodiscard]] inline const int getNumPoints() const { return this->points.size(); }

    [[nodiscard]] inline const std::vector<Vector2> &getCurvePoints() const { return this->points; }
    [[nodiscard]] inline const std::vector<float> &getCurveDistances() const { return this->curveDistances; }

   protected:
    // either one must be called from one of the subclasses
    void init(
        float approxLength);  // subdivide the curve by calling virtual pointAt() to create all intermediary points
    void initCustom(std::vector<Vector2> points);  // assume that the input vector = all intermediary points (already
                                                   // precalculated somewhere else)

   private:
    void calculateIntermediaryPoints(float approxLength);
    void calculateCurveDistances();

    std::vector<Vector2> points;

    float fTotalDistance;
    std::vector<float> curveDistances;
};

class SliderCurveTypeBezier2 final : public SliderCurveType {
   public:
    SliderCurveTypeBezier2(const std::vector<Vector2> &points);
    ~SliderCurveTypeBezier2() override { ; }

    Vector2 pointAt(float  /*t*/) override { return Vector2(); }  // unused
};

class SliderCurveTypeCentripetalCatmullRom final : public SliderCurveType {
   public:
    SliderCurveTypeCentripetalCatmullRom(const std::vector<Vector2> &points);
    ~SliderCurveTypeCentripetalCatmullRom() override { ; }

    Vector2 pointAt(float t) override;

   private:
    float time[4];
    std::vector<Vector2> points;
};

//*******************//
//	 Curve Classes	 //
//*******************//

class SliderCurveEqualDistanceMulti : public SliderCurve {
   public:
    SliderCurveEqualDistanceMulti(std::vector<Vector2> controlPoints, float pixelLength, float curvePointsSeparation);
    ~SliderCurveEqualDistanceMulti() override { ; }

    // must be called from one of the subclasses
    void init(const std::vector<SliderCurveType *> &curvesList);

    Vector2 pointAt(float t) override;
    Vector2 originalPointAt(float t) override;

   private:
    int iNCurve;
};

class SliderCurveLinearBezier final : public SliderCurveEqualDistanceMulti {
   public:
    SliderCurveLinearBezier(std::vector<Vector2> controlPoints, float pixelLength, bool line,
                            float curvePointsSeparation);
    ~SliderCurveLinearBezier() override { ; }
};

class SliderCurveCatmull final : public SliderCurveEqualDistanceMulti {
   public:
    SliderCurveCatmull(std::vector<Vector2> controlPoints, float pixelLength, float curvePointsSeparation);
    ~SliderCurveCatmull() override { ; }
};

class SliderCurveCircumscribedCircle final : public SliderCurve {
   public:
    SliderCurveCircumscribedCircle(std::vector<Vector2> controlPoints, float pixelLength, float curvePointsSeparation);
    ~SliderCurveCircumscribedCircle() override { ; }

    Vector2 pointAt(float t) override;
    Vector2 originalPointAt(float t) override;

    void updateStackPosition(float stackMulStackOffset,
                             bool HR) override;  // must also override this, due to the custom pointAt() function!

   private:
    Vector2 intersect(Vector2 a, Vector2 ta, Vector2 b, Vector2 tb);

    inline bool const isIn(float a, float b, float c) { return ((b > a && b < c) || (b < a && b > c)); }

    Vector2 vCircleCenter;
    Vector2 vOriginalCircleCenter;
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
    SliderBezierApproximator();

    std::vector<Vector2> createBezier(const std::vector<Vector2> &controlPoints);

   private:
    static double TOLERANCE_SQ;

    bool isFlatEnough(const std::vector<Vector2> &controlPoints);
    void subdivide(std::vector<Vector2> &controlPoints, std::vector<Vector2> &l, std::vector<Vector2> &r);
    void approximate(std::vector<Vector2> &controlPoints, std::vector<Vector2> &output);

    int iCount;
    std::vector<Vector2> subdivisionBuffer1;
    std::vector<Vector2> subdivisionBuffer2;
};
