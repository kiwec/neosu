// Copyright (c) 2015, PG & Jeffrey Han (opsu!), All rights reserved.
#include "SliderCurves.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include <stack>

//**********************//
//	 Curve Base Class	//
//**********************//

SliderCurve *SliderCurve::createCurve(char osuSliderCurveType, std::vector<vec2> controlPoints, float pixelLength) {
    const float points_separation = cv::slider_curve_points_separation.getFloat();
    return createCurve(osuSliderCurveType, std::move(controlPoints), pixelLength, points_separation);
}

SliderCurve *SliderCurve::createCurve(char osuSliderCurveType, std::vector<vec2> controlPoints, float pixelLength,
                                      float curvePointsSeparation) {
    if(osuSliderCurveType == OSUSLIDERCURVETYPE_PASSTHROUGH && controlPoints.size() == 3) {
        vec2 nora = controlPoints[1] - controlPoints[0];
        vec2 norb = controlPoints[1] - controlPoints[2];

        float temp = nora.x;
        nora.x = -nora.y;
        nora.y = temp;
        temp = norb.x;
        norb.x = -norb.y;
        norb.y = temp;

        // TODO: to properly support all aspire sliders (e.g. Ping), need to use osu circular arc calc + subdivide line
        // segments if they are too big

        if(std::abs(norb.x * nora.y - norb.y * nora.x) < 0.00001f)
            return new SliderCurveLinearBezier(controlPoints, pixelLength, false,
                                               curvePointsSeparation);  // vectors parallel, use linear bezier instead
        else
            return new SliderCurveCircumscribedCircle(controlPoints, pixelLength, curvePointsSeparation);
    } else if(osuSliderCurveType == OSUSLIDERCURVETYPE_CATMULL)
        return new SliderCurveCatmull(controlPoints, pixelLength, curvePointsSeparation);
    else
        return new SliderCurveLinearBezier(controlPoints, pixelLength,
                                           (osuSliderCurveType == OSUSLIDERCURVETYPE_LINEAR), curvePointsSeparation);
}

SliderCurve::SliderCurve(std::vector<vec2> controlPoints, float pixelLength) {
    this->controlPoints = std::move(controlPoints);
    this->fPixelLength = pixelLength;

    this->fStartAngle = 0.0f;
    this->fEndAngle = 0.0f;
}

void SliderCurve::updateStackPosition(float stackMulStackOffset, bool HR) {
    for(int i = 0; i < this->originalCurvePoints.size() && i < this->curvePoints.size(); i++) {
        this->curvePoints[i] =
            this->originalCurvePoints[i] - vec2(stackMulStackOffset, stackMulStackOffset * (HR ? -1.0f : 1.0f));
    }

    for(int s = 0; s < this->originalCurvePointSegments.size() && s < this->curvePointSegments.size(); s++) {
        for(int p = 0; p < this->originalCurvePointSegments[s].size() && p < this->curvePointSegments[s].size(); p++) {
            this->curvePointSegments[s][p] = this->originalCurvePointSegments[s][p] -
                                             vec2(stackMulStackOffset, stackMulStackOffset * (HR ? -1.0f : 1.0f));
        }
    }
}

//******************************************//
//	 Curve Type Base Class & Curve Types	//
//******************************************//

SliderCurveType::SliderCurveType() { this->fTotalDistance = 0.0f; }

void SliderCurveType::init(float approxLength) {
    this->calculateIntermediaryPoints(approxLength);
    this->calculateCurveDistances();
}

void SliderCurveType::initCustom(std::vector<vec2> points) {
    this->points = std::move(points);
    this->calculateCurveDistances();
}

void SliderCurveType::calculateIntermediaryPoints(float approxLength) {
    // subdivide the curve, calculate all intermediary points
    const int numPoints = (int)(approxLength / 4.0f) + 2;
    this->points.reserve(numPoints);
    for(int i = 0; i < numPoints; i++) {
        this->points.push_back(this->pointAt((float)i / (float)(numPoints - 1)));
    }
}

void SliderCurveType::calculateCurveDistances() {
    // reset
    this->fTotalDistance = 0.0f;
    this->curveDistances.clear();

    // find the distance of each point from the previous point (needed for some curve types)
    this->curveDistances.reserve(this->points.size());
    for(int i = 0; i < this->points.size(); i++) {
        const float curDist = (i == 0) ? 0 : vec::length(this->points[i] - this->points[i - 1]);

        this->curveDistances.push_back(curDist);
        this->fTotalDistance += curDist;
    }
}

SliderCurveTypeBezier2::SliderCurveTypeBezier2(const std::vector<vec2> &points) : SliderCurveType() {
    this->initCustom(SliderBezierApproximator().createBezier(points));
}

SliderCurveTypeCentripetalCatmullRom::SliderCurveTypeCentripetalCatmullRom(const std::vector<vec2> &points)
    : SliderCurveType(), time() {
    if(points.size() != 4) {
        debugLog("SliderCurveTypeCentripetalCatmullRom() Error: points.size() != 4!!!");
        return;
    }

    this->points = std::vector<vec2>(points);  // copy

    float approxLength = 0;
    for(int i = 1; i < 4; i++) {
        float len = 0;

        if(i > 0) len = vec::length(points[i] - points[i - 1]);

        if(len <= 0) len += 0.0001f;

        approxLength += len;

        // m_time[i] = (float) std::sqrt(len) + time[i-1]; // ^(0.5)
        this->time[i] = i;
    }

    this->init(approxLength / 2.0f);
}

vec2 SliderCurveTypeCentripetalCatmullRom::pointAt(float t) {
    t = t * (this->time[2] - this->time[1]) + this->time[1];

    const vec2 A1 = this->points[0] * ((this->time[1] - t) / (this->time[1] - this->time[0])) +
                       (this->points[1] * ((t - this->time[0]) / (this->time[1] - this->time[0])));
    const vec2 A2 = this->points[1] * ((this->time[2] - t) / (this->time[2] - this->time[1])) +
                       (this->points[2] * ((t - this->time[1]) / (this->time[2] - this->time[1])));
    const vec2 A3 = this->points[2] * ((this->time[3] - t) / (this->time[3] - this->time[2])) +
                       (this->points[3] * ((t - this->time[2]) / (this->time[3] - this->time[2])));

    const vec2 B1 = A1 * ((this->time[2] - t) / (this->time[2] - this->time[0])) +
                       (A2 * ((t - this->time[0]) / (this->time[2] - this->time[0])));
    const vec2 B2 = A2 * ((this->time[3] - t) / (this->time[3] - this->time[1])) +
                       (A3 * ((t - this->time[1]) / (this->time[3] - this->time[1])));

    const vec2 C = B1 * ((this->time[2] - t) / (this->time[2] - this->time[1])) +
                      (B2 * ((t - this->time[1]) / (this->time[2] - this->time[1])));

    return C;
}

//*******************//
//	 Curve Classes	 //
//*******************//

SliderCurveEqualDistanceMulti::SliderCurveEqualDistanceMulti(std::vector<vec2> controlPoints, float pixelLength,
                                                             float curvePointsSeparation)
    : SliderCurve(std::move(controlPoints), pixelLength) {
    const int max_points = cv::slider_curve_max_points.getInt();
    this->iNCurve =
        std::min((int)(this->fPixelLength / std::clamp<float>(curvePointsSeparation, 1.0f, 100.0f)), max_points);
}

void SliderCurveEqualDistanceMulti::init(const std::vector<SliderCurveType *> &curvesList) {
    if(curvesList.size() < 1) {
        debugLog("SliderCurveEqualDistanceMulti::init: Error: curvesList.size() == 0!!!");
        return;
    }

    int curCurveIndex = 0;
    int curPoint = 0;

    float distanceAt = 0.0f;
    float lastDistanceAt = 0.0f;

    SliderCurveType *curCurve = curvesList[curCurveIndex];
    {
        if(curCurve->getCurvePoints().size() < 1) {
            debugLog("SliderCurveEqualDistanceMulti::init: Error: curCurve->getCurvePoints().size() == 0!!!");

            // cleanup
            for(auto i : curvesList) {
                SAFE_DELETE(i);
            }

            return;
        }
    }
    vec2 lastCurve = curCurve->getCurvePoints()[curPoint];

    // length of the curve should be equal to the pixel length
    // for each distance, try to get in between the two points that are between it
    vec2 lastCurvePointForNextSegmentStart{0.f};
    std::vector<vec2> curCurvePoints;
    for(int i = 0; i < (this->iNCurve + 1); i++) {
        const float temp_dist = static_cast<float>((i * this->fPixelLength)) / static_cast<float>(this->iNCurve);
        const int prefDistance =
            (std::isfinite(temp_dist) && temp_dist >= static_cast<float>(std::numeric_limits<int>::min()) &&
             temp_dist <= static_cast<float>(std::numeric_limits<int>::max()))
                ? static_cast<int>(temp_dist)
                : 0;

        while(distanceAt < prefDistance) {
            lastDistanceAt = distanceAt;
            if(curCurve->getCurvePoints().size() > 0 && curPoint > -1 && curPoint < curCurve->getCurvePoints().size())
                lastCurve = curCurve->getCurvePoints()[curPoint];

            // jump to next point
            curPoint++;

            if(curPoint >= curCurve->getNumPoints()) {
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
                    curCurve = curvesList[curCurveIndex];
                    curPoint = 0;
                } else {
                    curPoint = curCurve->getNumPoints() - 1;
                    if(lastDistanceAt == distanceAt) {
                        // out of points even though the preferred distance hasn't been reached
                        break;
                    }
                }
            }

            if(curCurve->getCurveDistances().size() > 0 && curPoint > -1 &&
               curPoint < curCurve->getCurveDistances().size())
                distanceAt += curCurve->getCurveDistances()[curPoint];
        }

        const vec2 thisCurve =
            (curCurve->getCurvePoints().size() > 0 && curPoint > -1 && curPoint < curCurve->getCurvePoints().size()
                 ? curCurve->getCurvePoints()[curPoint]
                 : vec2(0, 0));

        // interpolate the point between the two closest distances
        this->curvePoints.emplace_back(0, 0);
        curCurvePoints.emplace_back(0, 0);
        if(distanceAt - lastDistanceAt > 1) {
            const float t = (prefDistance - lastDistanceAt) / (distanceAt - lastDistanceAt);
            this->curvePoints[i] =
                vec2(std::lerp(lastCurve.x, thisCurve.x, t), std::lerp(lastCurve.y, thisCurve.y, t));
        } else
            this->curvePoints[i] = thisCurve;

        // add the point to the current segment (this is not using the lerp'd point! would cause mm mesh artifacts if it
        // did)
        lastCurvePointForNextSegmentStart = thisCurve;
        curCurvePoints[curCurvePoints.size() - 1] = thisCurve;
    }

    // if we only had one segment, no jump to any next curve has occurred (and therefore no insertion of the segment
    // into the vector) manually add the lone segment here
    if(curCurvePoints.size() > 0) this->curvePointSegments.push_back(curCurvePoints);

    // sanity check
    // spec: FIXME: at least one of my maps triggers this (in upstream mcosu too), try to fix
    if(this->curvePoints.size() == 0) {
        debugLog("SliderCurveEqualDistanceMulti::init: Error: this->curvePoints.size() == 0!!!");

        // cleanup
        for(auto i : curvesList) {
            SAFE_DELETE(i);
        }

        return;
    }

    // make sure that the uninterpolated segment points are exactly as long as the pixelLength
    // this is necessary because we can't use the lerp'd points for the segments
    float segmentedLength = 0.0f;
    for(const auto &curvePointSegment : this->curvePointSegments) {
        for(int p = 0; p < curvePointSegment.size(); p++) {
            segmentedLength += ((p == 0) ? 0 : vec::length(curvePointSegment[p] - curvePointSegment[p - 1]));
        }
    }

    // TODO: this is still incorrect. sliders are sometimes too long or start too late, even if only for a few pixels
    if(segmentedLength > this->fPixelLength && this->curvePointSegments.size() > 1 &&
       this->curvePointSegments[0].size() > 1) {
        float excess = segmentedLength - this->fPixelLength;
        while(excess > 0) {
            for(int s = this->curvePointSegments.size() - 1; s >= 0; s--) {
                for(int p = this->curvePointSegments[s].size() - 1; p >= 0; p--) {
                    const float curLength =
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
        int cnt = 1;
        vec2 c2 = this->curvePoints[cnt++];
        while(cnt <= this->iNCurve && cnt < this->curvePoints.size() && vec::length(c2 - c1) < 1) {
            c2 = this->curvePoints[cnt++];
        }
        this->fStartAngle = (float)(std::atan2(c2.y - c1.y, c2.x - c1.x) * 180 / PI);
    }

    if(this->curvePoints.size() > 1) {
        if(this->iNCurve < this->curvePoints.size()) {
            vec2 c1 = this->curvePoints[this->iNCurve];
            int cnt = this->iNCurve - 1;
            vec2 c2 = this->curvePoints[cnt--];
            while(cnt >= 0 && vec::length(c2 - c1) < 1) {
                c2 = this->curvePoints[cnt--];
            }
            this->fEndAngle = (float)(std::atan2(c2.y - c1.y, c2.x - c1.x) * 180 / PI);
        }
    }

    // backup (for dynamic updateStackPosition() recalculation)
    this->originalCurvePoints = std::vector<vec2>(this->curvePoints);                             // copy
    this->originalCurvePointSegments = std::vector<std::vector<vec2>>(this->curvePointSegments);  // copy

    // cleanup
    for(auto i : curvesList) {
        SAFE_DELETE(i);
    }
}

vec2 SliderCurveEqualDistanceMulti::pointAt(float t) {
    if(this->curvePoints.size() < 1) return vec2(0, 0);

    const float indexF = t * this->iNCurve;
    const int index = (int)indexF;
    if(index >= this->iNCurve) {
        if(this->iNCurve > -1 && this->iNCurve < this->curvePoints.size())
            return this->curvePoints[this->iNCurve];
        else {
            debugLog("SliderCurveEqualDistanceMulti::pointAt() Error: Illegal index {:d}!!!", this->iNCurve);
            return vec2(0, 0);
        }
    } else {
        if(index < 0 || index + 1 >= this->curvePoints.size()) {
            debugLog("SliderCurveEqualDistanceMulti::pointAt() Error: Illegal index {:d}!!!", index);
            return vec2(0, 0);
        }

        const vec2 poi = this->curvePoints[index];
        const vec2 poi2 = this->curvePoints[index + 1];

        const float t2 = indexF - index;

        return vec2(std::lerp(poi.x, poi2.x, t2), std::lerp(poi.y, poi2.y, t2));
    }
}

vec2 SliderCurveEqualDistanceMulti::originalPointAt(float t) {
    if(this->originalCurvePoints.size() < 1) return vec2(0, 0);

    const float indexF = t * this->iNCurve;
    const int index = (int)indexF;
    if(index >= this->iNCurve) {
        if(this->iNCurve > -1 && this->iNCurve < this->originalCurvePoints.size())
            return this->originalCurvePoints[this->iNCurve];
        else {
            debugLog("SliderCurveEqualDistanceMulti::originalPointAt() Error: Illegal index {:d}!!!", this->iNCurve);
            return vec2(0, 0);
        }
    } else {
        if(index < 0 || index + 1 >= this->originalCurvePoints.size()) {
            debugLog("SliderCurveEqualDistanceMulti::originalPointAt() Error: Illegal index {:d}!!!", index);
            return vec2(0, 0);
        }

        const vec2 poi = this->originalCurvePoints[index];
        const vec2 poi2 = this->originalCurvePoints[index + 1];

        const float t2 = indexF - index;

        return vec2(std::lerp(poi.x, poi2.x, t2), std::lerp(poi.y, poi2.y, t2));
    }
}

SliderCurveLinearBezier::SliderCurveLinearBezier(std::vector<vec2> controlPoints, float pixelLength, bool line,
                                                 float curvePointsSeparation)
    : SliderCurveEqualDistanceMulti(std::move(controlPoints), pixelLength, curvePointsSeparation) {
    const int numControlPoints = this->controlPoints.size();

    std::vector<SliderCurveType *> beziers;

    std::vector<vec2> points;  // temporary list of points to separate different bezier curves

    // Beziers: splits points into different Beziers if has the same points (red points)
    // a b c - c d - d e f g
    // Lines: generate a new curve for each sequential pair
    // ab  bc  cd  de  ef  fg
    vec2 lastPoint{0.f};
    for(int i = 0; i < numControlPoints; i++) {
        const vec2 currentPoint = this->controlPoints[i];

        if(line) {
            if(i > 0) {
                points.push_back(currentPoint);

                beziers.push_back(new SliderCurveTypeBezier2(points));

                points.clear();
            }
        } else if(i > 0) {
            if(currentPoint == lastPoint) {
                if(points.size() >= 2) {
                    beziers.push_back(new SliderCurveTypeBezier2(points));
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
        beziers.push_back(new SliderCurveTypeBezier2(points));

        points.clear();
    }

    // build curve
    SliderCurveEqualDistanceMulti::init(beziers);
}

SliderCurveCatmull::SliderCurveCatmull(std::vector<vec2> controlPoints, float pixelLength,
                                       float curvePointsSeparation)
    : SliderCurveEqualDistanceMulti(std::move(controlPoints), pixelLength, curvePointsSeparation) {
    const int numControlPoints = this->controlPoints.size();

    std::vector<SliderCurveType *> catmulls;

    std::vector<vec2> points;  // temporary list of points to separate different curves

    // repeat the first and last points as controls points
    // only if the first/last two points are different
    // aabb
    // aabc abcc
    // aabc abcd bcdd
    if(this->controlPoints[0].x != this->controlPoints[1].x || this->controlPoints[0].y != this->controlPoints[1].y)
        points.push_back(this->controlPoints[0]);

    for(int i = 0; i < numControlPoints; i++) {
        points.push_back(this->controlPoints[i]);
        if(points.size() >= 4) {
            catmulls.push_back(new SliderCurveTypeCentripetalCatmullRom(points));

            points.erase(points.begin());
        }
    }

    if(this->controlPoints[numControlPoints - 1].x != this->controlPoints[numControlPoints - 2].x ||
       this->controlPoints[numControlPoints - 1].y != this->controlPoints[numControlPoints - 2].y)
        points.push_back(this->controlPoints[numControlPoints - 1]);

    if(points.size() >= 4) {
        catmulls.push_back(new SliderCurveTypeCentripetalCatmullRom(points));
    }

    // build curve
    SliderCurveEqualDistanceMulti::init(catmulls);
}

SliderCurveCircumscribedCircle::SliderCurveCircumscribedCircle(std::vector<vec2> controlPoints, float pixelLength,
                                                               float curvePointsSeparation)
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
    float temp = nora.x;
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

    this->fCalculationStartAngle = (float)std::atan2(startAngPoint.y, startAngPoint.x);
    const auto midAng = (float)std::atan2(midAngPoint.y, midAngPoint.x);
    this->fCalculationEndAngle = (float)std::atan2(endAngPoint.y, endAngPoint.x);

    // find the angles that pass through midAng
    if(!this->isIn(this->fCalculationStartAngle, midAng, this->fCalculationEndAngle)) {
        if(std::abs(this->fCalculationStartAngle + 2 * PI - this->fCalculationEndAngle) < 2 * PI &&
           this->isIn(this->fCalculationStartAngle + (2 * PI), midAng, this->fCalculationEndAngle))
            this->fCalculationStartAngle += 2 * PI;
        else if(std::abs(this->fCalculationStartAngle - (this->fCalculationEndAngle + 2 * PI)) < 2 * PI &&
                this->isIn(this->fCalculationStartAngle, midAng, this->fCalculationEndAngle + (2 * PI)))
            this->fCalculationEndAngle += 2 * PI;
        else if(std::abs(this->fCalculationStartAngle - 2 * PI - this->fCalculationEndAngle) < 2 * PI &&
                this->isIn(this->fCalculationStartAngle - (2 * PI), midAng, this->fCalculationEndAngle))
            this->fCalculationStartAngle -= 2 * PI;
        else if(std::abs(this->fCalculationStartAngle - (this->fCalculationEndAngle - 2 * PI)) < 2 * PI &&
                this->isIn(this->fCalculationStartAngle, midAng, this->fCalculationEndAngle - (2 * PI)))
            this->fCalculationEndAngle -= 2 * PI;
        else {
            debugLog(
                "SliderCurveCircumscribedCircle() Error: Cannot find angles between midAng ({:.3f} {:.3f} {:.3f})",
                this->fCalculationStartAngle, midAng, this->fCalculationEndAngle);
            return;
        }
    }

    // find an angle with an arc length of pixelLength along this circle
    this->fRadius = vec::length(startAngPoint);
    const float arcAng = this->fPixelLength / this->fRadius;  // len = theta * r / theta = len / r

    // now use it for our new end angle
    this->fCalculationEndAngle = (this->fCalculationEndAngle > this->fCalculationStartAngle)
                                     ? this->fCalculationStartAngle + arcAng
                                     : this->fCalculationStartAngle - arcAng;

    // find the angles to draw for repeats
    this->fEndAngle = (float)((this->fCalculationEndAngle +
                               (this->fCalculationStartAngle > this->fCalculationEndAngle ? PI / 2.0f : -PI / 2.0f)) *
                              180.0f / PI);
    this->fStartAngle = (float)((this->fCalculationStartAngle +
                                 (this->fCalculationStartAngle > this->fCalculationEndAngle ? -PI / 2.0f : PI / 2.0f)) *
                                180.0f / PI);

    // calculate points
    const float max_points = cv::slider_curve_max_points.getInt();
    const float steps =
        std::min(this->fPixelLength / (std::clamp<float>(curvePointsSeparation, 1.0f, 100.0f)), max_points);
    const int intSteps = (int)std::round(steps) + 2;  // must guarantee an int range of 0 to steps!
    for(int i = 0; i < intSteps; i++) {
        float t = std::clamp<float>((float)i / steps, 0.0f, 1.0f);
        this->curvePoints.push_back(this->pointAt(t));

        if(t >= 1.0f)  // early break if we've already reached the end
            break;
    }

    // add the segment (no special logic here for SliderCurveCircumscribedCircle, just add the entire vector)
    this->curvePointSegments.emplace_back(this->curvePoints);  // copy

    // backup (for dynamic updateStackPosition() recalculation)
    this->originalCurvePoints = std::vector<vec2>(this->curvePoints);                             // copy
    this->originalCurvePointSegments = std::vector<std::vector<vec2>>(this->curvePointSegments);  // copy
}

void SliderCurveCircumscribedCircle::updateStackPosition(float stackMulStackOffset, bool HR) {
    SliderCurve::updateStackPosition(stackMulStackOffset, HR);

    this->vCircleCenter =
        this->vOriginalCircleCenter - vec2(stackMulStackOffset, stackMulStackOffset * (HR ? -1.0f : 1.0f));
}

vec2 SliderCurveCircumscribedCircle::pointAt(float t) {
    const float sanityRange =
        cv::slider_curve_max_length
            .getFloat();  // NOTE: added to fix some aspire problems (endless drawFollowPoints and star calc etc.)
    const float ang = std::lerp(this->fCalculationStartAngle, this->fCalculationEndAngle, t);

    return vec2(std::clamp<float>(std::cos(ang) * this->fRadius + this->vCircleCenter.x, -sanityRange, sanityRange),
                   std::clamp<float>(std::sin(ang) * this->fRadius + this->vCircleCenter.y, -sanityRange, sanityRange));
}

vec2 SliderCurveCircumscribedCircle::originalPointAt(float t) {
    const float sanityRange =
        cv::slider_curve_max_length
            .getFloat();  // NOTE: added to fix some aspire problems (endless drawFollowPoints and star calc etc.)
    const float ang = std::lerp(this->fCalculationStartAngle, this->fCalculationEndAngle, t);

    return vec2(
        std::clamp<float>(std::cos(ang) * this->fRadius + this->vOriginalCircleCenter.x, -sanityRange, sanityRange),
        std::clamp<float>(std::sin(ang) * this->fRadius + this->vOriginalCircleCenter.y, -sanityRange, sanityRange));
}

vec2 SliderCurveCircumscribedCircle::intersect(vec2 a, vec2 ta, vec2 b, vec2 tb) {
    const float des = (tb.x * ta.y - tb.y * ta.x);
    if(std::abs(des) < 0.0001f) {
        debugLog("SliderCurveCircumscribedCircle::intersect() Error: Vectors are parallel!!!");
        return vec2(0, 0);
    }

    const float u = ((b.y - a.y) * ta.x + (a.x - b.x) * ta.y) / des;
    return (b + vec2(tb.x * u, tb.y * u));
}

//********************//
//	 Helper Classes	  //
//********************//

// https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Objects/BezierApproximator.cs
// https://github.com/ppy/osu-framework/blob/master/osu.Framework/MathUtils/PathApproximator.cs
// Copyright (c) ppy Pty Ltd <contact@ppy.sh>. Licensed under the MIT Licence.

double SliderBezierApproximator::TOLERANCE_SQ = 0.25 * 0.25;

SliderBezierApproximator::SliderBezierApproximator() { this->iCount = 0; }

std::vector<vec2> SliderBezierApproximator::createBezier(const std::vector<vec2> &controlPoints) {
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
            freeBuffers.push(parent);
            continue;
        }

        std::vector<vec2> rightChild;
        if(freeBuffers.size() > 0) {
            rightChild = std::move(freeBuffers.top());
            freeBuffers.pop();
        } else
            rightChild.resize(this->iCount);

        this->subdivide(parent, leftChild, rightChild);

        for(int i = 0; i < this->iCount; ++i) {
            parent[i] = leftChild[i];
        }

        toFlatten.push(std::move(rightChild));
        toFlatten.push(std::move(parent));
    }

    output.push_back(controlPoints[this->iCount - 1]);
    return output;
}

bool SliderBezierApproximator::isFlatEnough(const std::vector<vec2> &controlPoints) {
    if(controlPoints.size() < 1) return true;

    for(int i = 1; i < (int)(controlPoints.size() - 1); i++) {
        if(pow((double)vec::length(controlPoints[i - 1] - 2.f * controlPoints[i] + controlPoints[i + 1]), 2.0) >
           TOLERANCE_SQ * 4)
            return false;
    }

    return true;
}

void SliderBezierApproximator::subdivide(std::vector<vec2> &controlPoints, std::vector<vec2> &l,
                                         std::vector<vec2> &r) {
    std::vector<vec2> &midpoints = this->subdivisionBuffer1;

    for(int i = 0; i < this->iCount; ++i) {
        midpoints[i] = controlPoints[i];
    }

    for(int i = 0; i < this->iCount; i++) {
        l[i] = midpoints[0];
        r[this->iCount - i - 1] = midpoints[this->iCount - i - 1];

        for(int j = 0; j < this->iCount - i - 1; j++) {
            midpoints[j] = (midpoints[j] + midpoints[j + 1]) / 2.f;
        }
    }
}

void SliderBezierApproximator::approximate(std::vector<vec2> &controlPoints, std::vector<vec2> &output) {
    std::vector<vec2> &l = this->subdivisionBuffer2;
    std::vector<vec2> &r = this->subdivisionBuffer1;

    this->subdivide(controlPoints, l, r);

    for(int i = 0; i < this->iCount - 1; ++i) {
        l[this->iCount + i] = r[i + 1];
    }

    output.push_back(controlPoints[0]);
    for(int i = 1; i < this->iCount - 1; ++i) {
        const int index = 2 * i;
        vec2 p = 0.25f * (l[index - 1] + 2.f * l[index] + l[index + 1]);
        output.push_back(p);
    }
}
