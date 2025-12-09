// Copyright (c) 2015, PG & Jeffrey Han (opsu!), All rights reserved.
#include "SliderCurves.h"
#include "noinclude.h"
#include "types.h"

#ifndef BUILD_TOOLS_ONLY
#include "Engine.h"
#include "Logging.h"
#include "OsuConVars.h"
#define SLIDER_CURVE_POINTS_SEPARATION cv::slider_curve_points_separation.getFloat()
#define SLIDER_CURVE_MAX_LENGTH cv::slider_curve_max_length.getFloat()
#define SLIDER_CURVE_MAX_POINTS cv::slider_curve_max_points.getVal<u32>()
#else
#include <print>
#include <algorithm>
#define SLIDER_CURVE_POINTS_SEPARATION 2.5f
#define SLIDER_CURVE_MAX_LENGTH 32768.f
#define SLIDER_CURVE_MAX_POINTS 9999U
#define debugLog(...) std::println(__VA_ARGS__)
#endif

#include <stack>

namespace {

constexpr const SLIDERCURVETYPE CATMULL = 'C';
constexpr const SLIDERCURVETYPE BEZIER = 'B';
constexpr const SLIDERCURVETYPE LINEAR = 'L';
constexpr const SLIDERCURVETYPE PASSTHROUGH = 'P';

//***********************//
//	 Curve Subclasses	 //
//***********************//

class SliderCurveEqualDistanceMulti : public SliderCurve {
   public:
    static inline SliderCurveEqualDistanceMulti createLinearBezier(std::vector<vec2> controlPoints, f32 pixelLength,
                                                                   f32 curvePointsSeparation, bool line) {
        return {std::move(controlPoints), pixelLength, curvePointsSeparation, line};
    }
    static inline SliderCurveEqualDistanceMulti createCatmull(std::vector<vec2> controlPoints, f32 pixelLength,
                                                              f32 curvePointsSeparation) {
        return {std::move(controlPoints), pixelLength, curvePointsSeparation};
    }

    SliderCurveEqualDistanceMulti(const SliderCurveEqualDistanceMulti &) = default;
    SliderCurveEqualDistanceMulti &operator=(const SliderCurveEqualDistanceMulti &) = default;
    SliderCurveEqualDistanceMulti(SliderCurveEqualDistanceMulti &&) = default;
    SliderCurveEqualDistanceMulti &operator=(SliderCurveEqualDistanceMulti &&) = default;
    ~SliderCurveEqualDistanceMulti() override = default;

    [[nodiscard]] vec2 pointAt(f32 t) const override;
    [[nodiscard]] vec2 originalPointAt(f32 t) const override;

   private:
    // construct with createLinearBezier/createCatmull
    SliderCurveEqualDistanceMulti(std::vector<vec2> controlPoints, f32 pixelLength, f32 curvePointsSeparation,
                                  bool line);  // beziers
    SliderCurveEqualDistanceMulti(std::vector<vec2> controlPoints, f32 pixelLength,
                                  f32 curvePointsSeparation);  // catmulls
    struct SliderCurveDetails {
        // ctor helpers
       private:
        [[nodiscard]] static std::vector<vec2> initBezier(const std::vector<vec2> &initPoints);
        [[nodiscard]] static std::vector<vec2> initCatmull(const std::vector<vec2> &initPoints);

       public:
        SliderCurveDetails() = delete;

        // BEZIER or CATMULL initialization
        SliderCurveDetails(SLIDERCURVETYPE type, const std::vector<vec2> &initPoints)
            : points(type == BEZIER ? initBezier(initPoints) : initCatmull(initPoints)),
              curveDistances(std::make_unique_for_overwrite<f32[]>(points.size())) {
            assert(type == BEZIER || type == CATMULL);

            // calculate curve distances
            // find the distance of each point from the previous point (needed for some curve types)
            for(u64 i = 0; i < this->numCurveDistances(); i++) {
                const f32 curDist = (i == 0) ? 0 : vec::length(this->points[i] - this->points[i - 1]);

                this->curveDistances[i] = curDist;
            }
        }

        [[nodiscard]] forceinline u64 numPoints() const { return this->points.size(); }
        [[nodiscard]] forceinline u64 numCurveDistances() const { return this->points.size(); }  // must be the same

        std::vector<vec2> points;
        std::unique_ptr<f32[]> curveDistances;
    };

    void init(const std::vector<SliderCurveDetails> &curvesList);

    u32 iNCurve;
};

void SliderCurveEqualDistanceMulti::init(const std::vector<SliderCurveDetails> &curvesList) {
    i64 curCurveIndex = 0;
    i64 curPoint = 0;

    f32 distanceAt = 0.0f;
    f32 lastDistanceAt = 0.0f;

    const SliderCurveDetails *curCurve = &curvesList[curCurveIndex];
    if(curCurve->numPoints() < 1) {
        debugLog("SliderCurveEqualDistanceMulti::init: Error: curCurve->getNumPoints() == 0!!!");

        return;
    }

    vec2 lastCurve = curCurve->points[curPoint];

    // length of the curve should be equal to the pixel length
    // for each distance, try to get in between the two points that are between it
    vec2 lastCurvePointForNextSegmentStart{0.f};
    std::vector<vec2> curCurvePoints;
    for(i64 i = 0; i < (this->iNCurve + 1LL); i++) {
        const f32 temp_dist = (f32)((f32)i * this->fPixelLength) / (f32)this->iNCurve;
        const i32 prefDistance = (std::isfinite(temp_dist) && temp_dist >= (f32)(std::numeric_limits<i32>::min()) &&
                                  temp_dist <= (f32)(std::numeric_limits<i32>::max()))
                                     ? (i32)(temp_dist)
                                     : 0;

        while(distanceAt < prefDistance) {
            lastDistanceAt = distanceAt;
            if(curCurve->numPoints() > 0 && curPoint > -1 && curPoint < curCurve->numPoints())
                lastCurve = curCurve->points[curPoint];

            // jump to next point
            curPoint++;

            if(curPoint >= curCurve->numPoints()) {
                // jump to next curve
                curCurveIndex++;

                // when jumping to the next curve, add the current segment to the list of segments
                if(curCurvePoints.size() > 0) {
                    this->curvePointSegments.push_back(curCurvePoints);
                    curCurvePoints.clear();

                    // prepare the next segment by setting/adding the starting point to the exact end point of the
                    // previous segment this also enables an optimization, namely that startcaps only have to be drawn
                    // [for every segment] if the startpoint != endpoint in the loop
                    if(this->curvePoints.size() > 0) curCurvePoints.push_back(lastCurvePointForNextSegmentStart);
                }

                if(curCurveIndex < curvesList.size()) {
                    curCurve = &curvesList[curCurveIndex];
                    curPoint = 0;
                } else {
                    curPoint = curCurve->numPoints() - 1;
                    if(lastDistanceAt == distanceAt) {
                        // out of points even though the preferred distance hasn't been reached
                        break;
                    }
                }
            }

            if(curCurve->numCurveDistances() > 0 && curPoint > -1 && curPoint < curCurve->numCurveDistances())
                distanceAt += curCurve->curveDistances[curPoint];
        }

        const vec2 thisCurve =
            (curCurve->numPoints() > 0 && curPoint > -1 && curPoint < curCurve->numPoints() ? curCurve->points[curPoint]
                                                                                            : vec2{0.f, 0.f});

        // interpolate the point between the two closest distances
        this->curvePoints.emplace_back(0, 0);
        curCurvePoints.emplace_back(0, 0);
        if(distanceAt - lastDistanceAt > 1) {
            const f32 t = (prefDistance - lastDistanceAt) / (distanceAt - lastDistanceAt);
            this->curvePoints[i] = vec2(std::lerp(lastCurve.x, thisCurve.x, t), std::lerp(lastCurve.y, thisCurve.y, t));
        } else
            this->curvePoints[i] = thisCurve;

        // add the point to the current segment (this is not using the lerp'd point! would cause mm mesh artifacts if it
        // did)
        lastCurvePointForNextSegmentStart = thisCurve;
        curCurvePoints.back() = thisCurve;
    }

    // if we only had one segment, no jump to any next curve has occurred (and therefore no insertion of the segment
    // into the vector) manually add the lone segment here
    if(curCurvePoints.size() > 0) this->curvePointSegments.push_back(curCurvePoints);

    // sanity check
    // spec: FIXME: at least one of my maps triggers this (in upstream mcosu too), try to fix
    if(this->curvePoints.size() == 0) {
        debugLog("SliderCurveEqualDistanceMulti::init: Error: this->curvePoints.size() == 0!!!");

        return;
    }

    // make sure that the uninterpolated segment points are exactly as long as the pixelLength
    // this is necessary because we can't use the lerp'd points for the segments
    f32 segmentedLength = 0.0f;
    for(const auto &curvePointSegment : this->curvePointSegments) {
        for(u64 p = 0; p < curvePointSegment.size(); p++) {
            segmentedLength += ((p == 0) ? 0 : vec::length(curvePointSegment[p] - curvePointSegment[p - 1]));
        }
    }

    // TODO: this is still incorrect. sliders are sometimes too long or start too late, even if only for a few pixels
    if(segmentedLength > this->fPixelLength && this->curvePointSegments.size() > 1 &&
       this->curvePointSegments[0].size() > 1) {
        f32 excess = segmentedLength - this->fPixelLength;
        while(excess > 0) {
            for(i64 s = (i64)this->curvePointSegments.size() - 1; s >= 0; s--) {
                for(i64 p = (i64)this->curvePointSegments[s].size() - 1; p >= 0; p--) {
                    const f32 curLength =
                        (p == 0) ? 0 : vec::length(this->curvePointSegments[s][p] - this->curvePointSegments[s][p - 1]);
                    if(curLength >= excess && p != 0) {
                        vec2 segmentVector =
                            vec::normalize(this->curvePointSegments[s][p] - this->curvePointSegments[s][p - 1]);
                        this->curvePointSegments[s][p] = this->curvePointSegments[s][p] - segmentVector * excess;
                        excess = 0.0f;
                        break;
                    } else {
                        this->curvePointSegments[s].erase(this->curvePointSegments[s].begin() + p);
                        excess -= curLength;
                    }
                }
            }
        }
    }

    // calculate start and end angles for possible repeats (good enough and cheaper than calculating it live every
    // frame)
    if(this->curvePoints.size() > 1) {
        vec2 c1 = this->curvePoints[0];
        u64 cnt = 1;
        vec2 c2 = this->curvePoints[cnt++];
        while(cnt <= this->iNCurve && cnt < this->curvePoints.size() && vec::length(c2 - c1) < 1) {
            c2 = this->curvePoints[cnt++];
        }
        this->fStartAngle = (f32)(std::atan2(c2.y - c1.y, c2.x - c1.x) * 180 / PI);
    }

    if(this->curvePoints.size() > 1) {
        if(this->iNCurve < this->curvePoints.size()) {
            vec2 c1 = this->curvePoints[this->iNCurve];
            i64 cnt = this->iNCurve - 1;
            vec2 c2 = this->curvePoints[cnt--];
            while(cnt >= 0 && vec::length(c2 - c1) < 1) {
                c2 = this->curvePoints[cnt--];
            }
            this->fEndAngle = (f32)(std::atan2(c2.y - c1.y, c2.x - c1.x) * 180 / PI);
        }
    }

    // backup (for dynamic updateStackPosition() recalculation)
    this->originalCurvePoints = this->curvePoints;                // copy
    this->originalCurvePointSegments = this->curvePointSegments;  // copy
}

vec2 SliderCurveEqualDistanceMulti::pointAt(f32 t) const {
    if(this->curvePoints.size() < 1) return {0.f, 0.f};

    const f64 indexD = (f64)t * this->iNCurve;
    const u64 index = (u64)indexD;
    if(index >= this->iNCurve) {
        if(this->iNCurve < this->curvePoints.size())
            return this->curvePoints[this->iNCurve];
        else {
            debugLog("SliderCurveEqualDistanceMulti::pointAt() Error: Illegal index {:d}!!!", this->iNCurve);
            return {0.f, 0.f};
        }
    } else {
        if(index + 1 >= this->curvePoints.size()) {
            debugLog("SliderCurveEqualDistanceMulti::pointAt() Error: Illegal index {:d}!!!", index);
            return {0.f, 0.f};
        }

        const vec2 poi = this->curvePoints[index];
        const vec2 poi2 = this->curvePoints[index + 1];

        const f32 t2 = (f32)(indexD - (f64)index);

        return {std::lerp(poi.x, poi2.x, t2), std::lerp(poi.y, poi2.y, t2)};
    }
}

vec2 SliderCurveEqualDistanceMulti::originalPointAt(f32 t) const {
    if(this->originalCurvePoints.size() < 1) return {0.f, 0.f};

    const f64 indexD = (f64)t * this->iNCurve;
    const u64 index = (u64)indexD;
    if(index >= this->iNCurve) {
        if(this->iNCurve < this->originalCurvePoints.size())
            return this->originalCurvePoints[this->iNCurve];
        else {
            debugLog("SliderCurveEqualDistanceMulti::originalPointAt() Error: Illegal index {:d}!!!", this->iNCurve);
            return {0.f, 0.f};
        }
    } else {
        if(index + 1 >= this->originalCurvePoints.size()) {
            debugLog("SliderCurveEqualDistanceMulti::originalPointAt() Error: Illegal index {:d}!!!", index);
            return {0.f, 0.f};
        }

        const vec2 poi = this->originalCurvePoints[index];
        const vec2 poi2 = this->originalCurvePoints[index + 1];

        const f32 t2 = (f32)(indexD - (f64)index);

        return {std::lerp(poi.x, poi2.x, t2), std::lerp(poi.y, poi2.y, t2)};
    }
}

//*******************//
//	 Bezier Curves	 //
//*******************//

std::vector<vec2> SliderCurveEqualDistanceMulti::SliderCurveDetails::initBezier(const std::vector<vec2> &initPoints) {
    // init helper
    struct SliderBezierApproximator {
        // https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Objects/BezierApproximator.cs
        // https://github.com/ppy/osu-framework/blob/master/osu.Framework/MathUtils/PathApproximator.cs
        // Copyright (c) ppy Pty Ltd <contact@ppy.sh>. Licensed under the MIT Licence.
       private:
        inline bool isFlatEnough(const std::vector<vec2> &controlPoints) {
            static constexpr const f64 TOLERANCE_SQ{0.25 * 0.25};

            if(controlPoints.size() < 1) return true;

            for(u64 i = 1; i < controlPoints.size() - 1; i++) {
                if(pow((f64)vec::length(controlPoints[i - 1] - 2.f * controlPoints[i] + controlPoints[i + 1]), 2.0) >
                   TOLERANCE_SQ * 4)
                    return false;
            }

            return true;
        }

        inline void subdivide(std::vector<vec2> &controlPoints, std::vector<vec2> &l, std::vector<vec2> &r) {
            std::vector<vec2> &midpoints = this->subdivisionBuffer1;

            for(u64 i = 0; i < this->iCount; ++i) {
                midpoints[i] = controlPoints[i];
            }

            for(u64 i = 0; i < this->iCount; i++) {
                l[i] = midpoints[0];
                r[this->iCount - i - 1] = midpoints[this->iCount - i - 1];

                for(u64 j = 0; j < this->iCount - i - 1; j++) {
                    midpoints[j] = (midpoints[j] + midpoints[j + 1]) / 2.f;
                }
            }
        }

        inline void approximate(std::vector<vec2> &controlPoints, std::vector<vec2> &output) {
            std::vector<vec2> &l = this->subdivisionBuffer2;
            std::vector<vec2> &r = this->subdivisionBuffer1;

            this->subdivide(controlPoints, l, r);

            for(u64 i = 0; i < this->iCount - 1; ++i) {
                l[this->iCount + i] = r[i + 1];
            }

            output.push_back(controlPoints[0]);
            for(u64 i = 1; i < this->iCount - 1; ++i) {
                const u64 index = 2 * i;
                vec2 p = 0.25f * (l[index - 1] + 2.f * l[index] + l[index + 1]);
                output.push_back(p);
            }
        }

        std::vector<vec2> subdivisionBuffer1;
        std::vector<vec2> subdivisionBuffer2;
        u64 iCount{0};

       public:
        inline std::vector<vec2> createBezier(const std::vector<vec2> &controlPoints) {
            this->iCount = controlPoints.size();

            std::vector<vec2> output;
            if(this->iCount == 0) return output;

            this->subdivisionBuffer1.resize(this->iCount);
            this->subdivisionBuffer2.resize(this->iCount * 2 - 1);

            std::stack<std::vector<vec2>> toFlatten;
            std::stack<std::vector<vec2>> freeBuffers;

            toFlatten.emplace(controlPoints);  // copy

            std::vector<vec2> &leftChild = this->subdivisionBuffer2;

            while(toFlatten.size() > 0) {
                std::vector<vec2> parent = std::move(toFlatten.top());
                toFlatten.pop();

                if(this->isFlatEnough(parent)) {
                    this->approximate(parent, output);
                    freeBuffers.push(std::move(parent));
                    continue;
                }

                std::vector<vec2> rightChild;
                if(freeBuffers.size() > 0) {
                    rightChild = std::move(freeBuffers.top());
                    freeBuffers.pop();
                } else
                    rightChild.resize(this->iCount);

                this->subdivide(parent, leftChild, rightChild);

                for(u64 i = 0; i < this->iCount; ++i) {
                    parent[i] = leftChild[i];
                }

                toFlatten.push(std::move(rightChild));
                toFlatten.push(std::move(parent));
            }

            output.push_back(controlPoints[this->iCount - 1]);
            return output;
        }
    };

    // precalculate all intermediary points
    return SliderBezierApproximator().createBezier(initPoints);
}

// beziers
SliderCurveEqualDistanceMulti::SliderCurveEqualDistanceMulti(std::vector<vec2> controlPoints_, f32 pixelLength,
                                                             f32 curvePointsSeparation, bool line)
    : SliderCurve(std::move(controlPoints_), pixelLength) {
    const u32 max_points = SLIDER_CURVE_MAX_POINTS;
    this->iNCurve =
        std::min((u32)(this->fPixelLength / std::clamp<f32>(curvePointsSeparation, 1.0f, 100.0f)), max_points);

    const u64 numControlPoints = this->controlPoints.size();

    std::vector<SliderCurveDetails> beziers;

    std::vector<vec2> points;  // temporary list of points to separate different bezier curves

    // Beziers: splits points into different Beziers if has the same points (red points)
    // a b c - c d - d e f g
    // Lines: generate a new curve for each sequential pair
    // ab  bc  cd  de  ef  fg
    vec2 lastPoint{0.f};
    for(u64 i = 0; i < numControlPoints; i++) {
        const vec2 currentPoint = this->controlPoints[i];

        if(line) {
            if(i > 0) {
                points.push_back(currentPoint);

                beziers.emplace_back(BEZIER, points);

                points.clear();
            }
        } else if(i > 0) {
            if(currentPoint == lastPoint) {
                if(points.size() >= 2) {
                    beziers.emplace_back(BEZIER, points);
                }

                points.clear();
            }
        }

        points.push_back(currentPoint);
        lastPoint = currentPoint;
    }

    if(line || points.size() < 2) {
        // trying to continue Bezier with less than 2 points
        // probably ending on a red point, just ignore it
    } else {
        beziers.emplace_back(BEZIER, points);

        points.clear();
    }

    // build curve
    if(beziers.size() > 0) {
        this->init(beziers);
    } else {
        debugLog(
            "ERROR: beziers.size() == 0 (line: {} points.size: {} numControlPoints: {} pixelLength: {} "
            "curvePointsSeparation: {})",
            line, points.size(), numControlPoints, pixelLength, curvePointsSeparation);
    }
}

//********************//
//   Catmull Curves   //
//********************//

std::vector<vec2> SliderCurveEqualDistanceMulti::SliderCurveDetails::initCatmull(const std::vector<vec2> &initPoints) {
    if(initPoints.size() != 4) {
        debugLog("SliderCurveType::initCatmull() Error: points.size() != 4!!!");
        return {};
    }

    f32 approxLength = 0;
    for(i32 i = 1; i < 4; i++) {
        approxLength += std::max(0.0001f, vec::length(initPoints[i] - initPoints[i - 1]));
    }

    // init helper lambda
    const auto cPointAt = [&initPoints] [[gnu::always_inline]] (f32 t) -> vec2 {
        t = t * (2 - 1) + 1;

        const vec2 A1 = initPoints[0] * ((1 - t) / (1 - 0)) + (initPoints[1] * ((t - 0) / (1 - 0)));
        const vec2 A2 = initPoints[1] * ((2 - t) / (2 - 1)) + (initPoints[2] * ((t - 1) / (2 - 1)));
        const vec2 A3 = initPoints[2] * ((3 - t) / (3 - 2)) + (initPoints[3] * ((t - 2) / (3 - 2)));

        const vec2 B1 = A1 * ((2 - t) / (2 - 0)) + (A2 * ((t - 0) / (2 - 0)));
        const vec2 B2 = A2 * ((3 - t) / (3 - 1)) + (A3 * ((t - 1) / (3 - 1)));

        const vec2 C = B1 * ((2 - t) / (2 - 1)) + (B2 * ((t - 1) / (2 - 1)));

        return C;
    };

    // subdivide the curve, calculate all intermediary points
    const i32 numPoints = (i32)(approxLength / 8.0f) + 2;

    std::vector<vec2> ret;
    ret.reserve(numPoints);
    for(i32 i = 0; i < numPoints; i++) {
        ret.emplace_back(cPointAt((f32)i / (f32)(numPoints - 1)));
    }

    return ret;
}

SliderCurveEqualDistanceMulti::SliderCurveEqualDistanceMulti(std::vector<vec2> controlPoints_, f32 pixelLength,
                                                             f32 curvePointsSeparation)
    : SliderCurve(std::move(controlPoints_), pixelLength) {
    const u32 max_points = SLIDER_CURVE_MAX_POINTS;
    this->iNCurve =
        std::min((u32)(this->fPixelLength / std::clamp<f32>(curvePointsSeparation, 1.0f, 100.0f)), max_points);

    const u64 numControlPoints = this->controlPoints.size();

    std::vector<SliderCurveDetails> catmulls;

    std::vector<vec2> points;  // temporary list of points to separate different curves

    // repeat the first and last points as controls points
    // only if the first/last two points are different
    // aabb
    // aabc abcc
    // aabc abcd bcdd
    if(this->controlPoints[0].x != this->controlPoints[1].x || this->controlPoints[0].y != this->controlPoints[1].y)
        points.push_back(this->controlPoints[0]);

    for(u64 i = 0; i < numControlPoints; i++) {
        points.push_back(this->controlPoints[i]);
        if(points.size() >= 4) {
            catmulls.emplace_back(CATMULL, points);

            points.erase(points.begin());
        }
    }

    if(this->controlPoints[numControlPoints - 1].x != this->controlPoints[numControlPoints - 2].x ||
       this->controlPoints[numControlPoints - 1].y != this->controlPoints[numControlPoints - 2].y)
        points.push_back(this->controlPoints[numControlPoints - 1]);

    if(points.size() >= 4) {
        catmulls.emplace_back(CATMULL, points);
    }

    // build curve
    if(catmulls.size() > 0) {
        this->init(catmulls);
    } else {
        debugLog(
            "ERROR: catmulls.size() == 0 (points.size: {} numControlPoints: {} pixelLength: {} curvePointsSeparation: "
            "{})",
            points.size(), numControlPoints, pixelLength, curvePointsSeparation);
    }
}

//**********************//
//	 Circular Curves	//
//**********************//

class SliderCurveCircumscribedCircle final : public SliderCurve {
   public:
    SliderCurveCircumscribedCircle(std::vector<vec2> controlPoints, f32 pixelLength, f32 curvePointsSeparation);

    SliderCurveCircumscribedCircle(const SliderCurveCircumscribedCircle &) = default;
    SliderCurveCircumscribedCircle &operator=(const SliderCurveCircumscribedCircle &) = default;
    SliderCurveCircumscribedCircle(SliderCurveCircumscribedCircle &&) = default;
    SliderCurveCircumscribedCircle &operator=(SliderCurveCircumscribedCircle &&) = default;
    ~SliderCurveCircumscribedCircle() override = default;

    [[nodiscard]] vec2 pointAt(f32 t) const override;
    [[nodiscard]] vec2 originalPointAt(f32 t) const override;

    void updateStackPosition(f32 stackMulStackOffset,
                             bool HR) override;  // must also override this, due to the custom pointAt() function!

   private:
    [[nodiscard]] static vec2 intersect(vec2 a, vec2 ta, vec2 b, vec2 tb);

    [[nodiscard]] static forceinline bool isIn(f32 a, f32 b, f32 c) { return ((b > a && b < c) || (b < a && b > c)); }

    vec2 vCircleCenter{0.f};
    vec2 vOriginalCircleCenter{0.f};
    f32 fRadius;
    f32 fCalculationStartAngle;
    f32 fCalculationEndAngle;
};

SliderCurveCircumscribedCircle::SliderCurveCircumscribedCircle(std::vector<vec2> controlPoints, f32 pixelLength,
                                                               f32 curvePointsSeparation)
    : SliderCurve(std::move(controlPoints), pixelLength) {
    if(this->controlPoints.size() != 3) {
        debugLog("SliderCurveCircumscribedCircle() Error: controlPoints.size() != 3");
        return;
    }

    // construct the three points
    const vec2 start = this->controlPoints[0];
    const vec2 mid = this->controlPoints[1];
    const vec2 end = this->controlPoints[2];

    // find the circle center
    const vec2 mida = start + (mid - start) * 0.5f;
    const vec2 midb = end + (mid - end) * 0.5f;

    vec2 nora = mid - start;
    f32 temp = nora.x;
    nora.x = -nora.y;
    nora.y = temp;
    vec2 norb = mid - end;
    temp = norb.x;
    norb.x = -norb.y;
    norb.y = temp;

    this->vOriginalCircleCenter = this->intersect(mida, nora, midb, norb);
    this->vCircleCenter = this->vOriginalCircleCenter;

    // find the angles relative to the circle center
    vec2 startAngPoint = start - this->vCircleCenter;
    vec2 midAngPoint = mid - this->vCircleCenter;
    vec2 endAngPoint = end - this->vCircleCenter;

    this->fCalculationStartAngle = (f32)std::atan2(startAngPoint.y, startAngPoint.x);
    const auto midAng = (f32)std::atan2(midAngPoint.y, midAngPoint.x);
    this->fCalculationEndAngle = (f32)std::atan2(endAngPoint.y, endAngPoint.x);

    // find the angles that pass through midAng
    if(!this->isIn(this->fCalculationStartAngle, midAng, this->fCalculationEndAngle)) {
        if(std::abs(this->fCalculationStartAngle + 2.f * PI - this->fCalculationEndAngle) < 2.f * PI &&
           this->isIn(this->fCalculationStartAngle + (2.f * PI), midAng, this->fCalculationEndAngle))
            this->fCalculationStartAngle += 2.f * PI;
        else if(std::abs(this->fCalculationStartAngle - (this->fCalculationEndAngle + 2.f * PI)) < 2.f * PI &&
                this->isIn(this->fCalculationStartAngle, midAng, this->fCalculationEndAngle + (2.f * PI)))
            this->fCalculationEndAngle += 2.f * PI;
        else if(std::abs(this->fCalculationStartAngle - 2.f * PI - this->fCalculationEndAngle) < 2.f * PI &&
                this->isIn(this->fCalculationStartAngle - (2.f * PI), midAng, this->fCalculationEndAngle))
            this->fCalculationStartAngle -= 2.f * PI;
        else if(std::abs(this->fCalculationStartAngle - (this->fCalculationEndAngle - 2.f * PI)) < 2.f * PI &&
                this->isIn(this->fCalculationStartAngle, midAng, this->fCalculationEndAngle - (2.f * PI)))
            this->fCalculationEndAngle -= 2.f * PI;
        else {
            debugLog("SliderCurveCircumscribedCircle() Error: Cannot find angles between midAng ({} {} {})",
                     this->fCalculationStartAngle, midAng, this->fCalculationEndAngle);
            return;
        }
    }

    // find an angle with an arc length of pixelLength along this circle
    this->fRadius = vec::length(startAngPoint);
    const f32 arcAng = this->fPixelLength / this->fRadius;  // len = theta * r / theta = len / r

    // now use it for our new end angle
    this->fCalculationEndAngle = (this->fCalculationEndAngle > this->fCalculationStartAngle)
                                     ? this->fCalculationStartAngle + arcAng
                                     : this->fCalculationStartAngle - arcAng;

    // find the angles to draw for repeats
    this->fEndAngle = (f32)((this->fCalculationEndAngle +
                             (this->fCalculationStartAngle > this->fCalculationEndAngle ? PI / 2.0f : -PI / 2.0f)) *
                            180.0f / PI);
    this->fStartAngle = (f32)((this->fCalculationStartAngle +
                               (this->fCalculationStartAngle > this->fCalculationEndAngle ? -PI / 2.0f : PI / 2.0f)) *
                              180.0f / PI);

    // calculate points
    const f32 max_points = SLIDER_CURVE_MAX_POINTS;
    const f32 steps = std::min(this->fPixelLength / (std::clamp<f32>(curvePointsSeparation, 1.0f, 100.0f)), max_points);
    const i32 intSteps = (i32)std::round(steps) + 2;  // must guarantee an int range of 0 to steps!
    for(i32 i = 0; i < intSteps; i++) {
        f32 t = std::clamp<f32>((f32)i / steps, 0.0f, 1.0f);
        this->curvePoints.push_back(this->pointAt(t));

        if(t >= 1.0f)  // early break if we've already reached the end
            break;
    }

    // add the segment (no special logic here for SliderCurveCircumscribedCircle, just add the entire vector)
    this->curvePointSegments.emplace_back(this->curvePoints);  // copy

    // backup (for dynamic updateStackPosition() recalculation)
    this->originalCurvePoints = this->curvePoints;                // copy
    this->originalCurvePointSegments = this->curvePointSegments;  // copy
}

void SliderCurveCircumscribedCircle::updateStackPosition(f32 stackMulStackOffset, bool HR) {
    SliderCurve::updateStackPosition(stackMulStackOffset, HR);

    this->vCircleCenter =
        this->vOriginalCircleCenter - vec2(stackMulStackOffset, stackMulStackOffset * (HR ? -1.0f : 1.0f));
}

vec2 SliderCurveCircumscribedCircle::pointAt(f32 t) const {
    const f32 sanityRange =
        SLIDER_CURVE_MAX_LENGTH;  // NOTE: added to fix some aspire problems (endless drawFollowPoints and star calc etc.)
    const f32 ang = std::lerp(this->fCalculationStartAngle, this->fCalculationEndAngle, t);

    return {std::clamp<f32>(std::cos(ang) * this->fRadius + this->vCircleCenter.x, -sanityRange, sanityRange),
            std::clamp<f32>(std::sin(ang) * this->fRadius + this->vCircleCenter.y, -sanityRange, sanityRange)};
}

vec2 SliderCurveCircumscribedCircle::originalPointAt(f32 t) const {
    const f32 sanityRange =
        SLIDER_CURVE_MAX_LENGTH;  // NOTE: added to fix some aspire problems (endless drawFollowPoints and star calc etc.)
    const f32 ang = std::lerp(this->fCalculationStartAngle, this->fCalculationEndAngle, t);

    return {std::clamp<f32>(std::cos(ang) * this->fRadius + this->vOriginalCircleCenter.x, -sanityRange, sanityRange),
            std::clamp<f32>(std::sin(ang) * this->fRadius + this->vOriginalCircleCenter.y, -sanityRange, sanityRange)};
}

vec2 SliderCurveCircumscribedCircle::intersect(vec2 a, vec2 ta, vec2 b, vec2 tb) {
    const f32 des = (tb.x * ta.y - tb.y * ta.x);
    if(std::abs(des) < 0.0001f) {
        debugLog("SliderCurveCircumscribedCircle::intersect() Error: Vectors are parallel!!!");
        return {0.f, 0.f};
    }

    const f32 u = ((b.y - a.y) * ta.x + (a.x - b.x) * ta.y) / des;
    return (b + vec2(tb.x * u, tb.y * u));
}

}  // namespace

//******************************//
//	 Curve Base Class Factory	//
//******************************//

std::unique_ptr<SliderCurve> SliderCurve::createCurve(SLIDERCURVETYPE type, std::vector<vec2> controlPoints,
                                                      f32 pixelLength) {
    const f32 points_separation = SLIDER_CURVE_POINTS_SEPARATION;
    return createCurve(type, std::move(controlPoints), pixelLength, points_separation);
}

std::unique_ptr<SliderCurve> SliderCurve::createCurve(SLIDERCURVETYPE type, std::vector<vec2> controlPoints,
                                                      f32 pixelLength, f32 curvePointsSeparation) {
    if(type == PASSTHROUGH && controlPoints.size() == 3) {
        vec2 nora = controlPoints[1] - controlPoints[0];
        vec2 norb = controlPoints[1] - controlPoints[2];

        f32 temp = nora.x;
        nora.x = -nora.y;
        nora.y = temp;
        temp = norb.x;
        norb.x = -norb.y;
        norb.y = temp;

        // TODO: to properly support all aspire sliders (e.g. Ping), need to use osu circular arc calc + subdivide line
        // segments if they are too big

        if(std::abs(norb.x * nora.y - norb.y * nora.x) < 0.00001f) {
            return std::make_unique<SliderCurveEqualDistanceMulti>(SliderCurveEqualDistanceMulti::createLinearBezier(
                std::move(controlPoints), pixelLength, curvePointsSeparation,
                false));  // vectors parallel, use linear bezier instead
        } else {
            return std::make_unique<SliderCurveCircumscribedCircle>(std::move(controlPoints), pixelLength, curvePointsSeparation);
        }
    } else if(type == CATMULL) {
        return std::make_unique<SliderCurveEqualDistanceMulti>(
            SliderCurveEqualDistanceMulti::createCatmull(std::move(controlPoints), pixelLength, curvePointsSeparation));
    } else {
        return std::make_unique<SliderCurveEqualDistanceMulti>(SliderCurveEqualDistanceMulti::createLinearBezier(
            std::move(controlPoints), pixelLength, curvePointsSeparation, (type == LINEAR)));
    }
}

SliderCurve::SliderCurve(std::vector<vec2> controlPoints, f32 pixelLength) {
    this->controlPoints = std::move(controlPoints);
    this->fPixelLength = pixelLength;

    this->fStartAngle = 0.0f;
    this->fEndAngle = 0.0f;
}

void SliderCurve::updateStackPosition(f32 stackMulStackOffset, bool HR) {
    for(u64 i = 0; i < this->originalCurvePoints.size() && i < this->curvePoints.size(); i++) {
        this->curvePoints[i] =
            this->originalCurvePoints[i] - vec2(stackMulStackOffset, stackMulStackOffset * (HR ? -1.0f : 1.0f));
    }

    for(u64 s = 0; s < this->originalCurvePointSegments.size() && s < this->curvePointSegments.size(); s++) {
        for(u64 p = 0; p < this->originalCurvePointSegments[s].size() && p < this->curvePointSegments[s].size(); p++) {
            this->curvePointSegments[s][p] = this->originalCurvePointSegments[s][p] -
                                             vec2(stackMulStackOffset, stackMulStackOffset * (HR ? -1.0f : 1.0f));
        }
    }
}
