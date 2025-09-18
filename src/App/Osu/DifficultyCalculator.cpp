// Copyright (c) 2019, PG & Francesco149, All rights reserved.
#include "DifficultyCalculator.h"

#include <algorithm>

#include "Playfield.h"
#include "ConVar.h"
#include "Engine.h"
#include "GameRules.h"
#include "Osu.h"
#include "SliderCurves.h"

OsuDifficultyHitObject::OsuDifficultyHitObject(TYPE type, vec2 pos, i32 time)
    : OsuDifficultyHitObject(type, pos, time, time) {}

OsuDifficultyHitObject::OsuDifficultyHitObject(TYPE type, vec2 pos, i32 time, i32 endTime)
    : OsuDifficultyHitObject(type, pos, time, endTime, 0.0f, '\0', std::vector<vec2>(), 0.0f,
                             std::vector<SLIDER_SCORING_TIME>(), 0, true) {}

OsuDifficultyHitObject::OsuDifficultyHitObject(TYPE type, vec2 pos, i32 time, i32 endTime, f32 spanDuration,
                                               i8 osuSliderCurveType, const std::vector<vec2> &controlPoints,
                                               f32 pixelLength, std::vector<SLIDER_SCORING_TIME> scoringTimes,
                                               i32 repeats, bool calculateSliderCurveInConstructor) {
    this->type = type;
    this->pos = pos;
    this->time = time;
    this->endTime = endTime;
    this->spanDuration = spanDuration;
    this->osuSliderCurveType = osuSliderCurveType;
    this->pixelLength = pixelLength;
    this->scoringTimes = std::move(scoringTimes);

    this->curve = nullptr;
    this->scheduledCurveAlloc = false;
    this->scheduledCurveAllocStackOffset = 0.0f;
    this->repeats = repeats;

    this->stack = 0;
    this->originalPos = this->pos;

    // build slider curve, if this is a (valid) slider
    if(this->type == TYPE::SLIDER && controlPoints.size() > 1) {
        if(calculateSliderCurveInConstructor) {
            // old: too much kept memory allocations for over 14000 sliders in
            // https://osu.ppy.sh/beatmapsets/592138#osu/1277649

            // NOTE: this is still used for beatmaps with less than 5000 sliders (because precomputing all curves is way
            // faster, especially for star cache loader) NOTE: the 5000 slider threshold was chosen by looking at the
            // longest non-aspire marathon maps NOTE: 15000 slider curves use ~1.6 GB of RAM, for 32-bit executables
            // with 2GB cap limiting to 5000 sliders gives ~530 MB which should be survivable without crashing (even
            // though the heap gets fragmented to fuck)

            // 6000 sliders @ The Weather Channel - 139 Degrees (Tiggz Mumbson) [Special Weather Statement].osu
            // 3674 sliders @ Various Artists - Alternator Compilation (Monstrata) [Marathon].osu
            // 4599 sliders @ Renard - Because Maybe! (Mismagius) [- Nogard Marathon -].osu
            // 4921 sliders @ Renard - Because Maybe! (Mismagius) [- Nogard Marathon v2 -].osu
            // 14960 sliders @ pishifat - H E L L O  T H E R E (Kondou-Shinichi) [Sliders in the 69th centries].osu
            // 5208 sliders @ MillhioreF - haitai but every hai adds another haitai in the background (Chewy-san)
            // [Weriko Rank the dream (nerf) but loli].osu

            this->curve = SliderCurve::createCurve(this->osuSliderCurveType, controlPoints, this->pixelLength,
                                                   cv::stars_slider_curve_points_separation.getFloat());
        } else {
            // new: delay curve creation to when it's needed, and also immediately delete afterwards (at the cost of
            // having to store a copy of the control points)
            this->scheduledCurveAlloc = true;
            this->scheduledCurveAllocControlPoints = controlPoints;
        }
    }
}

OsuDifficultyHitObject::~OsuDifficultyHitObject() { SAFE_DELETE(curve); }

OsuDifficultyHitObject::OsuDifficultyHitObject(OsuDifficultyHitObject &&dobj) noexcept {
    // move
    this->type = dobj.type;
    this->pos = dobj.pos;
    this->time = dobj.time;
    this->endTime = dobj.endTime;
    this->spanDuration = dobj.spanDuration;
    this->osuSliderCurveType = dobj.osuSliderCurveType;
    this->pixelLength = dobj.pixelLength;
    this->scoringTimes = std::move(dobj.scoringTimes);

    this->curve = dobj.curve;
    this->scheduledCurveAlloc = dobj.scheduledCurveAlloc;
    this->scheduledCurveAllocControlPoints = std::move(dobj.scheduledCurveAllocControlPoints);
    this->scheduledCurveAllocStackOffset = dobj.scheduledCurveAllocStackOffset;
    this->repeats = dobj.repeats;

    this->stack = dobj.stack;
    this->originalPos = dobj.originalPos;

    // reset source
    dobj.curve = nullptr;
    dobj.scheduledCurveAlloc = false;
}

OsuDifficultyHitObject &OsuDifficultyHitObject::operator=(OsuDifficultyHitObject &&dobj) noexcept {
    // self-assignment check
    if(this == &dobj) return *this;

    SAFE_DELETE(this->curve);
    this->scheduledCurveAllocControlPoints.clear();

    // move all data
    this->type = dobj.type;
    this->pos = dobj.pos;
    this->time = dobj.time;
    this->endTime = dobj.endTime;
    this->spanDuration = dobj.spanDuration;
    this->osuSliderCurveType = dobj.osuSliderCurveType;
    this->pixelLength = dobj.pixelLength;
    this->scoringTimes = std::move(dobj.scoringTimes);

    this->curve = dobj.curve;
    this->scheduledCurveAlloc = dobj.scheduledCurveAlloc;
    this->scheduledCurveAllocControlPoints = std::move(dobj.scheduledCurveAllocControlPoints);
    this->scheduledCurveAllocStackOffset = dobj.scheduledCurveAllocStackOffset;
    this->repeats = dobj.repeats;

    this->stack = dobj.stack;
    this->originalPos = dobj.originalPos;

    // completely reset source object to prevent any potential reuse
    dobj.curve = nullptr;
    dobj.scheduledCurveAlloc = false;
    dobj.scheduledCurveAllocControlPoints.clear();
    dobj.type = TYPE::INVALID;

    return *this;
}

void OsuDifficultyHitObject::updateStackPosition(f32 stackOffset) {
    this->scheduledCurveAllocStackOffset = stackOffset;

    this->pos = this->originalPos - vec2(this->stack * stackOffset, this->stack * stackOffset);

    this->updateCurveStackPosition(stackOffset);
}

void OsuDifficultyHitObject::updateCurveStackPosition(f32 stackOffset) {
    if(this->curve != nullptr) this->curve->updateStackPosition(this->stack * stackOffset, false);
}

vec2 OsuDifficultyHitObject::getOriginalRawPosAt(i32 pos) {
    // NOTE: the delayed curve creation has been deliberately disabled here for stacking purposes for beatmaps with
    // insane slider counts for performance reasons NOTE: this means that these aspire maps will have incorrect stars
    // due to incorrect slider stacking, but the delta is below 0.02 even for the most insane maps which currently exist
    // NOTE: if this were to ever get implemented properly, then a sliding window for curve allocation must be added to
    // the stack algorithm itself (as doing it in here is O(n!) madness) NOTE: to validate the delta, use Acid Rain
    // [Aspire] - Karoo13 (6.76* with slider stacks -> 6.75* without slider stacks)

    if(this->type != TYPE::SLIDER || (this->curve == nullptr)) {
        return this->originalPos;
    } else {
        if(pos <= this->time)
            return this->curve->originalPointAt(0.0f);
        else if(pos >= this->endTime) {
            if(this->repeats % 2 == 0) {
                return this->curve->originalPointAt(0.0f);
            } else {
                return this->curve->originalPointAt(1.0f);
            }
        } else {
            return this->curve->originalPointAt(this->getT(pos, false));
        }
    }
}

f32 OsuDifficultyHitObject::getT(i32 pos, bool raw) {
    f32 t = (f32)((i32)pos - (i32)this->time) / this->spanDuration;
    if(raw)
        return t;
    else {
        f32 floorVal = (f32)std::floor(t);
        return ((i32)floorVal % 2 == 0) ? t - floorVal : floorVal + 1 - t;
    }
}

f64 DifficultyCalculator::calculateStarDiffForHitObjects(StarCalcParams &params) {
    std::atomic<bool> dead;
    dead = false;
    return calculateStarDiffForHitObjects(params, dead);
}

f64 DifficultyCalculator::calculateStarDiffForHitObjects(StarCalcParams &params, const std::atomic<bool> &dead) {
    std::vector<DiffObject> emptyCachedDiffObjects;
    return calculateStarDiffForHitObjectsInt(emptyCachedDiffObjects, params, nullptr, dead);
}

f64 DifficultyCalculator::calculateStarDiffForHitObjectsInt(std::vector<DiffObject> &cachedDiffObjects,
                                                            StarCalcParams &params, IncrementalState *incremental,
                                                            const std::atomic<bool> &dead) {
    // NOTE: depends on speed multiplier + CS + OD + relax + touchDevice

    // NOTE: upToObjectIndex is applied way below, during the construction of the 'dobjects'

    // NOTE: osu always returns 0 stars for beatmaps with only 1 object, except if that object is a slider
    if(params.sortedHitObjects.size() < 2) {
        if(params.sortedHitObjects.size() < 1) return 0.0;
        if(params.sortedHitObjects[0].type != OsuDifficultyHitObject::TYPE::SLIDER) return 0.0;
    }

    // global independent variables/constants

    // NOTE: clamped CS because neosu allows CS > ~12.1429 (at which point the diameter becomes negative)
    f32 circleRadiusInOsuPixels = 64.0f * GameRules::getRawHitCircleScale(std::clamp<f32>(params.CS, 0.0f, 12.142f));
    const f32 hitWindow300 = 2.0f * GameRules::odToMilliseconds(params.OD) / params.speedMultiplier;

    // ******************************************************************************************************************************************
    // //

    // see setDistances() @
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

    static const f32 normalized_radius = 50.0f;  // normalization factor
    static const f32 maximum_slider_radius = normalized_radius * 2.4f;
    static const f32 assumed_slider_radius = normalized_radius * 1.8f;
    static const f32 circlesize_buff_treshold = 30;  // non-normalized diameter where the circlesize buff starts

    // multiplier to normalize positions so that we can calc as if everything was the same circlesize.
    // also handle high CS bonus

    f32 radius_scaling_factor = normalized_radius / circleRadiusInOsuPixels;
    if(circleRadiusInOsuPixels < circlesize_buff_treshold) {
        const f32 smallCircleBonus = std::min(circlesize_buff_treshold - circleRadiusInOsuPixels, 5.0f) / 50.0f;
        radius_scaling_factor *= 1.0f + smallCircleBonus;
    }

    // ******************************************************************************************************************************************
    // //

    // see
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

    class DistanceCalc {
       public:
        static void computeSliderCursorPosition(DiffObject &slider, f32 circleRadius) {
            if(slider.lazyCalcFinished || slider.ho->curve == nullptr) return;

            // NOTE: lazer won't load sliders above a certain length, but neosu will
            // this isn't entirely accurate to how lazer does it (as that skips loading the object entirely),
            // but this is a good middle ground for maps that aren't completely aspire and still have relatively normal
            // star counts on lazer see: DJ Noriken - Stargazer feat. YUC'e (PSYQUI Remix) (Hishiro Chizuru) [Starg-Azer
            // isn't so great? Are you kidding me?]
            if(cv::stars_ignore_clamped_sliders.getBool()) {
                if(slider.ho->curve->getPixelLength() >= cv::slider_curve_max_length.getFloat()) return;
            }

            // NOTE: although this looks like a duplicate of the end tick time, this really does have a noticeable
            // impact on some maps due to precision issues see: Ocelot - KAEDE (Hollow Wings) [EX EX]
            const f64 tailLeniency = (f64)cv::slider_end_inside_check_offset.getInt();
            const f64 totalDuration = (f64)slider.ho->spanDuration * slider.ho->repeats;
            f64 trackingEndTime = (f64)slider.ho->time + std::max(totalDuration - tailLeniency, totalDuration / 2.0);

            // NOTE: lazer has logic to reorder the last slider tick if it happens after trackingEndTime here, which
            // already happens in neosu
            slider.lazyTravelTime = trackingEndTime - (f64)slider.ho->time;

            f64 endTimeMin = slider.lazyTravelTime / slider.ho->spanDuration;
            if(std::fmod(endTimeMin, 2.0) >= 1.0)
                endTimeMin = 1.0 - std::fmod(endTimeMin, 1.0);
            else
                endTimeMin = std::fmod(endTimeMin, 1.0);

            slider.lazyEndPos = slider.ho->curve->pointAt(endTimeMin);

            vec2 cursor_pos = slider.ho->pos;
            f64 scaling_factor = 50.0 / circleRadius;

            for(size_t i = 0; i < slider.ho->scoringTimes.size(); i++) {
                vec2 diff{0.f};

                if(slider.ho->scoringTimes[i].type == OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::END) {
                    // NOTE: In lazer, the position of the slider end is at the visual end, but the time is at the
                    // scoring end
                    diff = slider.ho->curve->pointAt(slider.ho->repeats % 2 ? 1.0 : 0.0) - cursor_pos;
                } else {
                    f64 progress = (std::clamp<f64>(slider.ho->scoringTimes[i].time - (f64)slider.ho->time, 0.f,
                                                    slider.ho->getDuration())) /
                                   slider.ho->spanDuration;
                    if(std::fmod(progress, 2.0) >= 1.0)
                        progress = 1.0 - std::fmod(progress, 1.0);
                    else
                        progress = std::fmod(progress, 1.0);

                    diff = slider.ho->curve->pointAt(progress) - cursor_pos;
                }

                f64 diff_len = scaling_factor * vec::length(diff);

                f64 req_diff = 90.0;

                if(i == slider.ho->scoringTimes.size() - 1) {
                    // Slider end
                    vec2 lazy_diff = slider.lazyEndPos - cursor_pos;
                    if(vec::length(lazy_diff) < vec::length(diff)) diff = lazy_diff;
                    diff_len = scaling_factor * vec::length(diff);
                } else if(slider.ho->scoringTimes[i].type ==
                          OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::REPEAT) {
                    // Slider repeat
                    req_diff = 50.0;
                }

                if(diff_len > req_diff) {
                    cursor_pos += (diff * (f32)((diff_len - req_diff) / diff_len));
                    diff_len *= (diff_len - req_diff) / diff_len;
                    slider.lazyTravelDist += diff_len;
                }

                if(i == slider.ho->scoringTimes.size() - 1) slider.lazyEndPos = cursor_pos;
            }

            slider.lazyCalcFinished = true;
        }

        static vec2 getEndCursorPosition(DiffObject &hitObject, f32 circleRadius) {
            if(hitObject.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                computeSliderCursorPosition(hitObject, circleRadius);
                return hitObject
                    .lazyEndPos;  // (slider.lazyEndPos is already initialized to ho->pos in DiffObject constructor)
            }

            return hitObject.ho->pos;
        }
    };

    // ******************************************************************************************************************************************
    // //

    // initialize dobjects
    const size_t numDiffObjects =
        (params.upToObjectIndex < 0) ? params.sortedHitObjects.size() : params.upToObjectIndex + 1;
    const bool isUsingCachedDiffObjects = (cachedDiffObjects.size() > 0);
    if(!isUsingCachedDiffObjects) {
        cachedDiffObjects.reserve(numDiffObjects);

        // respect upToObjectIndex!
        for(size_t i = 0; i < numDiffObjects; i++) {
            if(dead.load()) return 0.0;

            // this already initializes the angle to NaN
            cachedDiffObjects.emplace_back(&params.sortedHitObjects[i], radius_scaling_factor, cachedDiffObjects,
                                           (i32)i - 1);
        }
    }

    DiffObject *diffObjects = cachedDiffObjects.data();

    // calculate angles and travel/jump distances (before calculating strains)
    if(!isUsingCachedDiffObjects) {
        const f32 starsSliderCurvePointsSeparation = cv::stars_slider_curve_points_separation.getFloat();
        for(size_t i = 0; i < numDiffObjects; i++) {
            if(dead.load()) return 0.0;

            // see setDistances() @
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

            if(i > 0) {
                // calculate travel/jump distances
                DiffObject &cur = diffObjects[i];
                DiffObject &prev1 = diffObjects[i - 1];

                // MCKAY:
                {
                    // delay curve creation to when it's needed (1)
                    if(prev1.ho->scheduledCurveAlloc && prev1.ho->curve == nullptr) {
                        prev1.ho->curve = SliderCurve::createCurve(
                            prev1.ho->osuSliderCurveType, prev1.ho->scheduledCurveAllocControlPoints,
                            prev1.ho->pixelLength, starsSliderCurvePointsSeparation);
                        prev1.ho->updateCurveStackPosition(
                            prev1.ho->scheduledCurveAllocStackOffset);  // NOTE: respect stacking
                    }
                }

                if(cur.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                    DistanceCalc::computeSliderCursorPosition(cur, circleRadiusInOsuPixels);
                    cur.travelDistance = cur.lazyTravelDist * pow(1.0 + (cur.ho->repeats - 1) / 2.5, 1.0 / 2.5);
                    cur.travelTime = std::max(cur.lazyTravelTime, 25.0);
                }

                // don't need to jump to reach spinners
                if(cur.ho->type == OsuDifficultyHitObject::TYPE::SPINNER ||
                   prev1.ho->type == OsuDifficultyHitObject::TYPE::SPINNER)
                    continue;

                const vec2 lastCursorPosition = DistanceCalc::getEndCursorPosition(prev1, circleRadiusInOsuPixels);

                f64 cur_strain_time =
                    (f64)std::max(cur.ho->time - prev1.ho->time, 25);  // strain_time isn't initialized here
                cur.jumpDistance = vec::length((cur.norm_start - lastCursorPosition * radius_scaling_factor));
                cur.minJumpDistance = cur.jumpDistance;
                cur.minJumpTime = cur_strain_time;

                if(prev1.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                    f64 last_travel = std::max(prev1.lazyTravelTime, 25.0);
                    cur.minJumpTime = std::max(cur_strain_time - last_travel, 25.0);

                    // NOTE: "curve shouldn't be null here, but Yin [test7] causes that to happen"
                    // NOTE: the curve can be null if controlPoints.size() < 1 because the OsuDifficultyHitObject()
                    // constructor will then not set scheduledCurveAlloc to true (which is perfectly fine and correct)
                    f32 tail_jump_dist =
                        vec::distance((prev1.ho->curve ? prev1.ho->curve->pointAt(prev1.ho->repeats % 2 ? 1.0 : 0.0)
                                                       : prev1.ho->pos),
                                      cur.ho->pos) *
                        radius_scaling_factor;
                    cur.minJumpDistance = std::max(
                        0.0f, std::min((f32)cur.minJumpDistance - (maximum_slider_radius - assumed_slider_radius),
                                       tail_jump_dist - maximum_slider_radius));
                }

                // calculate angles
                if(i > 1) {
                    DiffObject &prev2 = diffObjects[i - 2];
                    if(prev2.ho->type == OsuDifficultyHitObject::TYPE::SPINNER) continue;

                    const vec2 lastLastCursorPosition =
                        DistanceCalc::getEndCursorPosition(prev2, circleRadiusInOsuPixels);

                    // MCKAY:
                    {
                        // and also immediately delete afterwards (2)
                        if(i > 2)  // NOTE: this trivial sliding window implementation will keep the last 2 curves alive
                                   // at the end, but they get auto deleted later anyway so w/e
                        {
                            DiffObject &prev3 = diffObjects[i - 3];

                            if(prev3.ho->scheduledCurveAlloc) SAFE_DELETE(prev3.ho->curve);
                        }
                    }

                    const vec2 v1 = lastLastCursorPosition - prev1.ho->pos;
                    const vec2 v2 = cur.ho->pos - lastCursorPosition;

                    const f64 dot = vec::dot(v1, v2);
                    const f64 det = (v1.x * v2.y) - (v1.y * v2.x);

                    cur.angle = std::fabs(std::atan2(det, dot));
                }
            }
        }
    }

    // calculate strains/skills
    // NOTE(McKay): yes, this loses some extremely minor accuracy (~0.001 stars territory) for live star/pp for some
    // rare individual upToObjectIndex due to not being recomputed for the cut set of cached diffObjects every time, but
    // the performance gain is so insane I don't care
    if(!isUsingCachedDiffObjects) {
        for(size_t i = 1; i < numDiffObjects; i++)  // NOTE: start at 1
        {
            diffObjects[i].calculate_strains(diffObjects[i - 1],
                                             (i == numDiffObjects - 1) ? nullptr : &diffObjects[i + 1], hitWindow300);
        }
    }

    // calculate final difficulty (weigh strains)
    f64 aimNoSliders =
        DiffObject::calculate_difficulty(Skills::Skill::AIM_NO_SLIDERS, diffObjects, numDiffObjects,
                                         incremental ? &incremental[(size_t)Skills::Skill::AIM_NO_SLIDERS] : nullptr);
    *params.aim =
        DiffObject::calculate_difficulty(Skills::Skill::AIM_SLIDERS, diffObjects, numDiffObjects,
                                         incremental ? &incremental[(size_t)Skills::Skill::AIM_SLIDERS] : nullptr,
                                         params.outAimStrains, params.difficultAimStrains);
    *params.speed =
        DiffObject::calculate_difficulty(Skills::Skill::SPEED, diffObjects, numDiffObjects,
                                         incremental ? &incremental[(size_t)Skills::Skill::SPEED] : nullptr,
                                         params.outSpeedStrains, params.difficultSpeedStrains, params.speedNotes);

    static const f64 star_scaling_factor = 0.0675;

    aimNoSliders = std::sqrt(aimNoSliders) * star_scaling_factor;
    *params.aim = std::sqrt(*params.aim) * star_scaling_factor;
    *params.speed = std::sqrt(*params.speed) * star_scaling_factor;

    *params.aimSliderFactor = (*params.aim > 0) ? aimNoSliders / *params.aim : 1.0;

    if(params.touchDevice) *params.aim = pow(*params.aim, 0.8);

    if(params.relax) {
        *params.aim *= 0.9;
        *params.speed = 0.0;
    }

    f64 baseAimPerformance = pow(5.0 * std::max(1.0, *params.aim / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 baseSpeedPerformance = pow(5.0 * std::max(1.0, *params.speed / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 basePerformance = pow(pow(baseAimPerformance, 1.1) + pow(baseSpeedPerformance, 1.1), 1.0 / 1.1);
    return basePerformance > 0.00001
               ? 1.0476895531716472 /* Math.Cbrt(OsuPerformanceCalculator.PERFORMANCE_BASE_MULTIPLIER) */ * 0.027 *
                     (std::cbrt(100000.0 / pow(2.0, 1 / 1.1) * basePerformance) + 4.0)
               : 0.0;
}

f64 DifficultyCalculator::calculatePPv2(u32 modsLegacy, f64 timescale, f64 ar, f64 od, f64 aim, f64 aimSliderFactor,
                                        f64 aimDifficultStrains, f64 speed, f64 speedNotes, f64 speedDifficultStrains,
                                        i32 numCircles, i32 numSliders, i32 numSpinners, i32 maxPossibleCombo,
                                        i32 combo, i32 misses, i32 c300, i32 c100, i32 c50) {
    // NOTE: depends on active mods + OD + AR

    // apply "timescale" aka speed multiplier to ar/od
    // (the original incoming ar/od values are guaranteed to not yet have any speed multiplier applied to them, but they
    // do have non-time-related mods already applied, like HR or any custom overrides) (yes, this does work correctly
    // when the override slider "locking" feature is used. in this case, the stored ar/od is already compensated such
    // that it will have the locked value AFTER applying the speed multiplier here) (all UI elements which display ar/od
    // from stored scores, like the ranking screen or score buttons, also do this calculation before displaying the
    // values to the user. of course the mod selection screen does too.)
    ar = GameRules::arWithSpeed(ar, timescale);
    od = GameRules::odWithSpeed(od, timescale);

    if(combo < 0) combo = maxPossibleCombo;

    if(combo < 1) return 0.0;

    i32 totalHits = c300 + c100 + c50 + misses;
    f64 accuracy = (totalHits > 0 ? (f64)(c300 * 300 + c100 * 100 + c50 * 50) / (f64)(totalHits * 300) : 0.0);
    i32 amountHitObjectsWithAccuracy =
        (ModMasks::legacy_eq(modsLegacy, LegacyFlags::ScoreV2) ? (numCircles + numSliders) : numCircles);

    // calculateEffectiveMissCount @
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/OsuPerformanceCalculator.cs required
    // because slider breaks aren't exposed to pp calculation
    f64 comboBasedMissCount = 0.0;
    if(numSliders > 0) {
        f64 fullComboThreshold = maxPossibleCombo - (0.1 * numSliders);
        if((f64)combo < fullComboThreshold) {
            comboBasedMissCount = fullComboThreshold / std::max(1.0, (f64)combo);
        }
    }
    f64 effectiveMissCount = std::clamp<f64>(comboBasedMissCount, (f64)misses, (f64)(c50 + c100 + misses));

    // custom multipliers for nofail and spunout
    f64 multiplier = 1.15;  // keep final pp normalized across changes
    {
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::NoFail))
            multiplier *= std::max(
                0.9, 1.0 - 0.02 * effectiveMissCount);  // see https://github.com/ppy/osu-performance/pull/127/files

        if((ModMasks::legacy_eq(modsLegacy, LegacyFlags::SpunOut)) && totalHits > 0)
            multiplier *= 1.0 - pow((f64)numSpinners / (f64)totalHits,
                                    0.85);  // see https://github.com/ppy/osu-performance/pull/110/

        if((ModMasks::legacy_eq(modsLegacy, LegacyFlags::Relax))) {
            f64 okMultiplier = std::max(0.0, od > 0.0 ? 1.0 - pow(od / 13.33, 1.8) : 1.0);   // 100
            f64 mehMultiplier = std::max(0.0, od > 0.0 ? 1.0 - pow(od / 13.33, 5.0) : 1.0);  // 50
            effectiveMissCount =
                std::min(effectiveMissCount + c100 * okMultiplier + c50 * mehMultiplier, (f64)totalHits);
        }
    }

    f64 aimValue = 0.0;
    if(!(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Autopilot))) {
        f64 rawAim = aim;
        aimValue = pow(5.0 * std::max(1.0, rawAim / 0.0675) - 4.0, 3.0) / 100000.0;

        // length bonus
        f64 lengthBonus = 0.95 + 0.4 * std::min(1.0, ((f64)totalHits / 2000.0)) +
                          (totalHits > 2000 ? std::log10(((f64)totalHits / 2000.0)) * 0.5 : 0.0);
        aimValue *= lengthBonus;

        // miss penalty
        // see https://github.com/ppy/osu/pull/16280/
        if(effectiveMissCount > 0 && totalHits > 0) {
            aimValue *= 0.96 / ((effectiveMissCount / (4.0 * pow(log(aimDifficultStrains), 0.94))) + 1.0);
        }

        // ar bonus
        f64 approachRateFactor = 0.0;  // see https://github.com/ppy/osu-performance/pull/125/
        if(!(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Relax))) {
            if(ar > 10.33)
                approachRateFactor =
                    0.3 * (ar - 10.33);  // from 0.3 to 0.4 see https://github.com/ppy/osu-performance/pull/125/ // and
            // completely changed the logic in https://github.com/ppy/osu-performance/pull/135/
            else if(ar < 8.0)
                approachRateFactor =
                    0.05 * (8.0 - ar);  // from 0.01 to 0.1 see https://github.com/ppy/osu-performance/pull/125/
                                        // // and back again from 0.1 to 0.01 see
                                        // https://github.com/ppy/osu-performance/pull/133/ // and completely
                                        // changed the logic in https://github.com/ppy/osu-performance/pull/135/
        }

        aimValue *= 1.0 + approachRateFactor * lengthBonus;

        // hidden
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Hidden))
            aimValue *= 1.0 + 0.04 * (std::max(12.0 - ar,
                                               0.0));  // NOTE: clamped to 0 because neosu allows AR > 12

        // "We assume 15% of sliders in a map are difficult since there's no way to tell from the performance
        // calculator."
        f64 estimateDifficultSliders = numSliders * 0.15;
        if(numSliders > 0) {
            f64 estimateSliderEndsDropped = std::clamp<f64>(
                (f64)std::min(c100 + c50 + misses, maxPossibleCombo - combo), 0.0, estimateDifficultSliders);
            f64 sliderNerfFactor =
                (1.0 - aimSliderFactor) * pow(1.0 - estimateSliderEndsDropped / estimateDifficultSliders, 3.0) +
                aimSliderFactor;
            aimValue *= sliderNerfFactor;
        }

        // scale aim with acc
        aimValue *= accuracy;
        // also consider acc difficulty when doing that
        aimValue *= 0.98 + pow(od, 2.0) / 2500.0;
    }

    f64 speedValue = 0.0;
    if(!(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Relax))) {
        speedValue = pow(5.0 * std::max(1.0, speed / 0.0675) - 4.0, 3.0) / 100000.0;

        // length bonus
        f64 lengthBonus = 0.95 + 0.4 * std::min(1.0, ((f64)totalHits / 2000.0)) +
                          (totalHits > 2000 ? std::log10(((f64)totalHits / 2000.0)) * 0.5 : 0.0);
        speedValue *= lengthBonus;

        // miss penalty
        // see https://github.com/ppy/osu/pull/16280/
        if(effectiveMissCount > 0) {
            speedValue *= 0.96 / ((effectiveMissCount / (4.0 * pow(log(speedDifficultStrains), 0.94))) + 1.0);
        }

        // ar bonus
        f64 approachRateFactor = 0.0;  // see https://github.com/ppy/osu-performance/pull/125/
        if(ar > 10.33)
            approachRateFactor =
                0.3 * (ar - 10.33);  // from 0.3 to 0.4 see https://github.com/ppy/osu-performance/pull/125/ // and
                                     // completely changed the logic in https://github.com/ppy/osu-performance/pull/135/

        speedValue *= 1.0 + approachRateFactor * lengthBonus;

        // hidden
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Hidden))
            speedValue *= 1.0 + 0.04 * (std::max(12.0 - ar,
                                                 0.0));  // NOTE: clamped to 0 because neosu allows AR > 12

        // "Calculate accuracy assuming the worst case scenario"
        f64 relevantTotalDiff = totalHits - speedNotes;
        f64 relevantCountGreat = std::max(0.0, c300 - relevantTotalDiff);
        f64 relevantCountOk = std::max(0.0, c100 - std::max(0.0, relevantTotalDiff - c300));
        f64 relevantCountMeh = std::max(0.0, c50 - std::max(0.0, relevantTotalDiff - c300 - c100));
        f64 relevantAccuracy = speedNotes == 0 ? 0
                                               : (relevantCountGreat * 6.0 + relevantCountOk * 2.0 + relevantCountMeh) /
                                                     (speedNotes * 6.0);

        // see https://github.com/ppy/osu-performance/pull/128/
        // Scale the speed value with accuracy and OD
        speedValue *= (0.95 + pow(od, 2.0) / 750.0) * pow((accuracy + relevantAccuracy) / 2.0, (14.5 - od) / 2.0);
        // Scale the speed value with # of 50s to punish doubletapping.
        speedValue *= pow(0.99, c50 < (totalHits / 500.0) ? 0.0 : c50 - (totalHits / 500.0));
    }

    f64 accuracyValue = 0.0;
    if(!(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Relax))) {
        f64 betterAccuracyPercentage;
        if(amountHitObjectsWithAccuracy > 0)
            betterAccuracyPercentage =
                ((f64)(c300 - (totalHits - amountHitObjectsWithAccuracy)) * 6.0 + (c100 * 2.0) + c50) /
                (f64)(amountHitObjectsWithAccuracy * 6.0);
        else
            betterAccuracyPercentage = 0.0;

        // it's possible to reach negative accuracy, cap at zero
        if(betterAccuracyPercentage < 0.0) betterAccuracyPercentage = 0.0;

        // arbitrary values tom crafted out of trial and error
        accuracyValue = pow(1.52163, od) * pow(betterAccuracyPercentage, 24.0) * 2.83;

        // length bonus
        accuracyValue *= std::min(1.15, pow(amountHitObjectsWithAccuracy / 1000.0, 0.3));

        // hidden bonus
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Hidden)) accuracyValue *= 1.08;
        // flashlight bonus
        if(ModMasks::legacy_eq(modsLegacy, LegacyFlags::Flashlight)) accuracyValue *= 1.02;
    }

    f64 totalValue = pow(pow(aimValue, 1.1) + pow(speedValue, 1.1) + pow(accuracyValue, 1.1), 1.0 / 1.1) * multiplier;
    return totalValue;
}

DifficultyCalculator::DiffObject::DiffObject(OsuDifficultyHitObject *base_object, float radius_scaling_factor,
                                             std::vector<DiffObject> &diff_objects, int prevObjectIdx)
    : objects(diff_objects) {
    this->ho = base_object;

    for(double &strain : this->strains) {
        strain = 0.0;
    }
    this->raw_speed_strain = 0.0;
    this->rhythm = 0.0;

    this->norm_start = this->ho->pos * radius_scaling_factor;

    this->angle = std::numeric_limits<float>::quiet_NaN();

    this->jumpDistance = 0.0;
    this->minJumpDistance = 0.0;
    this->minJumpTime = 0.0;
    this->travelDistance = 0.0;

    this->delta_time = 0.0;
    this->strain_time = 0.0;

    this->lazyCalcFinished = false;
    this->lazyEndPos = this->ho->pos;
    this->lazyTravelDist = 0.0;
    this->lazyTravelTime = 0.0;
    this->travelTime = 0.0;

    this->prevObjectIndex = prevObjectIdx;
}

void DifficultyCalculator::DiffObject::calculate_strains(const DiffObject &prev, const DiffObject *next,
                                                         double hitWindow300) {
    this->calculate_strain(prev, next, hitWindow300, Skills::Skill::SPEED);
    this->calculate_strain(prev, next, hitWindow300, Skills::Skill::AIM_SLIDERS);
    this->calculate_strain(prev, next, hitWindow300, Skills::Skill::AIM_NO_SLIDERS);
}

void DifficultyCalculator::DiffObject::calculate_strain(const DiffObject &prev, const DiffObject *next,
                                                        double hitWindow300, const Skills::Skill dtype) {
    double currentStrainOfDiffObject = 0;

    const long time_elapsed = this->ho->time - prev.ho->time;

    // update our delta time
    this->delta_time = (double)time_elapsed;
    this->strain_time = (double)std::max(time_elapsed, 25l);

    switch(this->ho->type) {
        case OsuDifficultyHitObject::TYPE::SLIDER:
        case OsuDifficultyHitObject::TYPE::CIRCLE:
            currentStrainOfDiffObject = this->spacing_weight2(dtype, prev, next, hitWindow300);
            break;

        case OsuDifficultyHitObject::TYPE::SPINNER:
            break;

        case OsuDifficultyHitObject::TYPE::INVALID:
            // NOTE: silently ignore
            return;
    }

    // see Process() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    double currentStrain = prev.strains[Skills::skillToIndex(dtype)];
    {
        currentStrain *= strainDecay(dtype, dtype == Skills::Skill::SPEED ? this->strain_time : this->delta_time);
        currentStrain += currentStrainOfDiffObject * weight_scaling[Skills::skillToIndex(dtype)];
    }
    this->strains[Skills::skillToIndex(dtype)] = currentStrain;
}

double DifficultyCalculator::DiffObject::calculate_difficulty(const Skills::Skill type, const DiffObject *dobjects,
                                                              size_t dobjectCount, IncrementalState *incremental,
                                                              std::vector<double> *outStrains, f64 *outDifficultStrains,
                                                              double *outRelevantNotes) {
    // (old) see https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    // (new) see https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs

    static const f64 strain_step = 400.0;  // the length of each strain section

    // max strains are weighted from highest to lowest, and this is how much the weight decays.
    static const f64 decay_weight = 0.9;

    if(dobjectCount < 1) return 0.0;

    f64 interval_end =
        incremental ? incremental->interval_end : (std::ceil((f64)dobjects[0].ho->time / strain_step) * strain_step);
    f64 max_strain = incremental ? incremental->max_strain : 0.0;

    std::vector<f64> highestStrains;
    std::vector<f64> *highestStrainsRef = incremental ? &incremental->highest_strains : &highestStrains;
    for(size_t i = (incremental ? dobjectCount - 1 : 0); i < dobjectCount; i++) {
        const DiffObject &cur = dobjects[i];
        const DiffObject &prev = dobjects[i > 0 ? i - 1 : i];

        // make previous peak strain decay until the current object
        while(cur.ho->time > interval_end) {
            if(incremental) {
                highestStrainsRef->insert(std::ranges::upper_bound(*highestStrainsRef, max_strain), max_strain);
            } else {
                highestStrainsRef->push_back(max_strain);
            }

            // skip calculating strain decay for very long breaks (e.g. beatmap upload size limit hack diffs)
            // strainDecay with a base of 0.3 at 60 seconds is 4.23911583e-32, well below any meaningful difference even
            // after being multiplied by object strain
            double strainDelta = interval_end - (double)prev.ho->time;
            if(i < 1 || strainDelta > 600000.0) {  // !prev
                max_strain = 0.0;
            } else {
                max_strain = prev.get_strain(type) * strainDecay(type, strainDelta);
            }

            interval_end += strain_step;
        }

        // calculate max strain for this interval
        double cur_strain = cur.get_strain(type);
        max_strain = std::max(max_strain, cur_strain);
    }

    // the peak strain will not be saved for the last section in the above loop
    if(incremental) {
        incremental->interval_end = interval_end;
        incremental->max_strain = max_strain;
        highestStrains.reserve(incremental->highest_strains.size() + 1);  // required so insert call doesn't reallocate
        highestStrains = incremental->highest_strains;
        highestStrains.insert(std::ranges::upper_bound(highestStrains, max_strain), max_strain);
    } else {
        highestStrains.push_back(max_strain);
    }

    if(outStrains != nullptr) {
        (*outStrains) = highestStrains;  // save a copy
    }

    // calculate relevant speed note count
    // RelevantNoteCount @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
    if(outRelevantNotes) {
        const auto diffObjCompare = [=](const DiffObject &x, const DiffObject &y) {
            return x.get_strain(type) < y.get_strain(type);
        };

        f64 maxObjectStrain;
        if(incremental) {
            maxObjectStrain = std::max(incremental->max_object_strain, dobjects[dobjectCount - 1].get_strain(type));
        } else {
            maxObjectStrain = (*std::max_element(dobjects, dobjects + dobjectCount, diffObjCompare)).get_strain(type);
        }

        if(dobjectCount < 1 || maxObjectStrain == 0.0) {
            *outRelevantNotes = 0.0;
        } else {
            f64 tempSum = 0.0;
            if(incremental && std::abs(incremental->max_object_strain - maxObjectStrain) < DIFFCALC_EPSILON) {
                incremental->relevant_note_sum +=
                    1.0 /
                    (1.0 + std::exp(-(dobjects[dobjectCount - 1].get_strain(type) / maxObjectStrain * 12.0 - 6.0)));
                tempSum = incremental->relevant_note_sum;
            } else {
                for(size_t i = 0; i < dobjectCount; i++) {
                    tempSum += 1.0 / (1.0 + std::exp(-(dobjects[i].get_strain(type) / maxObjectStrain * 12.0 - 6.0)));
                }
                if(incremental) {
                    incremental->max_object_strain = maxObjectStrain;
                    incremental->relevant_note_sum = tempSum;
                }
            }

            *outRelevantNotes = tempSum;
        }
    }

    // (old) see DifficultyValue() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    // (new) see DifficultyValue() @
    // https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs (new) see
    // DifficultyValue() @
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/OsuStrainSkill.cs

    static const size_t reducedSectionCount = 10;
    static const double reducedStrainBaseline = 0.75;

    double difficulty = 0.0;
    double weight = 1.0;

    // sort strains
    // NOTE: lazer does this from highest to lowest, but sorting it in reverse lets the reduced top section loop below
    // have a better average insertion time
    if(!incremental) {
        std::ranges::sort(highestStrains);
    }

    // new implementation (https://github.com/ppy/osu/pull/13483/)
    {
        size_t skillSpecificReducedSectionCount = reducedSectionCount;
        {
            switch(type) {
                case Skills::Skill::SPEED:
                    skillSpecificReducedSectionCount = 5;
                    break;
                case Skills::Skill::AIM_SLIDERS:
                case Skills::Skill::AIM_NO_SLIDERS:
                    break;
            }
        }

        // "We are reducing the highest strains first to account for extreme difficulty spikes"
        size_t actualReducedSectionCount = std::min(highestStrains.size(), skillSpecificReducedSectionCount);
        for(size_t i = 0; i < actualReducedSectionCount; i++) {
            const f64 scale = std::log10(
                std::lerp(1.0, 10.0, std::clamp<f64>((f64)i / (f64)skillSpecificReducedSectionCount, 0.0, 1.0)));
            highestStrains[highestStrains.size() - i - 1] *= std::lerp(reducedStrainBaseline, 1.0, scale);
        }

        // re-sort
        double reducedSections[reducedSectionCount];  // actualReducedSectionCount <= skillSpecificReducedSectionCount
                                                      // <= reducedSectionCount
        memcpy(reducedSections, &highestStrains[highestStrains.size() - actualReducedSectionCount],
               actualReducedSectionCount * sizeof(f64));
        highestStrains.erase(highestStrains.end() - actualReducedSectionCount, highestStrains.end());
        for(size_t i = 0; i < actualReducedSectionCount; i++) {
            highestStrains.insert(std::ranges::upper_bound(highestStrains, reducedSections[i]), reducedSections[i]);
        }

        // weigh the top strains
        for(size_t i = 0; i < highestStrains.size(); i++) {
            f64 last = difficulty;
            difficulty += highestStrains[highestStrains.size() - i - 1] * weight;
            weight *= decay_weight;
            if(std::abs(difficulty - last) < DIFFCALC_EPSILON) {
                break;
            }
        }
    }

    if(outDifficultStrains) {
        if(difficulty == 0.0) {
            *outDifficultStrains = difficulty;
        } else {
            f64 consistentTopStrain = difficulty / 10.0;
            f64 tempSum = 0.0;
            if(incremental && std::abs(incremental->consistent_top_strain - consistentTopStrain) < DIFFCALC_EPSILON) {
                incremental->difficult_strains +=
                    1.1 /
                    (1 + std::exp(-10 * (dobjects[dobjectCount - 1].get_strain(type) / consistentTopStrain - 0.88)));
                tempSum = incremental->difficult_strains;
            } else {
                for(size_t i = 0; i < dobjectCount; i++) {
                    tempSum += 1.1 / (1 + std::exp(-10 * (dobjects[i].get_strain(type) / consistentTopStrain - 0.88)));
                }
                if(incremental) {
                    incremental->consistent_top_strain = consistentTopStrain;
                    incremental->difficult_strains = tempSum;
                }
            }
            *outDifficultStrains = tempSum;
        }
    }

    return difficulty;
}

// old implementation (ppv2.0)
double DifficultyCalculator::DiffObject::spacing_weight1(const double distance, const Skills::Skill diff_type) {
    // arbitrary tresholds to determine when a stream is spaced enough that is becomes hard to alternate.
    static const double single_spacing_threshold = 125.0;
    static const double stream_spacing = 110.0;

    // almost the normalized circle diameter (104px)
    static const double almost_diameter = 90.0;

    switch(diff_type) {
        case Skills::Skill::SPEED:
            if(distance > single_spacing_threshold)
                return 2.5;
            else if(distance > stream_spacing)
                return 1.6 + 0.9 * (distance - stream_spacing) / (single_spacing_threshold - stream_spacing);
            else if(distance > almost_diameter)
                return 1.2 + 0.4 * (distance - almost_diameter) / (stream_spacing - almost_diameter);
            else if(distance > almost_diameter / 2.0)
                return 0.95 + 0.25 * (distance - almost_diameter / 2.0) / (almost_diameter / 2.0);
            else
                return 0.95;

        case Skills::Skill::AIM_SLIDERS:
        case Skills::Skill::AIM_NO_SLIDERS:
            return std::pow(distance, 0.99);
    }

    return 0.0;
}

// new implementation, Xexxar, (ppv2.1), see
// https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/
double DifficultyCalculator::DiffObject::spacing_weight2(const Skills::Skill diff_type, const DiffObject &prev,
                                                         const DiffObject *next, double hitWindow300) {
    static const double single_spacing_threshold = 125.0;

    static const double min_speed_bonus = 75.0; /* ~200BPM 1/4 streams */
    static const double speed_balancing_factor = 40.0;
    static const double distance_multiplier = 0.94;

    static const int history_time_max = 5000;
    static const int history_objects_max = 32;
    static const double rhythm_overall_multiplier = 0.95;
    static const double rhythm_ratio_multiplier = 12.0;

    // double angle_bonus = 1.0; // (apparently unused now in lazer?)

    switch(diff_type) {
        case Skills::Skill::SPEED: {
            // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
            if(this->ho->type == OsuDifficultyHitObject::TYPE::SPINNER) {
                this->raw_speed_strain = 0.0;
                this->rhythm = 0.0;

                return 0.0;
            }

            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/SpeedEvaluator.cs
            const double distance = std::min(single_spacing_threshold, prev.travelDistance + this->minJumpDistance);

            double strain_time = this->strain_time;
            strain_time /= std::clamp<double>((strain_time / hitWindow300) / 0.93, 0.92, 1.0);

            f64 doubletapness = 1.0 - this->get_doubletapness(next, hitWindow300);

            f64 speed_bonus = 0.0;
            if(strain_time < min_speed_bonus) {
                speed_bonus = 0.75 * pow((min_speed_bonus - strain_time) / speed_balancing_factor, 2.0);
            }

            f64 distance_bonus = pow(distance / single_spacing_threshold, 3.95) * distance_multiplier;
            this->raw_speed_strain = (1.0 + speed_bonus + distance_bonus) * 1000.0 * doubletapness / strain_time;

            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/RhythmEvaluator.cs
            f64 rhythmComplexitySum = 0;
            const f64 deltaDifferenceEpsilon = hitWindow300 * 0.3;

            RhythmIsland island{INT_MAX, 0};
            RhythmIsland previousIsland{INT_MAX, 0};

            std::vector<std::pair<RhythmIsland, i32>> islandCounts;

            f64 startRatio = 0.0;  // store the ratio of the current start of an island to buff for tighter rhythms

            bool firstDeltaSwitch = false;

            int historicalNoteCount = std::min(this->prevObjectIndex, history_objects_max);

            int rhythmStart = 0;

            while(rhythmStart < historicalNoteCount - 2 &&
                  this->ho->time - this->get_previous(rhythmStart)->ho->time < history_time_max) {
                rhythmStart++;
            }

            const DiffObject *prevObj = this->get_previous(rhythmStart);
            const DiffObject *lastObj = this->get_previous(rhythmStart + 1);
            for(int i = rhythmStart; i > 0; i--) {
                const DiffObject *currObj = this->get_previous(i - 1);

                // scales note 0 to 1 from history to now
                f64 timeDecay = (history_time_max - (this->ho->time - currObj->ho->time)) / (f64)history_time_max;
                f64 noteDecay = (f64)(historicalNoteCount - i) / historicalNoteCount;
                f64 currHistoricalDecay = std::min(noteDecay, timeDecay);

                double currDelta = currObj->strain_time;
                double prevDelta = prevObj->strain_time;
                double lastDelta = lastObj->strain_time;

                // calculate how much current delta difference deserves a rhythm bonus
                // this function is meant to reduce rhythm bonus for deltas that are multiples of each other (i.e 100
                // and 200)
                f64 deltaDifferenceRatio = std::min(prevDelta, currDelta) / std::max(prevDelta, currDelta);
                f64 currRatio =
                    1.0 + rhythm_ratio_multiplier * std::min(0.5, pow(std::sin(PI / deltaDifferenceRatio), 2.0));

                // reduce ratio bonus if delta difference is too big
                f64 fraction = std::max(prevDelta / currDelta, currDelta / prevDelta);
                f64 fractionMultiplier = std::clamp<f64>(2.0 - fraction / 8.0, 0.0, 1.0);

                f64 windowPenalty =
                    std::min(1.0, std::max(0.0, std::abs(prevDelta - currDelta) - deltaDifferenceEpsilon) /
                                      deltaDifferenceEpsilon);

                f64 effectiveRatio = windowPenalty * currRatio * fractionMultiplier;

                if(firstDeltaSwitch) {
                    if(std::abs(prevDelta - currDelta) < deltaDifferenceEpsilon) {
                        // island is still progressing
                        if(island.delta == INT_MAX) {
                            island.delta = std::max((int)currDelta, 25);
                        }
                        island.deltaCount++;
                    } else {
                        // bpm change is into slider, this is easy acc window
                        if(currObj->ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                            effectiveRatio *= 0.125;
                        }

                        // bpm change was from a slider, this is easier typically than circle -> circle
                        if(prevObj->ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                            effectiveRatio *= 0.3;
                        }

                        // repeated island polarity (2 -> 4, 3 -> 5)
                        if(island.deltaCount % 2 == previousIsland.deltaCount % 2) {
                            effectiveRatio *= 0.5;
                        }

                        // previous increase happened a note ago, 1/1->1/2-1/4, dont want to buff this.
                        if(lastDelta > prevDelta + deltaDifferenceEpsilon &&
                           prevDelta > currDelta + deltaDifferenceEpsilon) {
                            effectiveRatio *= 0.125;
                        }

                        // repeated island size (ex: triplet -> triplet)
                        if(previousIsland.deltaCount == island.deltaCount) {
                            effectiveRatio *= 0.5;
                        }

                        std::pair<RhythmIsland, int> *islandCount = nullptr;
                        for(auto &i : islandCounts) {
                            if(i.first.equals(island, deltaDifferenceEpsilon)) {
                                islandCount = &i;
                                break;
                            }
                        }

                        if(islandCount != nullptr) {
                            // only add island to island counts if they're going one after another
                            if(previousIsland.equals(island, deltaDifferenceEpsilon)) islandCount->second++;

                            // repeated island (ex: triplet -> triplet)
                            static const f64 E = 2.7182818284590451;
                            f64 power = 2.75 / (1.0 + std::pow(E, 14.0 - (0.24 * island.delta)));
                            effectiveRatio *=
                                std::min(3.0 / islandCount->second, std::pow(1.0 / islandCount->second, power));
                        } else {
                            islandCounts.emplace_back(island, 1);
                        }

                        // scale down the difficulty if the object is doubletappable
                        f64 doubletapness = prevObj->get_doubletapness(currObj, hitWindow300);
                        effectiveRatio *= 1.0 - doubletapness * 0.75;

                        rhythmComplexitySum += std::sqrt(effectiveRatio * startRatio) * currHistoricalDecay;

                        startRatio = effectiveRatio;

                        previousIsland = island;

                        // we're slowing down, stop counting
                        if(prevDelta + deltaDifferenceEpsilon < currDelta) {
                            // if we're speeding up, this stays true and  we keep counting island size.
                            firstDeltaSwitch = false;
                        }

                        island = RhythmIsland{std::max((i32)currDelta, 25), 1};
                    }
                } else if(prevDelta > currDelta + deltaDifferenceEpsilon) {  // we want to be speeding up.
                    // Begin counting island until we change speed again.
                    firstDeltaSwitch = true;

                    // bpm change is into slider, this is easy acc window
                    if(currObj->ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                        effectiveRatio *= 0.6;
                    }

                    // bpm change was from a slider, this is easier typically than circle -> circle
                    if(prevObj->ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                        effectiveRatio *= 0.6;
                    }

                    startRatio = effectiveRatio;

                    island = RhythmIsland{std::max((i32)currDelta, 25), 1};
                }

                lastObj = prevObj;
                prevObj = currObj;
            }

            this->rhythm = std::sqrt(4.0 + rhythmComplexitySum * rhythm_overall_multiplier) / 2.0;

            return this->raw_speed_strain;
        } break;

        case Skills::Skill::AIM_SLIDERS:
        case Skills::Skill::AIM_NO_SLIDERS: {
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/AimEvaluator.cs
            static const double wide_angle_multiplier = 1.5;
            static const double acute_angle_multiplier = 1.95;
            static const double slider_multiplier = 1.35;
            static const double velocity_change_multiplier = 0.75;

            const bool withSliders = (diff_type == Skills::Skill::AIM_SLIDERS);

            if(this->ho->type == OsuDifficultyHitObject::TYPE::SPINNER || this->prevObjectIndex <= 1 ||
               prev.ho->type == OsuDifficultyHitObject::TYPE::SPINNER)
                return 0.0;

            auto calcWideAngleBonus = [](double angle) {
                return std::pow(std::sin(3.0 / 4.0 * (std::min(5.0 / 6.0 * PI, std::max(PI / 6.0, angle)) - PI / 6.0)),
                                2.0);
            };
            auto calcAcuteAngleBonus = [=](double angle) { return 1.0 - calcWideAngleBonus(angle); };

            const DiffObject *prevPrev = this->get_previous(1);
            double currVelocity = this->jumpDistance / this->strain_time;

            if(prev.ho->type == OsuDifficultyHitObject::TYPE::SLIDER && withSliders) {
                double travelVelocity = prev.travelDistance / prev.travelTime;
                double movementVelocity = this->minJumpDistance / this->minJumpTime;
                currVelocity = std::max(currVelocity, movementVelocity + travelVelocity);
            }
            double aimStrain = currVelocity;

            double prevVelocity = prev.jumpDistance / prev.strain_time;
            if(prevPrev->ho->type == OsuDifficultyHitObject::TYPE::SLIDER && withSliders) {
                double travelVelocity = prevPrev->travelDistance / prevPrev->travelTime;
                double movementVelocity = prev.minJumpDistance / prev.minJumpTime;
                prevVelocity = std::max(prevVelocity, movementVelocity + travelVelocity);
            }

            double wideAngleBonus = 0;
            double acuteAngleBonus = 0;
            double sliderBonus = 0;
            double velocityChangeBonus = 0;

            if(std::max(this->strain_time, prev.strain_time) < 1.25 * std::min(this->strain_time, prev.strain_time)) {
                if(!std::isnan(this->angle) && !std::isnan(prev.angle) && !std::isnan(prevPrev->angle)) {
                    double angleBonus = std::min(currVelocity, prevVelocity);

                    wideAngleBonus = calcWideAngleBonus(this->angle);
                    acuteAngleBonus =
                        this->strain_time > 100
                            ? 0.0
                            : (calcAcuteAngleBonus(this->angle) * calcAcuteAngleBonus(prev.angle) *
                               std::min(angleBonus, 125.0 / this->strain_time) *
                               std::pow(std::sin(PI / 2.0 * std::min(1.0, (100.0 - this->strain_time) / 25.0)), 2.0) *
                               std::pow(std::sin(PI / 2.0 *
                                                 (std::clamp<double>(this->jumpDistance, 50.0, 100.0) - 50.0) / 50.0),
                                        2.0));

                    wideAngleBonus *=
                        angleBonus * (1.0 - std::min(wideAngleBonus, std::pow(calcWideAngleBonus(prev.angle), 3.0)));
                    acuteAngleBonus *=
                        0.5 +
                        0.5 * (1.0 - std::min(acuteAngleBonus, std::pow(calcAcuteAngleBonus(prevPrev->angle), 3.0)));
                }
            }

            if(std::max(prevVelocity, currVelocity) != 0.0) {
                prevVelocity = (prev.jumpDistance + prevPrev->travelDistance) / prev.strain_time;
                currVelocity = (this->jumpDistance + prev.travelDistance) / this->strain_time;

                double distRatio = std::pow(
                    std::sin(PI / 2.0 * std::abs(prevVelocity - currVelocity) / std::max(prevVelocity, currVelocity)),
                    2.0);
                double overlapVelocityBuff = std::min(125.0 / std::min(this->strain_time, prev.strain_time),
                                                      std::abs(prevVelocity - currVelocity));
                velocityChangeBonus = overlapVelocityBuff * distRatio *
                                      std::pow(std::min(this->strain_time, prev.strain_time) /
                                                   std::max(this->strain_time, prev.strain_time),
                                               2.0);
            }

            if(prev.ho->type == OsuDifficultyHitObject::TYPE::SLIDER)
                sliderBonus = prev.travelDistance / prev.travelTime;

            aimStrain +=
                std::max(acuteAngleBonus * acute_angle_multiplier,
                         wideAngleBonus * wide_angle_multiplier + velocityChangeBonus * velocity_change_multiplier);
            if(withSliders) aimStrain += sliderBonus * slider_multiplier;

            return aimStrain;
        } break;
    }

    return 0.0;
}

f64 DifficultyCalculator::DiffObject::get_doubletapness(const DifficultyCalculator::DiffObject *next,
                                                        f64 hitWindow300) const {
    if(next != nullptr) {
        f64 cur_delta = std::max(1.0, this->delta_time);
        f64 next_delta = std::max(1, next->ho->time - this->ho->time);  // next delta time isn't initialized yet
        f64 delta_diff = std::abs(next_delta - cur_delta);
        f64 speedRatio = cur_delta / std::max(cur_delta, delta_diff);
        f64 windowRatio = pow(std::min(1.0, cur_delta / hitWindow300), 2.0);

        return 1.0 - pow(speedRatio, 1.0 - windowRatio);
    }
    return 0.0;
}