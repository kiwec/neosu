// Copyright (c) 2019, PG & Francesco149, All rights reserved.
#include "DifficultyCalculator.h"

#include <algorithm>

#include "BeatmapInterface.h"
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
            // old: too much kept memory allocations for over 14000 sliders in https://osu.ppy.sh/beatmapsets/592138#osu/1277649

            // NOTE: this is still used for beatmaps with less than 5000 sliders (because precomputing all curves is way faster, especially for star cache loader)
            // NOTE: the 5000 slider threshold was chosen by looking at the longest non-aspire marathon maps
            // NOTE: 15000 slider curves use ~1.6 GB of RAM, for 32-bit executables with 2GB cap limiting to 5000 sliders gives ~530 MB which should be survivable without crashing (even though the heap gets fragmented to fuck)

            // 6000 sliders @ The Weather Channel - 139 Degrees (Tiggz Mumbson) [Special Weather Statement].osu
            // 3674 sliders @ Various Artists - Alternator Compilation (Monstrata) [Marathon].osu
            // 4599 sliders @ Renard - Because Maybe! (Mismagius) [- Nogard Marathon -].osu
            // 4921 sliders @ Renard - Because Maybe! (Mismagius) [- Nogard Marathon v2 -].osu
            // 14960 sliders @ pishifat - H E L L O  T H E R E (Kondou-Shinichi) [Sliders in the 69th centries].osu
            // 5208 sliders @ MillhioreF - haitai but every hai adds another haitai in the background (Chewy-san) [Weriko Rank the dream (nerf) but loli].osu

            this->curve = SliderCurve::createCurve(this->osuSliderCurveType, controlPoints, this->pixelLength,
                                                   cv::stars_slider_curve_points_separation.getFloat());
        } else {
            // new: delay curve creation to when it's needed, and also immediately delete afterwards (at the cost of having to store a copy of the control points)
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
    scheduledCurveAllocStackOffset = stackOffset;

    pos = originalPos - vec2(stack * stackOffset, stack * stackOffset);

    updateCurveStackPosition(stackOffset);
}

void OsuDifficultyHitObject::updateCurveStackPosition(f32 stackOffset) {
    if(curve != nullptr) curve->updateStackPosition(stack * stackOffset, false);
}

vec2 OsuDifficultyHitObject::getOriginalRawPosAt(i32 pos) const {
    // NOTE: the delayed curve creation has been deliberately disabled here for stacking purposes for beatmaps with insane slider counts for performance reasons
    // NOTE: this means that these aspire maps will have incorrect stars due to incorrect slider stacking, but the delta is below 0.02 even for the most insane maps which currently exist
    // NOTE: if this were to ever get implemented properly, then a sliding window for curve allocation must be added to the stack algorithm itself (as doing it in here is O(n!) madness)
    // NOTE: to validate the delta, use Acid Rain [Aspire] - Karoo13 (6.76* with slider stacks -> 6.75* without slider stacks)

    if(type != TYPE::SLIDER || (curve == nullptr /* && !scheduledCurveAlloc*/))
        return originalPos;
    else {
        // new (correct)
        if(pos <= time)
            return curve->originalPointAt(0.0f);
        else if(pos >= endTime) {
            if(repeats % 2 == 0)
                return curve->originalPointAt(0.0f);
            else
                return curve->originalPointAt(1.0f);
        } else
            return curve->originalPointAt(getT(pos, false));
    }
}

f32 OsuDifficultyHitObject::getT(i32 pos, bool raw) const {
    f32 t = (f32)((i32)pos - (i32)time) / spanDuration;
    if(raw)
        return t;
    else {
        f32 floorVal = (f32)std::floor(t);
        return ((i32)floorVal % 2 == 0) ? t - floorVal : floorVal + 1 - t;
    }
}

f64 DifficultyCalculator::calculateStarDiffForHitObjects(StarCalcParams &params) {
    // NOTE: depends on speed multiplier + CS + OD + relax + touchDevice

    // NOTE: upToObjectIndex is applied way below, during the construction of the 'dobjects'

    // NOTE: osu always returns 0 stars for beatmaps with only 1 object, except if that object is a slider
    if(params.sortedHitObjects.size() < 2) {
        if(params.sortedHitObjects.size() < 1) return 0.0;
        if(params.sortedHitObjects[0].type != OsuDifficultyHitObject::TYPE::SLIDER) return 0.0;
    }

    // global independent variables/constants
    // NOTE: clamped CS because McOsu allows CS > ~12.1429 (at which point the diameter becomes negative)
    f32 circleRadiusInOsuPixels = 64.0f * GameRules::getRawHitCircleScale(std::clamp<f32>(params.CS, 0.0f, 12.142f));
    const f32 hitWindow300 = 2.0f * GameRules::odTo300HitWindowMS(params.OD) / params.speedMultiplier;

    // ****************************************************************************************************************************************** //

    // see setDistances() @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

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

    // ****************************************************************************************************************************************** //

    // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

    class DistanceCalc {
       public:
        static void computeSliderCursorPosition(DiffObject &slider, f32 circleRadius) {
            if(slider.lazyCalcFinished || slider.ho->curve == nullptr) return;

            // NOTE: lazer won't load sliders above a certain length, but mcosu will
            // this isn't entirely accurate to how lazer does it (as that skips loading the object entirely),
            // but this is a good middle ground for maps that aren't completely aspire and still have relatively normal star counts on lazer
            // see: DJ Noriken - Stargazer feat. YUC'e (PSYQUI Remix) (Hishiro Chizuru) [Starg-Azer isn't so great? Are you kidding me?]
            if(cv::stars_ignore_clamped_sliders.getBool()) {
                if(slider.ho->curve->getPixelLength() >= cv::slider_curve_max_length.getFloat()) return;
            }

            // NOTE: although this looks like a duplicate of the end tick time, this really does have a noticeable impact on some maps due to precision issues
            // see: Ocelot - KAEDE (Hollow Wings) [EX EX]
            const f64 tailLeniency = (f64)cv::slider_end_inside_check_offset.getInt();
            const f64 totalDuration = (f64)slider.ho->spanDuration * slider.ho->repeats;
            f64 trackingEndTime = (f64)slider.ho->time + std::max(totalDuration - tailLeniency, totalDuration / 2.0);

            // NOTE: lazer has logic to reorder the last slider tick if it happens after trackingEndTime here, which already happens in mcosu

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
                vec2 diff;

                if(slider.ho->scoringTimes[i].type == OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::END) {
                    // NOTE: In lazer, the position of the slider end is at the visual end, but the time is at the scoring end
                    diff = slider.ho->curve->pointAt(slider.ho->repeats % 2 ? 1.0 : 0.0) - cursor_pos;
                } else {
                    f64 progress = (std::clamp<f32>(slider.ho->scoringTimes[i].time - (f32)slider.ho->time, 0.0f,
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

    // ****************************************************************************************************************************************** //

    // initialize dobjects
    const size_t numDiffObjects =
        (params.upToObjectIndex < 0) ? params.sortedHitObjects.size() : params.upToObjectIndex + 1;
    const bool isUsingCachedDiffObjects = (params.cachedDiffObjects.size() > 0);
    DiffObject *diffObjects;
    if(!isUsingCachedDiffObjects) {
        // not cached (full rebuild computation)
        params.cachedDiffObjects.reserve(numDiffObjects);
        for(size_t i = 0; i < numDiffObjects; i++) {
            if(params.dead.load(std::memory_order_acquire)) return 0.0;

            params.cachedDiffObjects.emplace_back(&params.sortedHitObjects[i], radius_scaling_factor,
                                                  params.cachedDiffObjects,
                                                  (i32)i - 1);  // this already initializes the angle to NaN
        }
    }
    diffObjects = params.cachedDiffObjects.data();

    // calculate angles and travel/jump distances (before calculating strains)
    if(!isUsingCachedDiffObjects) {
        const f32 starsSliderCurvePointsSeparation = cv::stars_slider_curve_points_separation.getFloat();
        for(size_t i = 0; i < numDiffObjects; i++) {
            if(params.dead.load(std::memory_order_acquire)) return 0.0;

            // see setDistances() @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

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
                    cur.travelDistance = cur.lazyTravelDist * std::pow(1.0 + (cur.ho->repeats - 1) / 2.5, 1.0 / 2.5);
                    cur.travelTime = std::max(cur.lazyTravelTime, 25.0);
                }

                // don't need to jump to reach spinners
                if(cur.ho->type == OsuDifficultyHitObject::TYPE::SPINNER ||
                   prev1.ho->type == OsuDifficultyHitObject::TYPE::SPINNER)
                    continue;

                const vec2 lastCursorPosition = DistanceCalc::getEndCursorPosition(prev1, circleRadiusInOsuPixels);

                f64 cur_strain_time =
                    (f64)std::max(cur.ho->time - prev1.ho->time, 25);  // strain_time isn't initialized here
                cur.jumpDistance = vec::length(cur.norm_start - lastCursorPosition * radius_scaling_factor);
                cur.minJumpDistance = cur.jumpDistance;
                cur.minJumpTime = cur_strain_time;

                if(prev1.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                    f64 last_travel = std::max(prev1.lazyTravelTime, 25.0);
                    cur.minJumpTime = std::max(cur_strain_time - last_travel, 25.0);

                    // NOTE: "curve shouldn't be null here, but Yin [test7] causes that to happen"
                    // NOTE: the curve can be null if controlPoints.size() < 1 because the OsuDifficultyHitObject() constructor will then not set scheduledCurveAlloc to true (which is perfectly fine and correct)
                    f32 tail_jump_dist =
                        vec::distance(prev1.ho->curve ? prev1.ho->curve->pointAt(prev1.ho->repeats % 2 ? 1.0 : 0.0)
                                                      : prev1.ho->pos,
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
                        // NOTE: this trivial sliding window implementation will keep the last 2 curves alive at the end, but they get auto deleted later anyway so w/e
                        if(i > 2) {
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
    if(!isUsingCachedDiffObjects)  // NOTE: yes, this loses some extremely minor accuracy (~0.001 stars territory) for live star/pp for some rare individual upToObjectIndex due to not being recomputed for the cut set of cached diffObjects every time, but the performance gain is so insane I don't care
    {
        for(size_t i = 1; i < numDiffObjects; i++)  // NOTE: start at 1
        {
            diffObjects[i].calculate_strains(diffObjects[i - 1],
                                             (i == numDiffObjects - 1) ? nullptr : &diffObjects[i + 1], hitWindow300);
        }
    }

    // calculate final difficulty (weigh strains)
    f64 aimNoSliders =
        DiffObject::calculate_difficulty(Skills::AIM_NO_SLIDERS, diffObjects, numDiffObjects,
                                         params.incremental ? &params.incremental[Skills::AIM_NO_SLIDERS] : nullptr);
    *params.aim =
        DiffObject::calculate_difficulty(Skills::AIM_SLIDERS, diffObjects, numDiffObjects,
                                         params.incremental ? &params.incremental[Skills::AIM_SLIDERS] : nullptr,
                                         params.outAimStrains, params.difficultAimStrains, params.aimDifficultSliders);
    *params.speed = DiffObject::calculate_difficulty(
        Skills::SPEED, diffObjects, numDiffObjects, params.incremental ? &params.incremental[Skills::SPEED] : nullptr,
        params.outSpeedStrains, params.difficultSpeedStrains, params.speedNotes);

    static const f64 star_scaling_factor = 0.0675;

    aimNoSliders = std::sqrt(aimNoSliders) * star_scaling_factor;
    *params.aim = std::sqrt(*params.aim) * star_scaling_factor;
    *params.speed = std::sqrt(*params.speed) * star_scaling_factor;

    *params.aimSliderFactor = (*params.aim > 0) ? aimNoSliders / *params.aim : 1.0;

    if(params.touchDevice) *params.aim = std::pow(*params.aim, 0.8);

    if(params.relax) {
        *params.aim *= 0.9;
        *params.speed = 0.0;
    }

    return calculateTotalStarsFromSkills(*params.aim, *params.speed);
}

f64 DifficultyCalculator::calculatePPv2(PPv2CalcParams cpar) {
    // NOTE: depends on active mods + OD + AR

    // apply "timescale" aka speed multiplier to ar/od
    // (the original incoming ar/od values are guaranteed to not yet have any speed multiplier applied to them, but they do have non-time-related mods already applied, like HR or any custom overrides)
    // (yes, this does work correctly when the override slider "locking" feature is used. in this case, the stored ar/od is already compensated such that it will have the locked value AFTER applying the speed multiplier here)
    // (all UI elements which display ar/od from stored scores, like the ranking screen or score buttons, also do this calculation before displaying the values to the user. of course the mod selection screen does too.)
    cpar.ar = GameRules::arWithSpeed(cpar.ar, cpar.mods.speed);
    cpar.od = GameRules::odWithSpeed(cpar.od, cpar.mods.speed);

    if(cpar.c300 < 0) cpar.c300 = cpar.numHitObjects - cpar.c100 - cpar.c50 - cpar.misses;

    if(cpar.combo < 0) cpar.combo = cpar.maxPossibleCombo;

    if(cpar.combo < 1) return 0.0;

    const i32 totalHits = cpar.c300 + cpar.c100 + cpar.c50 + cpar.misses;

    ScoreData score{
        .mods = cpar.mods,
        .accuracy =
            (totalHits > 0 ? (f64)(cpar.c300 * 300 + cpar.c100 * 100 + cpar.c50 * 50) / (f64)(totalHits * 300) : 0.0),
        .countGreat = cpar.c300,
        .countGood = cpar.c100,
        .countMeh = cpar.c50,
        .countMiss = cpar.misses,
        .totalHits = totalHits,
        .totalSuccessfulHits = cpar.c300 + cpar.c100 + cpar.c50,
        .beatmapMaxCombo = cpar.maxPossibleCombo,
        .scoreMaxCombo = cpar.combo,
        .amountHitObjectsWithAccuracy =
            (cpar.mods.has(ModFlags::ScoreV2) ? cpar.numCircles + cpar.numSliders : cpar.numCircles),
    };

    Attributes attributes{.AimStrain = cpar.aim,
                          .SliderFactor = cpar.aimSliderFactor,
                          .AimDifficultSliderCount = cpar.aimDifficultSliders,
                          .AimDifficultStrainCount = cpar.aimDifficultStrains,
                          .SpeedStrain = cpar.speed,
                          .SpeedNoteCount = cpar.speedNotes,
                          .SpeedDifficultStrainCount = cpar.speedDifficultStrains,
                          .ApproachRate = cpar.ar,
                          .OverallDifficulty = cpar.od,
                          .SliderCount = cpar.numSliders};

    // calculateEffectiveMissCount @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/OsuPerformanceCalculator.cs
    // required because slider breaks aren't exposed to pp calculation
    f64 comboBasedMissCount = 0.0;
    if(cpar.numSliders > 0) {
        f64 fullComboThreshold = cpar.maxPossibleCombo - (0.1 * cpar.numSliders);
        if((f64)cpar.combo < fullComboThreshold)
            comboBasedMissCount = fullComboThreshold / std::max(1.0, (f64)cpar.combo);
    }
    f64 effectiveMissCount =
        std::clamp<f64>(comboBasedMissCount, (f64)cpar.misses, (f64)(cpar.c50 + cpar.c100 + cpar.misses));

    // custom multipliers for nofail and spunout
    f64 multiplier = 1.15;  // keep final pp normalized across changes
    {
        if(cpar.mods.has(ModFlags::NoFail))
            multiplier *= std::max(
                0.9, 1.0 - 0.02 * effectiveMissCount);  // see https://github.com/ppy/osu-performance/pull/127/files

        if((cpar.mods.has(ModFlags::SpunOut)) && score.totalHits > 0)
            multiplier *= 1.0 - std::pow((f64)cpar.numSpinners / (f64)score.totalHits,
                                         0.85);  // see https://github.com/ppy/osu-performance/pull/110/

        if(cpar.mods.has(ModFlags::Relax)) {
            f64 okMultiplier = std::max(0.0, cpar.od > 0.0 ? 1.0 - std::pow(cpar.od / 13.33, 1.8) : 1.0);   // 100
            f64 mehMultiplier = std::max(0.0, cpar.od > 0.0 ? 1.0 - std::pow(cpar.od / 13.33, 5.0) : 1.0);  // 50
            effectiveMissCount = std::min(effectiveMissCount + cpar.c100 * okMultiplier + cpar.c50 * mehMultiplier,
                                          (f64)score.totalHits);
        }
    }

    const f64 speedDeviation = calculateSpeedDeviation(score, attributes);
    const f64 aimValue = computeAimValue(score, attributes, effectiveMissCount);
    const f64 speedValue = computeSpeedValue(score, attributes, effectiveMissCount, speedDeviation);
    const f64 accuracyValue = computeAccuracyValue(score, attributes);

    const f64 totalValue =
        std::pow(std::pow(aimValue, 1.1) + std::pow(speedValue, 1.1) + std::pow(accuracyValue, 1.1), 1.0 / 1.1) *
        multiplier;

    return totalValue;
}

f64 DifficultyCalculator::calculateTotalStarsFromSkills(f64 aim, f64 speed) {
    f64 baseAimPerformance = std::pow(5.0 * std::max(1.0, aim / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 baseSpeedPerformance = std::pow(5.0 * std::max(1.0, speed / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 basePerformance = std::pow(std::pow(baseAimPerformance, 1.1) + std::pow(baseSpeedPerformance, 1.1), 1.0 / 1.1);
    return basePerformance > 0.00001
               ? 1.0476895531716472 /* Math.Cbrt(OsuPerformanceCalculator.PERFORMANCE_BASE_MULTIPLIER) */ * 0.027 *
                     (std::cbrt(100000.0 / std::pow(2.0, 1 / 1.1) * basePerformance) + 4.0)
               : 0.0;
}

// https://github.com/ppy/osu-performance/blob/master/src/performance/osu/OsuScore.cpp
// https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/OsuPerformanceCalculator.cs

f64 DifficultyCalculator::computeAimValue(const ScoreData &score, const DifficultyCalculator::Attributes &attributes,
                                          f64 effectiveMissCount) {
    if(score.mods.has(ModFlags::Autopilot)) return 0.0;

    f64 aimDifficulty = attributes.AimStrain;

    // McOsu doesn't track dropped slider ends, so the ScoreV2/lazer case can't be handled here
    if(attributes.SliderCount > 0 && attributes.AimDifficultSliderCount > 0) {
        i32 maximumPossibleDroppedSliders = score.countGood + score.countMeh + score.countMiss;
        f64 estimateImproperlyFollowedDifficultSliders =
            std::clamp<f64>((f64)std::min(maximumPossibleDroppedSliders, score.beatmapMaxCombo - score.scoreMaxCombo),
                            0.0, attributes.AimDifficultSliderCount);
        f64 sliderNerfFactor =
            (1.0 - attributes.SliderFactor) *
                std::pow(1.0 - estimateImproperlyFollowedDifficultSliders / attributes.AimDifficultSliderCount, 3.0) +
            attributes.SliderFactor;
        aimDifficulty *= sliderNerfFactor;
    }

    f64 aimValue = std::pow(5.0 * std::max(1.0, aimDifficulty / 0.0675) - 4.0, 3.0) / 100000.0;

    // length bonus
    f64 lengthBonus = 0.95 + 0.4 * std::min(1.0, ((f64)score.totalHits / 2000.0)) +
                      (score.totalHits > 2000 ? std::log10(((f64)score.totalHits / 2000.0)) * 0.5 : 0.0);
    aimValue *= lengthBonus;

    // miss penalty
    // see https://github.com/ppy/osu/pull/16280/
    if(effectiveMissCount > 0 && score.totalHits > 0)
        aimValue *=
            0.96 / ((effectiveMissCount / (4.0 * std::pow(std::log(attributes.AimDifficultStrainCount), 0.94))) + 1.0);

    // ar bonus
    f64 approachRateFactor = 0.0;  // see https://github.com/ppy/osu-performance/pull/125/
    if(!score.mods.has(ModFlags::Relax)) {
        if(attributes.ApproachRate > 10.33)
            approachRateFactor =
                0.3 *
                (attributes.ApproachRate -
                 10.33);  // from 0.3 to 0.4 see https://github.com/ppy/osu-performance/pull/125/ // and completely changed the logic in https://github.com/ppy/osu-performance/pull/135/
        else if(attributes.ApproachRate < 8.0)
            approachRateFactor =
                0.05 *
                (8.0 -
                 attributes
                     .ApproachRate);  // from 0.01 to 0.1 see https://github.com/ppy/osu-performance/pull/125/ // and back again from 0.1 to 0.01 see https://github.com/ppy/osu-performance/pull/133/ // and completely changed the logic in https://github.com/ppy/osu-performance/pull/135/
    }

    aimValue *= 1.0 + approachRateFactor * lengthBonus;

    // hidden
    if(score.mods.has(ModFlags::Hidden))
        aimValue *= 1.0 + 0.04 * (std::max(12.0 - attributes.ApproachRate,
                                           0.0));  // NOTE: clamped to 0 because McOsu allows AR > 12

    // scale aim with acc
    aimValue *= score.accuracy;
    // also consider acc difficulty when doing that
    aimValue *= 0.98 + std::pow(std::max(0.0, attributes.OverallDifficulty), 2.0) / 2500.0;

    return aimValue;
}

f64 DifficultyCalculator::computeSpeedValue(const ScoreData &score, const Attributes &attributes,
                                            f64 effectiveMissCount, f64 speedDeviation) {
    if((score.mods.has(ModFlags::Relax)) || std::isnan(speedDeviation)) return 0.0;

    f64 speedValue = std::pow(5.0 * std::max(1.0, attributes.SpeedStrain / 0.0675) - 4.0, 3.0) / 100000.0;

    // length bonus
    f64 lengthBonus = 0.95 + 0.4 * std::min(1.0, ((f64)score.totalHits / 2000.0)) +
                      (score.totalHits > 2000 ? std::log10(((f64)score.totalHits / 2000.0)) * 0.5 : 0.0);
    speedValue *= lengthBonus;

    // miss penalty
    // see https://github.com/ppy/osu/pull/16280/
    if(effectiveMissCount > 0)
        speedValue *=
            0.96 /
            ((effectiveMissCount / (4.0 * std::pow(std::log(attributes.SpeedDifficultStrainCount), 0.94))) + 1.0);

    // ar bonus
    f64 approachRateFactor = 0.0;  // see https://github.com/ppy/osu-performance/pull/125/
    if(attributes.ApproachRate > 10.33)
        approachRateFactor =
            0.3 *
            (attributes.ApproachRate -
             10.33);  // from 0.3 to 0.4 see https://github.com/ppy/osu-performance/pull/125/ // and completely changed the logic in https://github.com/ppy/osu-performance/pull/135/

    speedValue *= 1.0 + approachRateFactor * lengthBonus;

    // hidden
    if(score.mods.has(ModFlags::Hidden))
        speedValue *= 1.0 + 0.04 * (std::max(12.0 - attributes.ApproachRate,
                                             0.0));  // NOTE: clamped to 0 because McOsu allows AR > 12

    f64 speedHighDeviationMultiplier = calculateSpeedHighDeviationNerf(attributes, speedDeviation);
    speedValue *= speedHighDeviationMultiplier;

    // "Calculate accuracy assuming the worst case scenario"
    f64 relevantTotalDiff = std::max(0.0, score.totalHits - attributes.SpeedNoteCount);
    f64 relevantCountGreat = std::max(0.0, score.countGreat - relevantTotalDiff);
    f64 relevantCountOk = std::max(0.0, score.countGood - std::max(0.0, relevantTotalDiff - score.countGreat));
    f64 relevantCountMeh =
        std::max(0.0, score.countMeh - std::max(0.0, relevantTotalDiff - score.countGreat - score.countGood));
    f64 relevantAccuracy =
        attributes.SpeedNoteCount == 0
            ? 0
            : (relevantCountGreat * 6.0 + relevantCountOk * 2.0 + relevantCountMeh) / (attributes.SpeedNoteCount * 6.0);

    // see https://github.com/ppy/osu-performance/pull/128/
    // Scale the speed value with accuracy and OD
    speedValue *= (0.95 + std::pow(std::max(0.0, attributes.OverallDifficulty), 2.0) / 750.0) *
                  std::pow((score.accuracy + relevantAccuracy) / 2.0, (14.5 - attributes.OverallDifficulty) / 2.0);

    // singletap buff (XXX: might be too weak)
    if(score.mods.has(ModFlags::Singletap)) {
        speedValue *= 1.25;
    }

    // no keylock nerf (XXX: might be too harsh)
    if(score.mods.has(ModFlags::NoKeylock)) {
        speedValue *= 0.5;
    }

    return speedValue;
}

f64 DifficultyCalculator::computeAccuracyValue(const ScoreData &score, const Attributes &attributes) {
    if(score.mods.has(ModFlags::Relax)) return 0.0;

    f64 betterAccuracyPercentage;
    if(score.amountHitObjectsWithAccuracy > 0)
        betterAccuracyPercentage =
            ((f64)(score.countGreat - std::max(score.totalHits - score.amountHitObjectsWithAccuracy, 0)) * 6.0 +
             (score.countGood * 2.0) + score.countMeh) /
            (f64)(score.amountHitObjectsWithAccuracy * 6.0);
    else
        betterAccuracyPercentage = 0.0;

    // it's possible to reach negative accuracy, cap at zero
    if(betterAccuracyPercentage < 0.0) betterAccuracyPercentage = 0.0;

    // arbitrary values tom crafted out of trial and error
    f64 accuracyValue =
        std::pow(1.52163, attributes.OverallDifficulty) * std::pow(betterAccuracyPercentage, 24.0) * 2.83;

    // length bonus
    accuracyValue *= std::min(1.15, std::pow(score.amountHitObjectsWithAccuracy / 1000.0, 0.3));

    // hidden bonus
    if(score.mods.has(ModFlags::Hidden)) accuracyValue *= 1.08;
    // flashlight bonus
    if(score.mods.has(ModFlags::Flashlight)) accuracyValue *= 1.02;

    return accuracyValue;
}

f64 DifficultyCalculator::calculateSpeedDeviation(const ScoreData &score, const Attributes &attributes) {
    if(score.countGreat + score.countGood + score.countMeh == 0) return std::numeric_limits<f64>::quiet_NaN();

    f64 speedNoteCount = attributes.SpeedNoteCount;
    speedNoteCount += (score.totalHits - attributes.SpeedNoteCount) * 0.1;

    f64 relevantCountMiss = std::min((f64)score.countMiss, speedNoteCount);
    f64 relevantCountMeh = std::min((f64)score.countMeh, speedNoteCount - relevantCountMiss);
    f64 relevantCountOk = std::min((f64)score.countGood, speedNoteCount - relevantCountMiss - relevantCountMeh);
    f64 relevantCountGreat = std::max(0.0, speedNoteCount - relevantCountMiss - relevantCountMeh - relevantCountOk);

    return calculateDeviation(attributes, score.mods.speed, relevantCountGreat, relevantCountOk, relevantCountMeh,
                              relevantCountMiss);
}

f64 DifficultyCalculator::calculateDeviation(const Attributes &attributes, f64 timescale, f64 relevantCountGreat,
                                             f64 relevantCountOk, f64 relevantCountMeh, f64 relevantCountMiss) {
    if(relevantCountGreat + relevantCountOk + relevantCountMeh <= 0.0) return std::numeric_limits<f64>::quiet_NaN();

    const f64 greatHitWindow = GameRules::odTo300HitWindowMS(attributes.OverallDifficulty) / timescale;
    const f64 okHitWindow = GameRules::odTo100HitWindowMS(attributes.OverallDifficulty) / timescale;
    const f64 mehHitWindow = GameRules::odTo50HitWindowMS(attributes.OverallDifficulty) / timescale;

    const f64 z = 2.32634787404;
    const f64 sqrt2 = 1.4142135623730951;
    const f64 sqrt3 = 1.7320508075688772;
    const f64 sqrt2OverPi = 0.7978845608028654;

    f64 objectCount = relevantCountGreat + relevantCountOk + relevantCountMeh + relevantCountMiss;
    f64 n = std::max(1.0, objectCount - relevantCountMiss - relevantCountMeh);
    f64 p = relevantCountGreat / n;
    f64 pLowerBound = (n * p + z * z / 2.0) / (n + z * z) - z / (n + z * z) * sqrt(n * p * (1.0 - p) + z * z / 4.0);
    f64 deviation = greatHitWindow / (sqrt2 * erfInv(pLowerBound));
    f64 randomValue = sqrt2OverPi * okHitWindow * std::exp(-0.5 * std::pow(okHitWindow / deviation, 2.0)) /
                      (deviation * erf(okHitWindow / (sqrt2 * deviation)));
    deviation *= std::sqrt(1.0 - randomValue);

    f64 limitValue = okHitWindow / sqrt3;
    if(pLowerBound == 0.0 || randomValue >= 1.0 || deviation > limitValue) deviation = limitValue;

    f64 mehVariance = (mehHitWindow * mehHitWindow + okHitWindow * mehHitWindow + okHitWindow * okHitWindow) / 3.0;
    return std::sqrt(
        ((relevantCountGreat + relevantCountOk) * std::pow(deviation, 2.0) + relevantCountMeh * mehVariance) /
        (relevantCountGreat + relevantCountOk + relevantCountMeh));
}

f64 DifficultyCalculator::calculateSpeedHighDeviationNerf(const Attributes &attributes, f64 speedDeviation) {
    if(std::isnan(speedDeviation)) return 0.0;

    f64 speedValue = std::pow(5.0 * std::max(1.0, attributes.SpeedStrain / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 excessSpeedDifficultyCutoff = 100.0 + 220.0 * std::pow(22.0 / speedDeviation, 6.5);
    if(speedValue <= excessSpeedDifficultyCutoff) return 1.0;

    const f64 scale = 50.0;
    f64 adjustedSpeedValue = scale * (std::log((speedValue - excessSpeedDifficultyCutoff) / scale + 1.0) +
                                      excessSpeedDifficultyCutoff / scale);
    f64 lerpVal = 1.0 - std::clamp<f64>((speedDeviation - 22.0) / (27.0 - 22.0), 0.0, 1.0);
    adjustedSpeedValue = std::lerp(adjustedSpeedValue, speedValue, lerpVal);

    return adjustedSpeedValue / speedValue;
}

f64 DifficultyCalculator::erf(f64 x) {
    switch(std::fpclassify(x)) {
        case FP_INFINITE:
            return (x > 0) ? 1.0 : -1.0;
        case FP_NAN:
            return std::numeric_limits<f64>::quiet_NaN();
        case FP_ZERO:
            return 0.0;
        default:
            return erfImp(x, false);
    }
}

f64 DifficultyCalculator::erfInv(f64 z) {
    if(z == 0.0)
        return 0.0;
    else if(z >= 1.0)
        return std::numeric_limits<f64>::infinity();
    else if(z <= -1.0)
        return -std::numeric_limits<f64>::infinity();

    if(z < 0.0)
        return erfInvImp(-z, 1.0 + z, -1.0);
    else
        return erfInvImp(z, 1.0 - z, 1.0);
}

f64 DifficultyCalculator::erfImp(f64 z, bool invert) {
    if(z < 0.0) {
        if(!invert) return -erfImp(-z, false);

        if(z < -0.5) return 2 - erfImp(-z, true);

        return 1.0 + erfImp(-z, false);
    }

    f64 result;
    if(z < 0.5) {
        static constexpr f64 erf_imp_an[] = {0.00337916709551257388990745,  -0.00073695653048167948530905,
                                             -0.374732337392919607868241,   0.0817442448733587196071743,
                                             -0.0421089319936548595203468,  0.0070165709512095756344528,
                                             -0.00495091255982435110337458, 0.000871646599037922480317225};
        static constexpr f64 erf_imp_ad[] = {1,
                                             -0.218088218087924645390535,
                                             0.412542972725442099083918,
                                             -0.0841891147873106755410271,
                                             0.0655338856400241519690695,
                                             -0.0120019604454941768171266,
                                             0.00408165558926174048329689,
                                             -0.000615900721557769691924509};

        if(z < 1e-10)
            result = (z * 1.125) + (z * 0.003379167095512573896158903121545171688);
        else
            result = (z * 1.125) + (z * evaluatePolynomial(z, erf_imp_an) / evaluatePolynomial(z, erf_imp_ad));
    } else if(z < 110) {
        invert = !invert;
        f64 r, b;

        if(z < 0.75) {
            static constexpr f64 erf_imp_bn[] = {-0.0361790390718262471360258, 0.292251883444882683221149,
                                                 0.281447041797604512774415,   0.125610208862766947294894,
                                                 0.0274135028268930549240776,  0.00250839672168065762786937};
            static constexpr f64 erf_imp_bd[] = {1,
                                                 1.8545005897903486499845,
                                                 1.43575803037831418074962,
                                                 0.582827658753036572454135,
                                                 0.124810476932949746447682,
                                                 0.0113724176546353285778481};
            r = evaluatePolynomial(z - 0.5, erf_imp_bn) / evaluatePolynomial(z - 0.5, erf_imp_bd);

            // NOTE: despite being assigned to a double, all of these are single-precision f32 literals in the original code
            b = 0.3440242112f;
        } else if(z < 1.25) {
            static constexpr f64 erf_imp_cn[] = {-0.0397876892611136856954425, 0.153165212467878293257683,
                                                 0.191260295600936245503129,   0.10276327061989304213645,
                                                 0.029637090615738836726027,   0.0046093486780275489468812,
                                                 0.000307607820348680180548455};
            static constexpr f64 erf_imp_cd[] = {1,
                                                 1.95520072987627704987886,
                                                 1.64762317199384860109595,
                                                 0.768238607022126250082483,
                                                 0.209793185936509782784315,
                                                 0.0319569316899913392596356,
                                                 0.00213363160895785378615014};
            r = evaluatePolynomial(z - 0.75, erf_imp_cn) / evaluatePolynomial(z - 0.75, erf_imp_cd);
            b = 0.419990927f;
        } else if(z < 2.25) {
            static constexpr f64 erf_imp_dn[] = {-0.0300838560557949717328341, 0.0538578829844454508530552,
                                                 0.0726211541651914182692959,  0.0367628469888049348429018,
                                                 0.00964629015572527529605267, 0.00133453480075291076745275,
                                                 0.778087599782504251917881e-4};
            static constexpr f64 erf_imp_dd[] = {1,
                                                 1.75967098147167528287343,
                                                 1.32883571437961120556307,
                                                 0.552528596508757581287907,
                                                 0.133793056941332861912279,
                                                 0.0179509645176280768640766,
                                                 0.00104712440019937356634038,
                                                 -0.106640381820357337177643e-7};
            r = evaluatePolynomial(z - 1.25, erf_imp_dn) / evaluatePolynomial(z - 1.25, erf_imp_dd);
            b = 0.4898625016f;
        } else if(z < 3.5) {
            static constexpr f64 erf_imp_en[] = {-0.0117907570137227847827732, 0.014262132090538809896674,
                                                 0.0202234435902960820020765,  0.00930668299990432009042239,
                                                 0.00213357802422065994322516, 0.00025022987386460102395382,
                                                 0.120534912219588189822126e-4};
            static constexpr f64 erf_imp_ed[] = {1,
                                                 1.50376225203620482047419,
                                                 0.965397786204462896346934,
                                                 0.339265230476796681555511,
                                                 0.0689740649541569716897427,
                                                 0.00771060262491768307365526,
                                                 0.000371421101531069302990367};
            r = evaluatePolynomial(z - 2.25, erf_imp_en) / evaluatePolynomial(z - 2.25, erf_imp_ed);
            b = 0.5317370892f;
        } else if(z < 5.25) {
            static constexpr f64 erf_imp_fn[] = {-0.00546954795538729307482955, 0.00404190278731707110245394,
                                                 0.0054963369553161170521356,   0.00212616472603945399437862,
                                                 0.000394984014495083900689956, 0.365565477064442377259271e-4,
                                                 0.135485897109932323253786e-5};
            static constexpr f64 erf_imp_fd[] = {1,
                                                 1.21019697773630784832251,
                                                 0.620914668221143886601045,
                                                 0.173038430661142762569515,
                                                 0.0276550813773432047594539,
                                                 0.00240625974424309709745382,
                                                 0.891811817251336577241006e-4,
                                                 -0.465528836283382684461025e-11};
            r = evaluatePolynomial(z - 3.5, erf_imp_fn) / evaluatePolynomial(z - 3.5, erf_imp_fd);
            b = 0.5489973426f;
        } else if(z < 8) {
            static constexpr f64 erf_imp_gn[] = {-0.00270722535905778347999196, 0.0013187563425029400461378,
                                                 0.00119925933261002333923989,  0.00027849619811344664248235,
                                                 0.267822988218331849989363e-4, 0.923043672315028197865066e-6};
            static constexpr f64 erf_imp_gd[] = {1,
                                                 0.814632808543141591118279,
                                                 0.268901665856299542168425,
                                                 0.0449877216103041118694989,
                                                 0.00381759663320248459168994,
                                                 0.000131571897888596914350697,
                                                 0.404815359675764138445257e-11};
            r = evaluatePolynomial(z - 5.25, erf_imp_gn) / evaluatePolynomial(z - 5.25, erf_imp_gd);
            b = 0.5571740866f;
        } else if(z < 11.5) {
            static constexpr f64 erf_imp_hn[] = {-0.00109946720691742196814323, 0.000406425442750422675169153,
                                                 0.000274499489416900707787024, 0.465293770646659383436343e-4,
                                                 0.320955425395767463401993e-5, 0.778286018145020892261936e-7};
            static constexpr f64 erf_imp_hd[] = {1,
                                                 0.588173710611846046373373,
                                                 0.139363331289409746077541,
                                                 0.0166329340417083678763028,
                                                 0.00100023921310234908642639,
                                                 0.24254837521587225125068e-4};
            r = evaluatePolynomial(z - 8, erf_imp_hn) / evaluatePolynomial(z - 8, erf_imp_hd);
            b = 0.5609807968f;
        } else if(z < 17) {
            static constexpr f64 erf_imp_in[] = {-0.00056907993601094962855594, 0.000169498540373762264416984,
                                                 0.518472354581100890120501e-4, 0.382819312231928859704678e-5,
                                                 0.824989931281894431781794e-7};
            static constexpr f64 erf_imp_id[] = {1,
                                                 0.339637250051139347430323,
                                                 0.043472647870310663055044,
                                                 0.00248549335224637114641629,
                                                 0.535633305337152900549536e-4,
                                                 -0.117490944405459578783846e-12};
            r = evaluatePolynomial(z - 11.5, erf_imp_in) / evaluatePolynomial(z - 11.5, erf_imp_id);
            b = 0.5626493692f;
        } else if(z < 24) {
            static constexpr f64 erf_imp_jn[] = {-0.000241313599483991337479091, 0.574224975202501512365975e-4,
                                                 0.115998962927383778460557e-4, 0.581762134402593739370875e-6,
                                                 0.853971555085673614607418e-8};
            static constexpr f64 erf_imp_jd[] = {1, 0.233044138299687841018015, 0.0204186940546440312625597,
                                                 0.000797185647564398289151125, 0.117019281670172327758019e-4};
            r = evaluatePolynomial(z - 17, erf_imp_jn) / evaluatePolynomial(z - 17, erf_imp_jd);
            b = 0.5634598136f;
        } else if(z < 38) {
            static constexpr f64 erf_imp_kn[] = {-0.000146674699277760365803642, 0.162666552112280519955647e-4,
                                                 0.269116248509165239294897e-5, 0.979584479468091935086972e-7,
                                                 0.101994647625723465722285e-8};
            static constexpr f64 erf_imp_kd[] = {1, 0.165907812944847226546036, 0.0103361716191505884359634,
                                                 0.000286593026373868366935721, 0.298401570840900340874568e-5};
            r = evaluatePolynomial(z - 24, erf_imp_kn) / evaluatePolynomial(z - 24, erf_imp_kd);
            b = 0.5638477802f;
        } else if(z < 60) {
            static constexpr f64 erf_imp_ln[] = {-0.583905797629771786720406e-4, 0.412510325105496173512992e-5,
                                                 0.431790922420250949096906e-6, 0.993365155590013193345569e-8,
                                                 0.653480510020104699270084e-10};
            static constexpr f64 erf_imp_ld[] = {1, 0.105077086072039915406159, 0.00414278428675475620830226,
                                                 0.726338754644523769144108e-4, 0.477818471047398785369849e-6};
            r = evaluatePolynomial(z - 38, erf_imp_ln) / evaluatePolynomial(z - 38, erf_imp_ld);
            b = 0.5640528202f;
        } else if(z < 85) {
            static constexpr f64 erf_imp_mn[] = {-0.196457797609229579459841e-4, 0.157243887666800692441195e-5,
                                                 0.543902511192700878690335e-7, 0.317472492369117710852685e-9};
            static constexpr f64 erf_imp_md[] = {1, 0.052803989240957632204885, 0.000926876069151753290378112,
                                                 0.541011723226630257077328e-5, 0.535093845803642394908747e-15};
            r = evaluatePolynomial(z - 60, erf_imp_mn) / evaluatePolynomial(z - 60, erf_imp_md);
            b = 0.5641309023f;
        } else {
            static constexpr f64 erf_imp_nn[] = {-0.789224703978722689089794e-5, 0.622088451660986955124162e-6,
                                                 0.145728445676882396797184e-7, 0.603715505542715364529243e-10};
            static constexpr f64 erf_imp_nd[] = {1, 0.0375328846356293715248719, 0.000467919535974625308126054,
                                                 0.193847039275845656900547e-5};
            r = evaluatePolynomial(z - 85, erf_imp_nn) / evaluatePolynomial(z - 85, erf_imp_nd);
            b = 0.5641584396f;
        }

        f64 g = std::exp(-z * z) / z;
        result = (g * b) + (g * r);
    } else {
        result = 0.0;
        invert = !invert;
    }

    if(invert) result = 1.0 - result;

    return result;
}

f64 DifficultyCalculator::erfInvImp(f64 p, f64 q, f64 s) {
    f64 result;

    if(p <= 0.5) {
        static constexpr f64 erv_inv_imp_an[] = {-0.000508781949658280665617, -0.00836874819741736770379,
                                                 0.0334806625409744615033,    -0.0126926147662974029034,
                                                 -0.0365637971411762664006,   0.0219878681111168899165,
                                                 0.00822687874676915743155,   -0.00538772965071242932965};
        static constexpr f64 erv_inv_imp_ad[] = {1,
                                                 -0.970005043303290640362,
                                                 -1.56574558234175846809,
                                                 1.56221558398423026363,
                                                 0.662328840472002992063,
                                                 -0.71228902341542847553,
                                                 -0.0527396382340099713954,
                                                 0.0795283687341571680018,
                                                 -0.00233393759374190016776,
                                                 0.000886216390456424707504};
        const f32 y = 0.0891314744949340820313f;
        f64 g = p * (p + 10);
        f64 r = evaluatePolynomial(p, erv_inv_imp_an) / evaluatePolynomial(p, erv_inv_imp_ad);
        result = (g * y) + (g * r);
    } else if(q >= 0.25) {
        static constexpr f64 erv_inv_imp_bn[] = {
            -0.202433508355938759655, 0.105264680699391713268, 8.37050328343119927838,
            17.6447298408374015486,   -18.8510648058714251895, -44.6382324441786960818,
            17.445385985570866523,    21.1294655448340526258,  -3.67192254707729348546};
        static constexpr f64 erv_inv_imp_bd[] = {1,
                                                 6.24264124854247537712,
                                                 3.9713437953343869095,
                                                 -28.6608180499800029974,
                                                 -20.1432634680485188801,
                                                 48.5609213108739935468,
                                                 10.8268667355460159008,
                                                 -22.6436933413139721736,
                                                 1.72114765761200282724};
        const f32 y = 2.249481201171875f;
        f64 g = std::sqrt(-2.0 * std::log(q));
        f64 xs = q - 0.25;
        f64 r = evaluatePolynomial(xs, erv_inv_imp_bn) / evaluatePolynomial(xs, erv_inv_imp_bd);
        result = g / (y + r);
    } else {
        f64 x = std::sqrt(-std::log(q));

        if(x < 3) {
            static constexpr f64 erv_inv_imp_cn[] = {
                -0.131102781679951906451,   -0.163794047193317060787,   0.117030156341995252019,
                0.387079738972604337464,    0.337785538912035898924,    0.142869534408157156766,
                0.0290157910005329060432,   0.00214558995388805277169,  -0.679465575181126350155e-6,
                0.285225331782217055858e-7, -0.681149956853776992068e-9};
            static constexpr f64 erv_inv_imp_cd[] = {1,
                                                     3.46625407242567245975,
                                                     5.38168345707006855425,
                                                     4.77846592945843778382,
                                                     2.59301921623620271374,
                                                     0.848854343457902036425,
                                                     0.152264338295331783612,
                                                     0.01105924229346489121};
            const f32 y = 0.807220458984375f;
            f64 xs = x - 1.125;
            f64 r = evaluatePolynomial(xs, erv_inv_imp_cn) / evaluatePolynomial(xs, erv_inv_imp_cd);
            result = (y * x) + (r * x);
        } else if(x < 6) {
            static constexpr f64 erv_inv_imp_dn[] = {
                -0.0350353787183177984712,  -0.00222426529213447927281,  0.0185573306514231072324,
                0.00950804701325919603619,  0.00187123492819559223345,   0.000157544617424960554631,
                0.460469890584317994083e-5, -0.230404776911882601748e-9, 0.266339227425782031962e-11};
            static constexpr f64 erv_inv_imp_dd[] = {1,
                                                     1.3653349817554063097,
                                                     0.762059164553623404043,
                                                     0.220091105764131249824,
                                                     0.0341589143670947727934,
                                                     0.00263861676657015992959,
                                                     0.764675292302794483503e-4};
            const f32 y = 0.93995571136474609375f;
            f64 xs = x - 3;
            f64 r = evaluatePolynomial(xs, erv_inv_imp_dn) / evaluatePolynomial(xs, erv_inv_imp_dd);
            result = (y * x) + (r * x);
        } else if(x < 18) {
            static constexpr f64 erv_inv_imp_en[] = {
                -0.0167431005076633737133,  -0.00112951438745580278863,   0.00105628862152492910091,
                0.000209386317487588078668, 0.149624783758342370182e-4,   0.449696789927706453732e-6,
                0.462596163522878599135e-8, -0.281128735628831791805e-13, 0.99055709973310326855e-16};
            static constexpr f64 erv_inv_imp_ed[] = {1,
                                                     0.591429344886417493481,
                                                     0.138151865749083321638,
                                                     0.0160746087093676504695,
                                                     0.000964011807005165528527,
                                                     0.275335474764726041141e-4,
                                                     0.282243172016108031869e-6};
            const f32 y = 0.98362827301025390625f;
            f64 xs = x - 6;
            f64 r = evaluatePolynomial(xs, erv_inv_imp_en) / evaluatePolynomial(xs, erv_inv_imp_ed);
            result = (y * x) + (r * x);
        } else if(x < 44) {
            static constexpr f64 erv_inv_imp_fn[] = {-0.0024978212791898131227,   -0.779190719229053954292e-5,
                                                     0.254723037413027451751e-4,  0.162397777342510920873e-5,
                                                     0.396341011304801168516e-7,  0.411632831190944208473e-9,
                                                     0.145596286718675035587e-11, -0.116765012397184275695e-17};
            static constexpr f64 erv_inv_imp_fd[] = {1,
                                                     0.207123112214422517181,
                                                     0.0169410838120975906478,
                                                     0.000690538265622684595676,
                                                     0.145007359818232637924e-4,
                                                     0.144437756628144157666e-6,
                                                     0.509761276599778486139e-9};
            const f32 y = 0.99714565277099609375f;
            f64 xs = x - 18;
            f64 r = evaluatePolynomial(xs, erv_inv_imp_fn) / evaluatePolynomial(xs, erv_inv_imp_fd);
            result = (y * x) + (r * x);
        } else {
            static constexpr f64 erv_inv_imp_gn[] = {-0.000539042911019078575891, -0.28398759004727721098e-6,
                                                     0.899465114892291446442e-6,  0.229345859265920864296e-7,
                                                     0.225561444863500149219e-9,  0.947846627503022684216e-12,
                                                     0.135880130108924861008e-14, -0.348890393399948882918e-21};
            static constexpr f64 erv_inv_imp_gd[] = {1,
                                                     0.0845746234001899436914,
                                                     0.00282092984726264681981,
                                                     0.468292921940894236786e-4,
                                                     0.399968812193862100054e-6,
                                                     0.161809290887904476097e-8,
                                                     0.231558608310259605225e-11};
            const f32 y = 0.99941349029541015625f;
            f64 xs = x - 44;
            f64 r = evaluatePolynomial(xs, erv_inv_imp_gn) / evaluatePolynomial(xs, erv_inv_imp_gd);
            result = (y * x) + (r * x);
        }
    }

    return s * result;
}

void DifficultyCalculator::DiffObject::calculate_strains(const DiffObject &prev, const DiffObject *next,
                                                         f64 hitWindow300) {
    calculate_strain(prev, next, hitWindow300, Skills::SPEED);
    calculate_strain(prev, next, hitWindow300, Skills::AIM_SLIDERS);
    calculate_strain(prev, next, hitWindow300, Skills::AIM_NO_SLIDERS);
}

void DifficultyCalculator::DiffObject::calculate_strain(const DiffObject &prev, const DiffObject *next,
                                                        f64 hitWindow300, const Skills::Skill dtype) {
    f64 currentStrainOfDiffObject = 0;

    const i32 time_elapsed = ho->time - prev.ho->time;

    // update our delta time
    delta_time = (f64)time_elapsed;
    strain_time = (f64)std::max(time_elapsed, 25);

    switch(ho->type) {
        case OsuDifficultyHitObject::TYPE::SLIDER:
        case OsuDifficultyHitObject::TYPE::CIRCLE:
            currentStrainOfDiffObject = spacing_weight2(dtype, prev, next, hitWindow300);
            break;

        case OsuDifficultyHitObject::TYPE::SPINNER:
            break;

        case OsuDifficultyHitObject::TYPE::INVALID:
            // NOTE: silently ignore
            return;
    }

    // see Process() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    f64 currentStrain = prev.strains[dtype];
    {
        currentStrain *= strainDecay(dtype, dtype == Skills::SPEED ? strain_time : delta_time);
        currentStrain += currentStrainOfDiffObject * weight_scaling[dtype];
    }
    strains[dtype] = currentStrain;
}

f64 DifficultyCalculator::DiffObject::calculate_difficulty(const Skills::Skill type, const DiffObject *dobjects,
                                                           size_t dobjectCount, IncrementalState *incremental,
                                                           std::vector<f64> *outStrains, f64 *outDifficultStrains,
                                                           f64 *outSkillSpecificAttrib) {
    // (old) see https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    // (new) see https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs

    static const f64 strain_step = 400.0;  // the length of each strain section
    static const f64 decay_weight =
        0.9;  // max strains are weighted from highest to lowest, and this is how much the weight decays.

    if(dobjectCount < 1) return 0.0;

    f64 interval_end =
        incremental ? incremental->interval_end : (std::ceil((f64)dobjects[0].ho->time / strain_step) * strain_step);
    f64 max_strain = incremental ? incremental->max_strain : 0.0;

    std::vector<f64> highestStrains;
    std::vector<f64> *highestStrainsRef = incremental ? &incremental->highest_strains : &highestStrains;
    std::vector<f64> sliderStrains;
    std::vector<f64> *sliderStrainsRef = incremental ? &incremental->slider_strains : &sliderStrains;
    for(size_t i = (incremental ? dobjectCount - 1 : 0); i < dobjectCount; i++) {
        const DiffObject &cur = dobjects[i];
        const DiffObject &prev = dobjects[i > 0 ? i - 1 : i];

        // make previous peak strain decay until the current object
        while(cur.ho->time > interval_end) {
            if(incremental)
                highestStrainsRef->insert(std::ranges::upper_bound(*highestStrainsRef, max_strain), max_strain);
            else
                highestStrainsRef->push_back(max_strain);

            // skip calculating strain decay for very long breaks (e.g. beatmap upload size limit hack diffs)
            // strainDecay with a base of 0.3 at 60 seconds is 4.23911583e-32, well below any meaningful difference even after being multiplied by object strain
            f64 strainDelta = interval_end - (f64)prev.ho->time;
            if(i < 1 || strainDelta > 600000.0)  // !prev
                max_strain = 0.0;
            else
                max_strain = prev.get_strain(type) * strainDecay(type, strainDelta);

            interval_end += strain_step;
        }

        // calculate max strain for this interval
        f64 cur_strain = cur.get_strain(type);
        max_strain = std::max(max_strain, cur_strain);

        // NOTE: this is done in StrainValueAt in lazer's code, but doing it here is more convenient for the incremental case
        if(type == Skills::AIM_SLIDERS && cur.ho->type == OsuDifficultyHitObject::TYPE::SLIDER)
            sliderStrainsRef->push_back(cur_strain);
    }

    // the peak strain will not be saved for the last section in the above loop
    if(incremental) {
        incremental->interval_end = interval_end;
        incremental->max_strain = max_strain;
        highestStrains.reserve(incremental->highest_strains.size() + 1);  // required so insert call doesn't reallocate
        highestStrains = incremental->highest_strains;
        highestStrains.insert(std::ranges::upper_bound(highestStrains, max_strain), max_strain);
    } else
        highestStrains.push_back(max_strain);

    if(outStrains != nullptr) (*outStrains) = highestStrains;  // save a copy

    if(outSkillSpecificAttrib) {
        if(type == Skills::SPEED) {
            // calculate relevant speed note count
            // RelevantNoteCount @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
            const auto compareDiffObjects = [=](const DiffObject &x, const DiffObject &y) {
                return (x.get_strain(type) < y.get_strain(type));
            };

            f64 maxObjectStrain;
            {
                if(incremental)
                    maxObjectStrain =
                        std::max(incremental->max_object_strain, dobjects[dobjectCount - 1].get_strain(type));
                else
                    maxObjectStrain =
                        (*std::max_element(dobjects, dobjects + dobjectCount, compareDiffObjects)).get_strain(type);
            }

            if(maxObjectStrain == 0.0)
                *outSkillSpecificAttrib = 0.0;
            else {
                f64 tempSum = 0.0;
                if(incremental && std::abs(incremental->max_object_strain - maxObjectStrain) < DIFFCALC_EPSILON) {
                    incremental->relevant_note_sum +=
                        1.0 / (1.0 + std::exp(-((dobjects[dobjectCount - 1].get_strain(type) / maxObjectStrain * 12.0) -
                                                6.0)));
                    tempSum = incremental->relevant_note_sum;
                } else {
                    for(size_t i = 0; i < dobjectCount; i++) {
                        tempSum +=
                            1.0 / (1.0 + std::exp(-((dobjects[i].get_strain(type) / maxObjectStrain * 12.0) - 6.0)));
                    }

                    if(incremental) {
                        incremental->max_object_strain = maxObjectStrain;
                        incremental->relevant_note_sum = tempSum;
                    }
                }
                *outSkillSpecificAttrib = tempSum;
            }
        } else if(type == Skills::AIM_SLIDERS) {
            // calculate difficult sliders
            // GetDifficultSliders @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Aim.cs
            const auto compareSliderObjects = [=](const DiffObject &x, const DiffObject &y) {
                return (x.get_slider_aim_strain() < y.get_slider_aim_strain());
            };

            if(incremental && dobjects[dobjectCount - 1].ho->type != OsuDifficultyHitObject::TYPE::SLIDER)
                *outSkillSpecificAttrib = incremental->difficult_sliders;
            else {
                f64 maxSliderStrain;
                f64 curSliderStrain = incremental ? dobjects[dobjectCount - 1].strains[Skills::AIM_SLIDERS] : 0.0;
                {
                    if(incremental) {
                        incremental->slider_strains.push_back(curSliderStrain);
                        maxSliderStrain = std::max(incremental->max_slider_strain, curSliderStrain);
                    } else
                        maxSliderStrain = (*std::max_element(dobjects, dobjects + dobjectCount, compareSliderObjects))
                                              .get_slider_aim_strain();
                }

                if(maxSliderStrain <= 0.0)
                    *outSkillSpecificAttrib = 0.0;
                else {
                    f64 tempSum = 0.0;
                    if(incremental && std::abs(incremental->max_slider_strain - maxSliderStrain) < DIFFCALC_EPSILON) {
                        incremental->difficult_sliders +=
                            1.0 / (1.0 + std::exp(-((curSliderStrain / maxSliderStrain * 12.0) - 6.0)));
                        tempSum = incremental->difficult_sliders;
                    } else {
                        if(incremental) {
                            for(f64 slider_strain : incremental->slider_strains) {
                                tempSum += 1.0 / (1.0 + std::exp(-((slider_strain / maxSliderStrain * 12.0) - 6.0)));
                            }
                            incremental->max_slider_strain = maxSliderStrain;
                            incremental->difficult_sliders = tempSum;
                        } else {
                            for(size_t i = 0; i < dobjectCount; i++) {
                                f64 sliderStrain = dobjects[i].get_slider_aim_strain();
                                if(sliderStrain >= 0.0)
                                    tempSum += 1.0 / (1.0 + std::exp(-((sliderStrain / maxSliderStrain * 12.0) - 6.0)));
                            }
                        }
                    }
                    *outSkillSpecificAttrib = tempSum;
                }
            }
        }
    }

    // (old) see DifficultyValue() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    // (new) see DifficultyValue() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs
    // (new) see DifficultyValue() @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/OsuStrainSkill.cs

    static const size_t reducedSectionCount = 10;
    static const f64 reducedStrainBaseline = 0.75;

    f64 difficulty = 0.0;
    f64 weight = 1.0;

    // sort strains
    {
        // new implementation
        // NOTE: lazer does this from highest to lowest, but sorting it in reverse lets the reduced top section loop below have a better average insertion time
        if(!incremental) std::ranges::sort(highestStrains);
    }

    // new implementation (https://github.com/ppy/osu/pull/13483/)
    {
        size_t skillSpecificReducedSectionCount = reducedSectionCount;
        {
            switch(type) {
                case Skills::SPEED:
                    skillSpecificReducedSectionCount = 5;
                    break;
                case Skills::AIM_SLIDERS:
                case Skills::AIM_NO_SLIDERS:
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
        f64 reducedSections
            [reducedSectionCount];  // actualReducedSectionCount <= skillSpecificReducedSectionCount <= reducedSectionCount
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
            if(std::abs(difficulty - last) < DIFFCALC_EPSILON) break;
        }
    }

    // see CountDifficultStrains @ https://github.com/ppy/osu/pull/16280/files#diff-07543a9ffe2a8d7f02cadf8ef7f81e3d7ec795ec376b2fff8bba7b10fb574e19R78
    if(outDifficultStrains) {
        if(difficulty == 0.0)
            *outDifficultStrains = difficulty;
        else {
            f64 tempSum = 0.0;
            {
                f64 consistentTopStrain = difficulty / 10.0;

                if(incremental &&
                   std::abs(incremental->consistent_top_strain - consistentTopStrain) < DIFFCALC_EPSILON) {
                    incremental->difficult_strains +=
                        1.1 /
                        (1.0 +
                         std::exp(-10.0 * (dobjects[dobjectCount - 1].get_strain(type) / consistentTopStrain - 0.88)));
                    tempSum = incremental->difficult_strains;
                } else {
                    for(size_t i = 0; i < dobjectCount; i++) {
                        tempSum +=
                            1.1 / (1.0 + std::exp(-10.0 * (dobjects[i].get_strain(type) / consistentTopStrain - 0.88)));
                    }

                    if(incremental) {
                        incremental->consistent_top_strain = consistentTopStrain;
                        incremental->difficult_strains = tempSum;
                    }
                }
            }
            *outDifficultStrains = tempSum;
        }
    }

    return difficulty;
}

// new implementation, Xexxar, (ppv2.1), see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/
f64 DifficultyCalculator::DiffObject::spacing_weight2(const Skills::Skill diff_type, const DiffObject &prev,
                                                      const DiffObject *next, f64 hitWindow300) {
    static const f64 single_spacing_threshold = 125.0;

    static const f64 min_speed_bonus = 75.0; /* ~200BPM 1/4 streams */
    static const f64 speed_balancing_factor = 40.0;
    static const f64 distance_multiplier = 0.9;

    static const i32 history_time_max = 5000;
    static const i32 history_objects_max = 32;
    static const f64 rhythm_overall_multiplier = 0.95;
    static const f64 rhythm_ratio_multiplier = 12.0;

    //f64 angle_bonus = 1.0; // (apparently unused now in lazer?)

    switch(diff_type) {
        case Skills::SPEED: {
            // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
            if(ho->type == OsuDifficultyHitObject::TYPE::SPINNER) {
                raw_speed_strain = 0.0;
                rhythm = 0.0;

                return 0.0;
            }

            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/SpeedEvaluator.cs
            const f64 distance = std::min(single_spacing_threshold, prev.travelDistance + minJumpDistance);

            f64 strain_time = this->strain_time;
            strain_time /= std::clamp<f64>((strain_time / hitWindow300) / 0.93, 0.92, 1.0);

            f64 doubletapness = 1.0 - get_doubletapness(next, hitWindow300);

            f64 speed_bonus = 0.0;
            if(strain_time < min_speed_bonus)
                speed_bonus = 0.75 * std::pow((min_speed_bonus - strain_time) / speed_balancing_factor, 2.0);

            f64 distance_bonus = std::pow(distance / single_spacing_threshold, 3.95) * distance_multiplier;
            raw_speed_strain = (1.0 + speed_bonus + distance_bonus) * 1000.0 * doubletapness / strain_time;

            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/RhythmEvaluator.cs
            f64 rhythmComplexitySum = 0;

            const f64 deltaDifferenceEpsilon = hitWindow300 * 0.3;

            RhythmIsland island{INT_MAX, 0};
            RhythmIsland previousIsland{INT_MAX, 0};

            std::vector<std::pair<RhythmIsland, int>> islandCounts;

            f64 startRatio = 0.0;  // store the ratio of the current start of an island to buff for tighter rhythms

            bool firstDeltaSwitch = false;

            i32 historicalNoteCount = std::min(prevObjectIndex, history_objects_max);

            i32 rhythmStart = 0;

            while(rhythmStart < historicalNoteCount - 2 &&
                  ho->time - get_previous(rhythmStart)->ho->time < history_time_max) {
                rhythmStart++;
            }

            const DiffObject *prevObj = get_previous(rhythmStart);
            const DiffObject *lastObj = get_previous(rhythmStart + 1);

            for(i32 i = rhythmStart; i > 0; i--) {
                const DiffObject *currObj = get_previous(i - 1);

                // scales note 0 to 1 from history to now
                f64 timeDecay = (history_time_max - (ho->time - currObj->ho->time)) / (f64)history_time_max;
                f64 noteDecay = (f64)(historicalNoteCount - i) / historicalNoteCount;

                f64 currHistoricalDecay =
                    std::min(noteDecay, timeDecay);  // either we're limited by time or limited by object count.

                f64 currDelta = currObj->strain_time;
                f64 prevDelta = prevObj->strain_time;
                f64 lastDelta = lastObj->strain_time;

                // calculate how much current delta difference deserves a rhythm bonus
                // this function is meant to reduce rhythm bonus for deltas that are multiples of each other (i.e 100 and 200)
                f64 deltaDifferenceRatio = std::min(prevDelta, currDelta) / std::max(prevDelta, currDelta);
                f64 currRatio =
                    1.0 + rhythm_ratio_multiplier * std::min(0.5, std::pow(std::sin(PI / deltaDifferenceRatio), 2.0));

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
                            island.delta = std::max((i32)currDelta, 25);
                        }
                        island.deltaCount++;
                    } else {
                        if(currObj->ho->type ==
                           OsuDifficultyHitObject::TYPE::SLIDER)  // bpm change is into slider, this is easy acc window
                            effectiveRatio *= 0.125;

                        if(prevObj->ho->type ==
                           OsuDifficultyHitObject::TYPE::
                               SLIDER)  // bpm change was from a slider, this is easier typically than circle -> circle
                            effectiveRatio *= 0.3;

                        if(island.deltaCount % 2 ==
                           previousIsland.deltaCount % 2)  // repeated island polarity (2 -> 4, 3 -> 5)
                            effectiveRatio *= 0.5;

                        if(lastDelta > prevDelta + deltaDifferenceEpsilon &&
                           prevDelta >
                               currDelta +
                                   deltaDifferenceEpsilon)  // previous increase happened a note ago, 1/1->1/2-1/4, dont want to buff this.
                            effectiveRatio *= 0.125;

                        if(previousIsland.deltaCount ==
                           island.deltaCount)  // repeated island size (ex: triplet -> triplet)
                            effectiveRatio *= 0.5;

                        std::pair<RhythmIsland, int> *islandCount = nullptr;
                        for(i32 i = 0; i < islandCounts.size(); i++) {
                            if(islandCounts[i].first.equals(island, deltaDifferenceEpsilon)) {
                                islandCount = &islandCounts[i];
                                break;
                            }
                        }

                        if(islandCount != nullptr) {
                            // only add island to island counts if they're going one after another
                            if(previousIsland.equals(island, deltaDifferenceEpsilon)) islandCount->second++;

                            // repeated island (ex: triplet -> triplet)
                            static constexpr f64 E = 2.7182818284590451;
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

                        if(prevDelta + deltaDifferenceEpsilon < currDelta)  // we're slowing down, stop counting
                            firstDeltaSwitch =
                                false;  // if we're speeding up, this stays true and  we keep counting island size.

                        island = RhythmIsland{std::max((i32)currDelta, 25), 1};
                    }
                } else if(prevDelta > currDelta + deltaDifferenceEpsilon)  // we want to be speeding up.
                {
                    // Begin counting island until we change speed again.
                    firstDeltaSwitch = true;

                    if(currObj->ho->type ==
                       OsuDifficultyHitObject::TYPE::SLIDER)  // bpm change is into slider, this is easy acc window
                        effectiveRatio *= 0.6;

                    if(prevObj->ho->type ==
                       OsuDifficultyHitObject::TYPE::
                           SLIDER)  // bpm change was from a slider, this is easier typically than circle -> circle
                        effectiveRatio *= 0.6;

                    startRatio = effectiveRatio;

                    island = RhythmIsland{std::max((i32)currDelta, 25), 1};
                }

                lastObj = prevObj;
                prevObj = currObj;
            }

            rhythm = std::sqrt(4.0 + rhythmComplexitySum * rhythm_overall_multiplier) / 2.0;

            return raw_speed_strain;
        } break;

        case Skills::AIM_SLIDERS:
        case Skills::AIM_NO_SLIDERS: {
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/AimEvaluator.cs
            static constexpr f64 wide_angle_multiplier = 1.5;
            static constexpr f64 acute_angle_multiplier = 2.6;
            static constexpr f64 slider_multiplier = 1.35;
            static constexpr f64 velocity_change_multiplier = 0.75;
            static constexpr f64 wiggle_multiplier = 1.02;

            const bool withSliders = (diff_type == Skills::AIM_SLIDERS);

            if(ho->type == OsuDifficultyHitObject::TYPE::SPINNER || prevObjectIndex <= 1 ||
               prev.ho->type == OsuDifficultyHitObject::TYPE::SPINNER)
                return 0.0;

            static constexpr auto reverseLerp = [](f64 x, f64 start, f64 end) {
                return std::clamp<f64>((x - start) / (end - start), 0.0, 1.0);
            };
            static constexpr auto smoothStep = [](f64 x, f64 start, f64 end) {
                x = reverseLerp(x, start, end);
                return x * x * (3.0 - 2.0 * x);
            };
            static constexpr auto smootherStep = [](f64 x, f64 start, f64 end) {
                x = reverseLerp(x, start, end);
                return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
            };
            static constexpr auto calcWideAngleBonus = [](f64 angle) {
                return smoothStep(angle, 40.0 * (PI / 180.0), 140.0 * (PI / 180.0));
            };
            static constexpr auto calcAcuteAngleBonus = [](f64 angle) {
                return smoothStep(angle, 140.0 * (PI / 180.0), 40.0 * (PI / 180.0));
            };

            const DiffObject *prevPrev = get_previous(1);
            f64 currVelocity = jumpDistance / strain_time;

            if(prev.ho->type == OsuDifficultyHitObject::TYPE::SLIDER && withSliders) {
                f64 travelVelocity = prev.travelDistance / prev.travelTime;
                f64 movementVelocity = minJumpDistance / minJumpTime;
                currVelocity = std::max(currVelocity, movementVelocity + travelVelocity);
            }
            f64 aimStrain = currVelocity;

            f64 prevVelocity = prev.jumpDistance / prev.strain_time;
            if(prevPrev->ho->type == OsuDifficultyHitObject::TYPE::SLIDER && withSliders) {
                f64 travelVelocity = prevPrev->travelDistance / prevPrev->travelTime;
                f64 movementVelocity = prev.minJumpDistance / prev.minJumpTime;
                prevVelocity = std::max(prevVelocity, movementVelocity + travelVelocity);
            }

            f64 wideAngleBonus = 0;
            f64 acuteAngleBonus = 0;
            f64 sliderBonus = 0;
            f64 velocityChangeBonus = 0;
            f64 wiggleBonus = 0;

            if(std::max(strain_time, prev.strain_time) < 1.25 * std::min(strain_time, prev.strain_time)) {
                if(!std::isnan(angle) && !std::isnan(prev.angle)) {
                    f64 angleBonus = std::min(currVelocity, prevVelocity);

                    wideAngleBonus = calcWideAngleBonus(angle);
                    acuteAngleBonus = calcAcuteAngleBonus(angle);

                    wideAngleBonus *= 1.0 - std::min(wideAngleBonus, pow(calcWideAngleBonus(prev.angle), 3.0));
                    acuteAngleBonus *=
                        0.08 + 0.92 * (1.0 - std::min(acuteAngleBonus, std::pow(calcAcuteAngleBonus(prev.angle), 3.0)));

                    wideAngleBonus *= angleBonus * smootherStep(jumpDistance, 0.0, 100.0);

                    acuteAngleBonus *= angleBonus * smootherStep(60000.0 / (strain_time * 2.0), 300.0, 400.0) *
                                       smootherStep(jumpDistance, 100.0, 200.0);

                    wiggleBonus = angleBonus * smootherStep(jumpDistance, 50.0, 100.0) *
                                  pow(reverseLerp(jumpDistance, 300.0, 100.0), 1.8) *
                                  smootherStep(angle, 110.0 * (PI / 180.0), 60.0 * (PI / 180.0)) *
                                  smootherStep(prev.jumpDistance, 50.0, 100.0) *
                                  pow(reverseLerp(prev.jumpDistance, 300.0, 100.0), 1.8) *
                                  smootherStep(prev.angle, 110.0 * (PI / 180.0), 60.0 * (PI / 180.0));
                }
            }

            if(std::max(prevVelocity, currVelocity) != 0.0) {
                prevVelocity = (prev.jumpDistance + prevPrev->travelDistance) / prev.strain_time;
                currVelocity = (jumpDistance + prev.travelDistance) / strain_time;

                f64 distRatio = std::pow(
                    std::sin(PI / 2.0 * std::abs(prevVelocity - currVelocity) / std::max(prevVelocity, currVelocity)),
                    2.0);
                f64 overlapVelocityBuff =
                    std::min(125.0 / std::min(strain_time, prev.strain_time), std::abs(prevVelocity - currVelocity));
                velocityChangeBonus =
                    overlapVelocityBuff * distRatio *
                    std::pow(std::min(strain_time, prev.strain_time) / std::max(strain_time, prev.strain_time), 2.0);
            }

            if(prev.ho->type == OsuDifficultyHitObject::TYPE::SLIDER)
                sliderBonus = prev.travelDistance / prev.travelTime;

            aimStrain += wiggleBonus * wiggle_multiplier;

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
        f64 cur_delta = std::max(1.0, delta_time);
        f64 next_delta = std::max(1, next->ho->time - ho->time);  // next delta time isn't initialized yet
        f64 delta_diff = std::abs(next_delta - cur_delta);
        f64 speedRatio = cur_delta / std::max(cur_delta, delta_diff);
        f64 windowRatio = std::pow(std::min(1.0, cur_delta / hitWindow300), 2.0);

        return 1.0 - std::pow(speedRatio, 1.0 - windowRatio);
    }
    return 0.0;
}
