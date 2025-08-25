#include "HitObjects.h"

#include <cmath>
#include <utility>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "Beatmap.h"
#include "Camera.h"
#include "ConVar.h"
#include "Engine.h"
#include "GameRules.h"
#include "HUD.h"
#include "ModFPoSu.h"
#include "Mouse.h"
#include "Osu.h"
#include "OpenGLHeaders.h"
#include "OpenGLLegacyInterface.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SliderCurves.h"
#include "SliderRenderer.h"
#include "SoundEngine.h"
#include "VertexArrayObject.h"

void HitObject::drawHitResult(Beatmap *beatmap, vec2 rawPos, LiveScore::HIT result, float animPercentInv,
                              float hitDeltaRangePercent) {
    drawHitResult(beatmap->getSkin(), beatmap->fHitcircleDiameter, beatmap->fRawHitcircleDiameter, rawPos, result,
                  animPercentInv, hitDeltaRangePercent);
}

void HitObject::drawHitResult(Skin *skin, float hitcircleDiameter, float rawHitcircleDiameter, vec2 rawPos,
                              LiveScore::HIT result, float animPercentInv, float hitDeltaRangePercent) {
    if(animPercentInv <= 0.0f) return;

    const float animPercent = 1.0f - animPercentInv;

    const float fadeInEndPercent = cv::hitresult_fadein_duration.getFloat() / cv::hitresult_duration.getFloat();

    // determine color/transparency
    {
        if(!cv::hitresult_delta_colorize.getBool() || result == LiveScore::HIT::HIT_MISS)
            g->setColor(0xffffffff);
        else {
            // NOTE: hitDeltaRangePercent is within -1.0f to 1.0f
            // -1.0f means early miss
            // 1.0f means late miss
            // -0.999999999f means early 50
            // 0.999999999f means late 50
            // percentage scale is linear with respect to the entire hittable 50s range in both directions (contrary to
            // OD brackets which are nonlinear of course)
            if(hitDeltaRangePercent != 0.0f) {
                hitDeltaRangePercent = std::clamp<float>(
                    hitDeltaRangePercent * cv::hitresult_delta_colorize_multiplier.getFloat(), -1.0f, 1.0f);

                const float rf = lerp3f(cv::hitresult_delta_colorize_early_r.getFloat() / 255.0f, 1.0f,
                                        cv::hitresult_delta_colorize_late_r.getFloat() / 255.0f,
                                        cv::hitresult_delta_colorize_interpolate.getBool()
                                            ? hitDeltaRangePercent / 2.0f + 0.5f
                                            : (hitDeltaRangePercent < 0.0f ? -1.0f : 1.0f));
                const float gf = lerp3f(cv::hitresult_delta_colorize_early_g.getFloat() / 255.0f, 1.0f,
                                        cv::hitresult_delta_colorize_late_g.getFloat() / 255.0f,
                                        cv::hitresult_delta_colorize_interpolate.getBool()
                                            ? hitDeltaRangePercent / 2.0f + 0.5f
                                            : (hitDeltaRangePercent < 0.0f ? -1.0f : 1.0f));
                const float bf = lerp3f(cv::hitresult_delta_colorize_early_b.getFloat() / 255.0f, 1.0f,
                                        cv::hitresult_delta_colorize_late_b.getFloat() / 255.0f,
                                        cv::hitresult_delta_colorize_interpolate.getBool()
                                            ? hitDeltaRangePercent / 2.0f + 0.5f
                                            : (hitDeltaRangePercent < 0.0f ? -1.0f : 1.0f));

                g->setColor(argb(1.0f, rf, gf, bf));
            }
        }

        const float fadeOutStartPercent =
            cv::hitresult_fadeout_start_time.getFloat() / cv::hitresult_duration.getFloat();
        const float fadeOutDurationPercent =
            cv::hitresult_fadeout_duration.getFloat() / cv::hitresult_duration.getFloat();

        g->setAlpha(std::clamp<float>(animPercent < fadeInEndPercent
                                          ? animPercent / fadeInEndPercent
                                          : 1.0f - ((animPercent - fadeOutStartPercent) / fadeOutDurationPercent),
                                      0.0f, 1.0f));
    }

    g->pushTransform();
    {
        const float osuCoordScaleMultiplier = hitcircleDiameter / rawHitcircleDiameter;

        bool doScaleOrRotateAnim = true;
        bool hasParticle = true;
        float hitImageScale = 1.0f;
        switch(result) {
            case LiveScore::HIT::HIT_MISS:
                doScaleOrRotateAnim = skin->getHit0()->getNumImages() == 1;
                hitImageScale = (rawHitcircleDiameter / skin->getHit0()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            case LiveScore::HIT::HIT_50:
                doScaleOrRotateAnim = skin->getHit50()->getNumImages() == 1;
                hasParticle = skin->getParticle50() != skin->getMissingTexture();
                hitImageScale = (rawHitcircleDiameter / skin->getHit50()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            case LiveScore::HIT::HIT_100:
                doScaleOrRotateAnim = skin->getHit100()->getNumImages() == 1;
                hasParticle = skin->getParticle100() != skin->getMissingTexture();
                hitImageScale =
                    (rawHitcircleDiameter / skin->getHit100()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            case LiveScore::HIT::HIT_300:
                doScaleOrRotateAnim = skin->getHit300()->getNumImages() == 1;
                hasParticle = skin->getParticle300() != skin->getMissingTexture();
                hitImageScale =
                    (rawHitcircleDiameter / skin->getHit300()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            case LiveScore::HIT::HIT_100K:
                doScaleOrRotateAnim = skin->getHit100k()->getNumImages() == 1;
                hasParticle = skin->getParticle100() != skin->getMissingTexture();
                hitImageScale =
                    (rawHitcircleDiameter / skin->getHit100k()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            case LiveScore::HIT::HIT_300K:
                doScaleOrRotateAnim = skin->getHit300k()->getNumImages() == 1;
                hasParticle = skin->getParticle300() != skin->getMissingTexture();
                hitImageScale =
                    (rawHitcircleDiameter / skin->getHit300k()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            case LiveScore::HIT::HIT_300G:
                doScaleOrRotateAnim = skin->getHit300g()->getNumImages() == 1;
                hasParticle = skin->getParticle300() != skin->getMissingTexture();
                hitImageScale =
                    (rawHitcircleDiameter / skin->getHit300g()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
                break;

            default:
                break;
        }

        // non-misses have a special scale animation (the type of which depends on hasParticle)
        float scale = 1.0f;
        if(doScaleOrRotateAnim && cv::hitresult_animated.getBool()) {
            if(!hasParticle) {
                if(animPercent < fadeInEndPercent * 0.8f)
                    scale =
                        std::lerp(0.6f, 1.1f, std::clamp<float>(animPercent / (fadeInEndPercent * 0.8f), 0.0f, 1.0f));
                else if(animPercent < fadeInEndPercent * 1.2f)
                    scale = std::lerp(1.1f, 0.9f,
                                      std::clamp<float>((animPercent - fadeInEndPercent * 0.8f) /
                                                            (fadeInEndPercent * 1.2f - fadeInEndPercent * 0.8f),
                                                        0.0f, 1.0f));
                else if(animPercent < fadeInEndPercent * 1.4f)
                    scale = std::lerp(0.9f, 1.0f,
                                      std::clamp<float>((animPercent - fadeInEndPercent * 1.2f) /
                                                            (fadeInEndPercent * 1.4f - fadeInEndPercent * 1.2f),
                                                        0.0f, 1.0f));
            } else
                scale = std::lerp(0.9f, 1.05f, std::clamp<float>(animPercent, 0.0f, 1.0f));

            // TODO: osu draws an additive copy of the hitresult on top (?) with 0.5 alpha anim and negative timing, if
            // the skin hasParticle. in this case only the copy does the wobble anim, while the main result just scales
        }

        switch(result) {
            case LiveScore::HIT::HIT_MISS: {
                // special case: animated misses don't move down, and skins with version <= 1 also don't move down
                vec2 downAnim{0.f};
                if(skin->getHit0()->getNumImages() < 2 && skin->getVersion() > 1.0f)
                    downAnim.y = std::lerp(-5.0f, 40.0f,
                                           std::clamp<float>(animPercent * animPercent * animPercent, 0.0f, 1.0f)) *
                                 osuCoordScaleMultiplier;

                float missScale = 1.0f + std::clamp<float>((1.0f - (animPercent / fadeInEndPercent)), 0.0f, 1.0f) *
                                             (cv::hitresult_miss_fadein_scale.getFloat() - 1.0f);
                if(!cv::hitresult_animated.getBool()) missScale = 1.0f;

                // TODO: rotation anim (only for all non-animated skins), rot = rng(-0.15f, 0.15f), anim1 = 120 ms to
                // rot, anim2 = rest to rot*2, all ease in

                skin->getHit0()->drawRaw(rawPos + downAnim, (doScaleOrRotateAnim ? missScale : 1.0f) * hitImageScale *
                                                                cv::hitresult_scale.getFloat());
            } break;

            case LiveScore::HIT::HIT_50:
                skin->getHit50()->drawRaw(
                    rawPos, (doScaleOrRotateAnim ? scale : 1.0f) * hitImageScale * cv::hitresult_scale.getFloat());
                break;

            case LiveScore::HIT::HIT_100:
                skin->getHit100()->drawRaw(
                    rawPos, (doScaleOrRotateAnim ? scale : 1.0f) * hitImageScale * cv::hitresult_scale.getFloat());
                break;

            case LiveScore::HIT::HIT_300:
                if(cv::hitresult_draw_300s.getBool()) {
                    skin->getHit300()->drawRaw(
                        rawPos, (doScaleOrRotateAnim ? scale : 1.0f) * hitImageScale * cv::hitresult_scale.getFloat());
                }
                break;

            case LiveScore::HIT::HIT_100K:
                skin->getHit100k()->drawRaw(
                    rawPos, (doScaleOrRotateAnim ? scale : 1.0f) * hitImageScale * cv::hitresult_scale.getFloat());
                break;

            case LiveScore::HIT::HIT_300K:
                if(cv::hitresult_draw_300s.getBool()) {
                    skin->getHit300k()->drawRaw(
                        rawPos, (doScaleOrRotateAnim ? scale : 1.0f) * hitImageScale * cv::hitresult_scale.getFloat());
                }
                break;

            case LiveScore::HIT::HIT_300G:
                if(cv::hitresult_draw_300s.getBool()) {
                    skin->getHit300g()->drawRaw(
                        rawPos, (doScaleOrRotateAnim ? scale : 1.0f) * hitImageScale * cv::hitresult_scale.getFloat());
                }
                break;

            default:
                break;
        }
    }
    g->popTransform();
}

HitObject::HitObject(long time, HitSamples samples, int comboNumber, bool isEndOfCombo, int colorCounter,
                     int colorOffset, BeatmapInterface *beatmap) {
    this->click_time = time;
    this->samples = std::move(samples);
    this->combo_number = comboNumber;
    this->is_end_of_combo = isEndOfCombo;
    this->iColorCounter = colorCounter;
    this->iColorOffset = colorOffset;

    this->fAlpha = 0.0f;
    this->fAlphaWithoutHidden = 0.0f;
    this->fAlphaForApproachCircle = 0.0f;
    this->fApproachScale = 0.0f;
    this->fHittableDimRGBColorMultiplierPercent = 1.0f;
    this->iApproachTime = 0;
    this->iFadeInTime = 0;
    this->duration = 0;
    this->iDelta = 0;
    this->duration = 0;

    this->bVisible = false;
    this->bFinished = false;
    this->bBlocked = false;
    this->bMisAim = false;
    this->iAutopilotDelta = 0;
    this->bOverrideHDApproachCircle = false;
    this->bUseFadeInTimeAsApproachTime = false;

    this->iStack = 0;

    this->hitresultanim1.time = -9999.0f;
    this->hitresultanim2.time = -9999.0f;

    this->bi = beatmap;
    this->bm = dynamic_cast<Beatmap *>(beatmap);  // should be NULL if SimulatedBeatmap
}

void HitObject::draw2() {
    this->drawHitResultAnim(this->hitresultanim1);
    this->drawHitResultAnim(this->hitresultanim2);
}

void HitObject::drawHitResultAnim(const HITRESULTANIM &hitresultanim) {
    if((hitresultanim.time - cv::hitresult_duration.getFloat()) <
           engine->getTime()  // NOTE: this is written like that on purpose, don't change it ("future" results can be
                              // scheduled with it, e.g. for slider end)
       && (hitresultanim.time + cv::hitresult_duration_max.getFloat() * (1.0f / osu->getAnimationSpeedMultiplier())) >
              engine->getTime()) {
        Skin *skin = this->bm->getSkin();
        {
            const long skinAnimationTimeStartOffset =
                this->click_time +
                (hitresultanim.addObjectDurationToSkinAnimationTimeStartOffset ? this->duration : 0) +
                hitresultanim.delta;

            skin->getHit0()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit0()->setAnimationFrameClampUp();
            skin->getHit50()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit50()->setAnimationFrameClampUp();
            skin->getHit100()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit100()->setAnimationFrameClampUp();
            skin->getHit100k()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit100k()->setAnimationFrameClampUp();
            skin->getHit300()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit300()->setAnimationFrameClampUp();
            skin->getHit300g()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit300g()->setAnimationFrameClampUp();
            skin->getHit300k()->setAnimationTimeOffset(skin->getAnimationSpeed(), skinAnimationTimeStartOffset);
            skin->getHit300k()->setAnimationFrameClampUp();

            const float animPercentInv =
                1.0f - (((engine->getTime() - hitresultanim.time) * osu->getAnimationSpeedMultiplier()) /
                        cv::hitresult_duration.getFloat());

            drawHitResult(this->bm, this->bm->osuCoords2Pixels(hitresultanim.rawPos), hitresultanim.result,
                          animPercentInv,
                          std::clamp<float>((float)hitresultanim.delta / this->bi->getHitWindow50(), -1.0f, 1.0f));
        }
    }
}

void HitObject::update(long curPos, f64 /*frame_time*/) {
    this->fAlphaForApproachCircle = 0.0f;
    this->fHittableDimRGBColorMultiplierPercent = 1.0f;

    const auto mods = this->bi->getMods();

    double animationSpeedMultipler = mods.speed / osu->getAnimationSpeedMultiplier();
    this->iApproachTime = (this->bUseFadeInTimeAsApproachTime ? (GameRules::getFadeInTime() * animationSpeedMultipler)
                                                              : (long)this->bi->getApproachTime());
    this->iFadeInTime = GameRules::getFadeInTime() * animationSpeedMultipler;

    this->iDelta = this->click_time - curPos;

    // 1 ms fudge by using >=, shouldn't really be a problem
    if(curPos >= (this->click_time - this->iApproachTime) && curPos < (this->click_time + this->duration)) {
        // approach circle scale
        const float scale = std::clamp<float>((float)this->iDelta / (float)this->iApproachTime, 0.0f, 1.0f);
        this->fApproachScale = 1 + (scale * cv::approach_scale_multiplier.getFloat());
        if(cv::mod_approach_different.getBool()) {
            const float back_const = 1.70158;

            float time = 1.0f - scale;
            {
                switch(cv::mod_approach_different_style.getInt()) {
                    default:  // "Linear"
                        break;
                    case 1:  // "Gravity" / InBack
                        time = time * time * ((back_const + 1.0f) * time - back_const);
                        break;
                    case 2:  // "InOut1" / InOutCubic
                        if(time < 0.5f)
                            time = time * time * time * 4.0f;
                        else {
                            --time;
                            time = time * time * time * 4.0f + 1.0f;
                        }
                        break;
                    case 3:  // "InOut2" / InOutQuint
                        if(time < 0.5f)
                            time = time * time * time * time * time * 16.0f;
                        else {
                            --time;
                            time = time * time * time * time * time * 16.0f + 1.0f;
                        }
                        break;
                    case 4:  // "Accelerate1" / In
                        time = time * time;
                        break;
                    case 5:  // "Accelerate2" / InCubic
                        time = time * time * time;
                        break;
                    case 6:  // "Accelerate3" / InQuint
                        time = time * time * time * time * time;
                        break;
                    case 7:  // "Decelerate1" / Out
                        time = time * (2.0f - time);
                        break;
                    case 8:  // "Decelerate2" / OutCubic
                        --time;
                        time = time * time * time + 1.0f;
                        break;
                    case 9:  // "Decelerate3" / OutQuint
                        --time;
                        time = time * time * time * time * time + 1.0f;
                        break;
                }
                // NOTE: some of the easing functions will overflow/underflow, don't clamp and instead allow it on
                // purpose
            }
            this->fApproachScale = 1 + std::lerp(cv::mod_approach_different_initial_size.getFloat() - 1.0f, 0.0f, time);
        }

        // hitobject body fadein
        const long fadeInStart = this->click_time - this->iApproachTime;
        const long fadeInEnd =
            std::min(this->click_time,
                     this->click_time - this->iApproachTime +
                         this->iFadeInTime);  // std::min() ensures that the fade always finishes at click_time
                                              // (even if the fadeintime is longer than the approachtime)
        this->fAlpha =
            std::clamp<float>(1.0f - ((float)(fadeInEnd - curPos) / (float)(fadeInEnd - fadeInStart)), 0.0f, 1.0f);
        this->fAlphaWithoutHidden = this->fAlpha;

        if(ModMasks::eq(mods.flags, Replay::ModFlags::Hidden)) {
            // hidden hitobject body fadein
            const float fin_start_percent = cv::mod_hd_circle_fadein_start_percent.getFloat();
            const float fin_end_percent = cv::mod_hd_circle_fadein_end_percent.getFloat();
            const float fout_start_percent = cv::mod_hd_circle_fadeout_start_percent.getFloat();
            const float fout_end_percent = cv::mod_hd_circle_fadeout_end_percent.getFloat();
            const long hiddenFadeInStart = this->click_time - (long)(this->iApproachTime * fin_start_percent);
            const long hiddenFadeInEnd = this->click_time - (long)(this->iApproachTime * fin_end_percent);
            this->fAlpha = std::clamp<float>(
                1.0f - ((float)(hiddenFadeInEnd - curPos) / (float)(hiddenFadeInEnd - hiddenFadeInStart)), 0.0f, 1.0f);

            // hidden hitobject body fadeout
            const long hiddenFadeOutStart = this->click_time - (long)(this->iApproachTime * fout_start_percent);
            const long hiddenFadeOutEnd = this->click_time - (long)(this->iApproachTime * fout_end_percent);
            if(curPos >= hiddenFadeOutStart)
                this->fAlpha = std::clamp<float>(
                    ((float)(hiddenFadeOutEnd - curPos) / (float)(hiddenFadeOutEnd - hiddenFadeOutStart)), 0.0f, 1.0f);
        }

        // approach circle fadein (doubled fadeintime)
        const long approachCircleFadeStart = this->click_time - this->iApproachTime;
        const long approachCircleFadeEnd =
            std::min(this->click_time,
                     this->click_time - this->iApproachTime +
                         2 * this->iFadeInTime);  // std::min() ensures that the fade always finishes at click_time
                                                  // (even if the fadeintime is longer than the approachtime)
        this->fAlphaForApproachCircle = std::clamp<float>(
            1.0f - ((float)(approachCircleFadeEnd - curPos) / (float)(approachCircleFadeEnd - approachCircleFadeStart)),
            0.0f, 1.0f);

        // hittable dim, see https://github.com/ppy/osu/pull/20572
        if(cv::hitobject_hittable_dim.getBool() &&
           (!(ModMasks::eq(this->bi->getMods().flags, Replay::ModFlags::Mafham)) ||
            !cv::mod_mafham_ignore_hittable_dim.getBool())) {
            const long hittableDimFadeStart = this->click_time - (long)GameRules::getHitWindowMiss();

            // yes, this means the un-dim animation cuts into the already clickable range
            const long hittableDimFadeEnd = hittableDimFadeStart + (long)cv::hitobject_hittable_dim_duration.getInt();

            this->fHittableDimRGBColorMultiplierPercent =
                std::lerp(cv::hitobject_hittable_dim_start_percent.getFloat(), 1.0f,
                          std::clamp<float>(1.0f - (float)(hittableDimFadeEnd - curPos) /
                                                       (float)(hittableDimFadeEnd - hittableDimFadeStart),
                                            0.0f, 1.0f));
        }

        this->bVisible = true;
    } else {
        this->fApproachScale = 1.0f;
        this->bVisible = false;
    }
}

void HitObject::addHitResult(LiveScore::HIT result, long delta, bool isEndOfCombo, vec2 posRaw, float targetDelta,
                             float targetAngle, bool ignoreOnHitErrorBar, bool ignoreCombo, bool ignoreHealth,
                             bool addObjectDurationToSkinAnimationTimeStartOffset) {
    if(this->bm != nullptr && osu->getModTarget() && result != LiveScore::HIT::HIT_MISS && targetDelta >= 0.0f) {
        const float p300 = cv::mod_target_300_percent.getFloat();
        const float p100 = cv::mod_target_100_percent.getFloat();
        const float p50 = cv::mod_target_50_percent.getFloat();

        if(targetDelta < p300 && (result == LiveScore::HIT::HIT_300 || result == LiveScore::HIT::HIT_100))
            result = LiveScore::HIT::HIT_300;
        else if(targetDelta < p100)
            result = LiveScore::HIT::HIT_100;
        else if(targetDelta < p50)
            result = LiveScore::HIT::HIT_50;
        else
            result = LiveScore::HIT::HIT_MISS;

        osu->getHUD()->addTarget(targetDelta, targetAngle);
    }

    const LiveScore::HIT returnedHit = this->bi->addHitResult(this, result, delta, isEndOfCombo, ignoreOnHitErrorBar,
                                                              false, ignoreCombo, false, ignoreHealth);
    if(this->bm == nullptr) return;

    HITRESULTANIM hitresultanim;
    {
        hitresultanim.result = (returnedHit != LiveScore::HIT::HIT_MISS ? returnedHit : result);
        hitresultanim.rawPos = posRaw;
        hitresultanim.delta = delta;
        hitresultanim.time = engine->getTime();
        hitresultanim.addObjectDurationToSkinAnimationTimeStartOffset = addObjectDurationToSkinAnimationTimeStartOffset;
    }

    // currently a maximum of 2 simultaneous results are supported (for drawing, per hitobject)
    if(engine->getTime() >
       this->hitresultanim1.time + cv::hitresult_duration_max.getFloat() * (1.0f / osu->getAnimationSpeedMultiplier()))
        this->hitresultanim1 = hitresultanim;
    else
        this->hitresultanim2 = hitresultanim;
}

void HitObject::onReset(long /*curPos*/) {
    this->bMisAim = false;
    this->iAutopilotDelta = 0;

    this->hitresultanim1.time = -9999.0f;
    this->hitresultanim2.time = -9999.0f;
}

float HitObject::lerp3f(float a, float b, float c, float percent) {
    if(percent <= 0.5f)
        return std::lerp(a, b, percent * 2.0f);
    else
        return std::lerp(b, c, (percent - 0.5f) * 2.0f);
}

int Circle::rainbowNumber = 0;
int Circle::rainbowColorCounter = 0;

void Circle::drawApproachCircle(Beatmap *beatmap, vec2 rawPos, int number, int colorCounter, int colorOffset,
                                float colorRGBMultiplier, float approachScale, float alpha,
                                bool overrideHDApproachCircle) {
    rainbowNumber = number;
    rainbowColorCounter = colorCounter;

    Color comboColor = Colors::scale(beatmap->getSkin()->getComboColorForCounter(colorCounter, colorOffset),
                                     colorRGBMultiplier * cv::circle_color_saturation.getFloat());

    drawApproachCircle(beatmap->getSkin(), beatmap->osuCoords2Pixels(rawPos), comboColor, beatmap->fHitcircleDiameter,
                       approachScale, alpha, osu->getModHD(), overrideHDApproachCircle);
}

void Circle::drawCircle(Beatmap *beatmap, vec2 rawPos, int number, int colorCounter, int colorOffset,
                        float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha, bool drawNumber,
                        bool overrideHDApproachCircle) {
    drawCircle(beatmap->getSkin(), beatmap->osuCoords2Pixels(rawPos), beatmap->fHitcircleDiameter,
               beatmap->getNumberScale(), beatmap->getHitcircleOverlapScale(), number, colorCounter, colorOffset,
               colorRGBMultiplier, approachScale, alpha, numberAlpha, drawNumber, overrideHDApproachCircle);
}

void Circle::drawCircle(Skin *skin, vec2 pos, float hitcircleDiameter, float numberScale, float overlapScale,
                        int number, int colorCounter, int colorOffset, float colorRGBMultiplier,
                        float /*approachScale*/, float alpha, float numberAlpha, bool drawNumber,
                        bool /*overrideHDApproachCircle*/) {
    if(alpha <= 0.0f || !cv::draw_circles.getBool()) return;

    rainbowNumber = number;
    rainbowColorCounter = colorCounter;

    Color comboColor = Colors::scale(skin->getComboColorForCounter(colorCounter, colorOffset),
                                     colorRGBMultiplier * cv::circle_color_saturation.getFloat());

    // approach circle
    /// drawApproachCircle(skin, pos, comboColor, hitcircleDiameter, approachScale, alpha, modHD,
    /// overrideHDApproachCircle); // they are now drawn separately in draw2()

    // circle
    const float circleImageScale = hitcircleDiameter / (128.0f * (skin->isHitCircle2x() ? 2.0f : 1.0f));
    drawHitCircle(skin->getHitCircle(), pos, comboColor, circleImageScale, alpha);

    // overlay
    const float circleOverlayImageScale = hitcircleDiameter / skin->getHitCircleOverlay2()->getSizeBaseRaw().x;
    if(!skin->getHitCircleOverlayAboveNumber())
        drawHitCircleOverlay(skin->getHitCircleOverlay2(), pos, circleOverlayImageScale, alpha, colorRGBMultiplier);

    // number
    if(drawNumber) drawHitCircleNumber(skin, numberScale, overlapScale, pos, number, numberAlpha, colorRGBMultiplier);

    // overlay
    if(skin->getHitCircleOverlayAboveNumber())
        drawHitCircleOverlay(skin->getHitCircleOverlay2(), pos, circleOverlayImageScale, alpha, colorRGBMultiplier);
}

void Circle::drawCircle(Skin *skin, vec2 pos, float hitcircleDiameter, Color color, float alpha) {
    // this function is only used by the target practice heatmap

    // circle
    const float circleImageScale = hitcircleDiameter / (128.0f * (skin->isHitCircle2x() ? 2.0f : 1.0f));
    drawHitCircle(skin->getHitCircle(), pos, color, circleImageScale, alpha);

    // overlay
    const float circleOverlayImageScale = hitcircleDiameter / skin->getHitCircleOverlay2()->getSizeBaseRaw().x;
    drawHitCircleOverlay(skin->getHitCircleOverlay2(), pos, circleOverlayImageScale, alpha, 1.0f);
}

void Circle::drawSliderStartCircle(Beatmap *beatmap, vec2 rawPos, int number, int colorCounter, int colorOffset,
                                   float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha,
                                   bool drawNumber, bool overrideHDApproachCircle) {
    drawSliderStartCircle(beatmap->getSkin(), beatmap->osuCoords2Pixels(rawPos), beatmap->fHitcircleDiameter,
                          beatmap->getNumberScale(), beatmap->getHitcircleOverlapScale(), number, colorCounter,
                          colorOffset, colorRGBMultiplier, approachScale, alpha, numberAlpha, drawNumber,
                          overrideHDApproachCircle);
}

void Circle::drawSliderStartCircle(Skin *skin, vec2 pos, float hitcircleDiameter, float numberScale,
                                   float hitcircleOverlapScale, int number, int colorCounter, int colorOffset,
                                   float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha,
                                   bool drawNumber, bool overrideHDApproachCircle) {
    if(alpha <= 0.0f || !cv::draw_circles.getBool()) return;

    // if no sliderstartcircle image is preset, fallback to default circle
    if(skin->getSliderStartCircle() == skin->getMissingTexture()) {
        drawCircle(skin, pos, hitcircleDiameter, numberScale, hitcircleOverlapScale, number, colorCounter, colorOffset,
                   colorRGBMultiplier, approachScale, alpha, numberAlpha, drawNumber,
                   overrideHDApproachCircle);  // normal
        return;
    }

    rainbowNumber = number;
    rainbowColorCounter = colorCounter;

    Color comboColor = Colors::scale(skin->getComboColorForCounter(colorCounter, colorOffset),
                                     colorRGBMultiplier * cv::circle_color_saturation.getFloat());

    // circle
    const float circleImageScale = hitcircleDiameter / (128.0f * (skin->isSliderStartCircle2x() ? 2.0f : 1.0f));
    drawHitCircle(skin->getSliderStartCircle(), pos, comboColor, circleImageScale, alpha);

    // overlay
    const float circleOverlayImageScale = hitcircleDiameter / skin->getSliderStartCircleOverlay2()->getSizeBaseRaw().x;
    if(skin->getSliderStartCircleOverlay() != skin->getMissingTexture()) {
        if(!skin->getHitCircleOverlayAboveNumber())
            drawHitCircleOverlay(skin->getSliderStartCircleOverlay2(), pos, circleOverlayImageScale, alpha,
                                 colorRGBMultiplier);
    }

    // number
    if(drawNumber)
        drawHitCircleNumber(skin, numberScale, hitcircleOverlapScale, pos, number, numberAlpha, colorRGBMultiplier);

    // overlay
    if(skin->getSliderStartCircleOverlay() != skin->getMissingTexture()) {
        if(skin->getHitCircleOverlayAboveNumber())
            drawHitCircleOverlay(skin->getSliderStartCircleOverlay2(), pos, circleOverlayImageScale, alpha,
                                 colorRGBMultiplier);
    }
}

void Circle::drawSliderEndCircle(Beatmap *beatmap, vec2 rawPos, int number, int colorCounter, int colorOffset,
                                 float colorRGBMultiplier, float approachScale, float alpha, float numberAlpha,
                                 bool drawNumber, bool overrideHDApproachCircle) {
    drawSliderEndCircle(beatmap->getSkin(), beatmap->osuCoords2Pixels(rawPos), beatmap->fHitcircleDiameter,
                        beatmap->getNumberScale(), beatmap->getHitcircleOverlapScale(), number, colorCounter,
                        colorOffset, colorRGBMultiplier, approachScale, alpha, numberAlpha, drawNumber,
                        overrideHDApproachCircle);
}

void Circle::drawSliderEndCircle(Skin *skin, vec2 pos, float hitcircleDiameter, float numberScale, float overlapScale,
                                 int number, int colorCounter, int colorOffset, float colorRGBMultiplier,
                                 float approachScale, float alpha, float numberAlpha, bool drawNumber,
                                 bool overrideHDApproachCircle) {
    if(alpha <= 0.0f || !cv::slider_draw_endcircle.getBool() || !cv::draw_circles.getBool()) return;

    // if no sliderendcircle image is preset, fallback to default circle
    if(skin->getSliderEndCircle() == skin->getMissingTexture()) {
        drawCircle(skin, pos, hitcircleDiameter, numberScale, overlapScale, number, colorCounter, colorOffset,
                   colorRGBMultiplier, approachScale, alpha, numberAlpha, drawNumber, overrideHDApproachCircle);
        return;
    }

    rainbowNumber = number;
    rainbowColorCounter = colorCounter;

    Color comboColor = Colors::scale(skin->getComboColorForCounter(colorCounter, colorOffset),
                                     colorRGBMultiplier * cv::circle_color_saturation.getFloat());

    // circle
    const float circleImageScale = hitcircleDiameter / (128.0f * (skin->isSliderEndCircle2x() ? 2.0f : 1.0f));
    drawHitCircle(skin->getSliderEndCircle(), pos, comboColor, circleImageScale, alpha);

    // overlay
    if(skin->getSliderEndCircleOverlay() != skin->getMissingTexture()) {
        const float circleOverlayImageScale =
            hitcircleDiameter / skin->getSliderEndCircleOverlay2()->getSizeBaseRaw().x;
        drawHitCircleOverlay(skin->getSliderEndCircleOverlay2(), pos, circleOverlayImageScale, alpha,
                             colorRGBMultiplier);
    }
}

void Circle::drawApproachCircle(Skin *skin, vec2 pos, Color comboColor, float hitcircleDiameter, float approachScale,
                                float alpha, bool modHD, bool overrideHDApproachCircle) {
    if((!modHD || overrideHDApproachCircle) && cv::draw_approach_circles.getBool() && !cv::mod_mafham.getBool()) {
        if(approachScale > 1.0f) {
            const float approachCircleImageScale =
                hitcircleDiameter / (128.0f * (skin->isApproachCircle2x() ? 2.0f : 1.0f));

            g->setColor(comboColor);

            if(cv::circle_rainbow.getBool()) {
                float frequency = 0.3f;
                float time = engine->getTime() * 20;

                char red1 = std::sin(frequency * time + 0 + rainbowNumber * rainbowColorCounter) * 127 + 128;
                char green1 = std::sin(frequency * time + 2 + rainbowNumber * rainbowColorCounter) * 127 + 128;
                char blue1 = std::sin(frequency * time + 4 + rainbowNumber * rainbowColorCounter) * 127 + 128;

                g->setColor(argb(255, red1, green1, blue1));
            }

            g->setAlpha(alpha * cv::approach_circle_alpha_multiplier.getFloat());

            g->pushTransform();
            {
                g->scale(approachCircleImageScale * approachScale, approachCircleImageScale * approachScale);
                g->translate(pos.x, pos.y);
                g->drawImage(skin->getApproachCircle());
            }
            g->popTransform();
        }
    }
}

void Circle::drawHitCircleOverlay(SkinImage *hitCircleOverlayImage, vec2 pos, float circleOverlayImageScale,
                                  float alpha, float colorRGBMultiplier) {
    g->setColor(argb(alpha, colorRGBMultiplier, colorRGBMultiplier, colorRGBMultiplier));
    hitCircleOverlayImage->drawRaw(pos, circleOverlayImageScale);
}

void Circle::drawHitCircle(Image *hitCircleImage, vec2 pos, Color comboColor, float circleImageScale, float alpha) {
    g->setColor(comboColor);

    if(cv::circle_rainbow.getBool()) {
        float frequency = 0.3f;
        float time = engine->getTime() * 20;

        char red1 = std::sin(frequency * time + 0 + rainbowNumber * rainbowNumber * rainbowColorCounter) * 127 + 128;
        char green1 = std::sin(frequency * time + 2 + rainbowNumber * rainbowNumber * rainbowColorCounter) * 127 + 128;
        char blue1 = std::sin(frequency * time + 4 + rainbowNumber * rainbowNumber * rainbowColorCounter) * 127 + 128;

        g->setColor(argb(255, red1, green1, blue1));
    }

    g->setAlpha(alpha);

    g->pushTransform();
    {
        g->scale(circleImageScale, circleImageScale);
        g->translate(pos.x, pos.y);
        g->drawImage(hitCircleImage);
    }
    g->popTransform();
}

void Circle::drawHitCircleNumber(Skin *skin, float numberScale, float overlapScale, vec2 pos, int number,
                                 float numberAlpha, float /*colorRGBMultiplier*/) {
    if(!cv::draw_numbers.getBool()) return;

    class DigitWidth {
       public:
        static float getWidth(Skin *skin, int digit) {
            switch(digit) {
                case 0:
                    return skin->getDefault0()->getWidth();
                case 1:
                    return skin->getDefault1()->getWidth();
                case 2:
                    return skin->getDefault2()->getWidth();
                case 3:
                    return skin->getDefault3()->getWidth();
                case 4:
                    return skin->getDefault4()->getWidth();
                case 5:
                    return skin->getDefault5()->getWidth();
                case 6:
                    return skin->getDefault6()->getWidth();
                case 7:
                    return skin->getDefault7()->getWidth();
                case 8:
                    return skin->getDefault8()->getWidth();
                case 9:
                    return skin->getDefault9()->getWidth();
            }

            return skin->getDefault0()->getWidth();
        }
    };

    // generate digits
    std::vector<int> digits;
    while(number >= 10) {
        digits.push_back(number % 10);
        number = number / 10;
    }
    digits.push_back(number);

    // set color
    // g->setColor(argb(1.0f, colorRGBMultiplier, colorRGBMultiplier, colorRGBMultiplier)); // see
    // https://github.com/ppy/osu/issues/24506
    g->setColor(0xffffffff);
    if(cv::circle_number_rainbow.getBool()) {
        float frequency = 0.3f;
        float time = engine->getTime() * 20;

        char red1 =
            std::sin(frequency * time + 0 + rainbowNumber * rainbowNumber * rainbowNumber * rainbowColorCounter) * 127 +
            128;
        char green1 =
            std::sin(frequency * time + 2 + rainbowNumber * rainbowNumber * rainbowNumber * rainbowColorCounter) * 127 +
            128;
        char blue1 =
            std::sin(frequency * time + 4 + rainbowNumber * rainbowNumber * rainbowNumber * rainbowColorCounter) * 127 +
            128;

        g->setColor(argb(255, red1, green1, blue1));
    }
    g->setAlpha(numberAlpha);

    // draw digits, start at correct offset
    g->pushTransform();
    {
        g->scale(numberScale, numberScale);
        g->translate(pos.x, pos.y);
        {
            float digitWidthCombined = 0.0f;
            for(int digit : digits) {
                digitWidthCombined += DigitWidth::getWidth(skin, digit);
            }

            const int digitOverlapCount = digits.size() - 1;
            g->translate(
                -(digitWidthCombined * numberScale - skin->getHitCircleOverlap() * digitOverlapCount * overlapScale) *
                        0.5f +
                    DigitWidth::getWidth(skin, (digits.size() > 0 ? digits[digits.size() - 1] : 0)) * numberScale *
                        0.5f,
                0);
        }

        for(int i = digits.size() - 1; i >= 0; i--) {
            switch(digits[i]) {
                case 0:
                    g->drawImage(skin->getDefault0());
                    break;
                case 1:
                    g->drawImage(skin->getDefault1());
                    break;
                case 2:
                    g->drawImage(skin->getDefault2());
                    break;
                case 3:
                    g->drawImage(skin->getDefault3());
                    break;
                case 4:
                    g->drawImage(skin->getDefault4());
                    break;
                case 5:
                    g->drawImage(skin->getDefault5());
                    break;
                case 6:
                    g->drawImage(skin->getDefault6());
                    break;
                case 7:
                    g->drawImage(skin->getDefault7());
                    break;
                case 8:
                    g->drawImage(skin->getDefault8());
                    break;
                case 9:
                    g->drawImage(skin->getDefault9());
                    break;
            }

            float offset = DigitWidth::getWidth(skin, digits[i]) * numberScale;
            if(i > 0) {
                offset += DigitWidth::getWidth(skin, digits[i - 1]) * numberScale;
            }

            g->translate(offset * 0.5f - skin->getHitCircleOverlap() * overlapScale, 0);
        }
    }
    g->popTransform();
}

Circle::Circle(int x, int y, long time, HitSamples samples, int comboNumber, bool isEndOfCombo, int colorCounter,
               int colorOffset, BeatmapInterface *beatmap)
    : HitObject(time, samples, comboNumber, isEndOfCombo, colorCounter, colorOffset, beatmap) {
    this->type = HitObjectType::CIRCLE;

    this->vOriginalRawPos = vec2(x, y);
    this->vRawPos = this->vOriginalRawPos;
    this->bWaiting = false;
    this->fHitAnimation = 0.0f;
    this->fShakeAnimation = 0.0f;
}

Circle::~Circle() { this->onReset(0); }

void Circle::draw() {
    HitObject::draw();
    Skin *skin = osu->getSkin();

    // draw hit animation
    bool is_instafade = cv::instafade.getBool();
    if(!is_instafade && this->fHitAnimation > 0.0f && this->fHitAnimation != 1.0f && !osu->getModHD()) {
        float alpha = 1.0f - this->fHitAnimation;

        float scale = this->fHitAnimation;
        scale = -scale * (scale - 2.0f);  // quad out scale

        const bool drawNumber = skin->getVersion() > 1.0f ? false : true;
        const float foscale = cv::circle_fade_out_scale.getFloat();

        g->pushTransform();
        {
            g->scale((1.0f + scale * foscale), (1.0f + scale * foscale));
            skin->getHitCircleOverlay2()->setAnimationTimeOffset(
                skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk() ? this->click_time - this->iApproachTime
                                                                              : this->bm->getCurMusicPosWithOffsets());
            drawCircle(this->bm, this->vRawPos, this->combo_number, this->iColorCounter, this->iColorOffset, 1.0f, 1.0f,
                       alpha, alpha, drawNumber);
        }
        g->popTransform();
    }

    if(this->bFinished ||
       (!this->bVisible && !this->bWaiting))  // special case needed for when we are past this objects time, but still
                                              // within not-miss range, because we still need to draw the object
        return;

    // draw circle
    const bool hd = osu->getModHD();
    vec2 shakeCorrectedPos = this->vRawPos;
    if(engine->getTime() < this->fShakeAnimation && !this->bm->isInMafhamRenderChunk())  // handle note blocking shaking
    {
        float smooth = 1.0f - ((this->fShakeAnimation - engine->getTime()) /
                               cv::circle_shake_duration.getFloat());  // goes from 0 to 1
        if(smooth < 0.5f)
            smooth = smooth / 0.5f;
        else
            smooth = (1.0f - smooth) / 0.5f;
        // (now smooth goes from 0 to 1 to 0 linearly)
        smooth = -smooth * (smooth - 2);  // quad out
        smooth = -smooth * (smooth - 2);  // quad out twice
        shakeCorrectedPos.x += std::sin(engine->getTime() * 120) * smooth * cv::circle_shake_strength.getFloat();
    }
    skin->getHitCircleOverlay2()->setAnimationTimeOffset(
        skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk() ? this->click_time - this->iApproachTime
                                                                      : this->bm->getCurMusicPosWithOffsets());
    drawCircle(this->bm, shakeCorrectedPos, this->combo_number, this->iColorCounter, this->iColorOffset,
               this->fHittableDimRGBColorMultiplierPercent, this->bWaiting && !hd ? 1.0f : this->fApproachScale,
               this->bWaiting && !hd ? 1.0f : this->fAlpha, this->bWaiting && !hd ? 1.0f : this->fAlpha, true,
               this->bOverrideHDApproachCircle);
}

void Circle::draw2() {
    HitObject::draw2();
    if(this->bFinished || (!this->bVisible && !this->bWaiting))
        return;  // special case needed for when we are past this objects time, but still within not-miss range, because
                 // we still need to draw the object

    // draw approach circle
    const bool hd = osu->getModHD();

    // HACKHACK: don't fucking change this piece of code here, it fixes a heisenbug
    // (https://github.com/McKay42/McOsu/issues/165)
    if(cv::bug_flicker_log.getBool()) {
        const float approachCircleImageScale =
            this->bm->fHitcircleDiameter / (128.0f * (this->bm->getSkin()->isApproachCircle2x() ? 2.0f : 1.0f));
        debugLog("click_time = {:d}, aScale = {:f}, iScale = {:f}\n", click_time, this->fApproachScale,
                 approachCircleImageScale);
    }

    drawApproachCircle(this->bm, this->vRawPos, this->combo_number, this->iColorCounter, this->iColorOffset,
                       this->fHittableDimRGBColorMultiplierPercent, this->bWaiting && !hd ? 1.0f : this->fApproachScale,
                       this->bWaiting && !hd ? 1.0f : this->fAlphaForApproachCircle, this->bOverrideHDApproachCircle);
}

void Circle::update(long curPos, f64 frame_time) {
    HitObject::update(curPos, frame_time);
    if(this->bFinished) return;

    const auto mods = this->bi->getMods();
    const long delta = curPos - this->click_time;

    if(ModMasks::eq(mods.flags, Replay::ModFlags::Autoplay)) {
        if(curPos >= this->click_time) {
            this->onHit(LiveScore::HIT::HIT_300, 0);
        }
        return;
    }

    if(ModMasks::eq(mods.flags, Replay::ModFlags::Relax)) {
        if(curPos >= this->click_time + (long)cv::relax_offset.getInt() && !this->bi->isPaused() &&
           !this->bi->isContinueScheduled()) {
            const vec2 pos = this->bi->osuCoords2Pixels(this->vRawPos);
            const float cursorDelta = vec::length(this->bi->getCursorPos() - pos);
            if((cursorDelta < this->bi->fHitcircleDiameter / 2.0f &&
                (this->bi->getModsLegacy() & LegacyFlags::Relax))) {
                LiveScore::HIT result = this->bi->getHitResult(delta);

                if(result != LiveScore::HIT::HIT_NULL) {
                    const float targetDelta = cursorDelta / (this->bi->fHitcircleDiameter / 2.0f);
                    const float targetAngle = glm::degrees(
                        std::atan2(this->bi->getCursorPos().y - pos.y, this->bi->getCursorPos().x - pos.x));

                    this->onHit(result, delta, targetDelta, targetAngle);
                }
            }
        }
    }

    if(delta >= 0) {
        this->bWaiting = true;

        // if this is a miss after waiting
        if(delta > (long)this->bi->getHitWindow50()) {
            this->onHit(LiveScore::HIT::HIT_MISS, delta);
        }
    } else {
        this->bWaiting = false;
    }
}

void Circle::updateStackPosition(float stackOffset) {
    this->vRawPos =
        this->vOriginalRawPos -
        vec2(this->iStack * stackOffset,
             this->iStack * stackOffset * ((this->bi->getModsLegacy() & LegacyFlags::HardRock) ? -1.0f : 1.0f));
}

void Circle::miss(long curPos) {
    if(this->bFinished) return;

    const long delta = curPos - this->click_time;

    this->onHit(LiveScore::HIT::HIT_MISS, delta);
}

void Circle::onClickEvent(std::vector<Click> &clicks) {
    if(this->bFinished) return;

    const vec2 cursorPos = clicks[0].pos;
    const vec2 pos = this->bi->osuCoords2Pixels(this->vRawPos);
    const float cursorDelta = vec::length(cursorPos - pos);

    if(cursorDelta < this->bi->fHitcircleDiameter / 2.0f) {
        // note blocking & shake
        if(this->bBlocked) {
            this->fShakeAnimation = engine->getTime() + cv::circle_shake_duration.getFloat();
            return;  // ignore click event completely
        }

        const long delta = clicks[0].click_time - (long)this->click_time;

        LiveScore::HIT result = this->bi->getHitResult(delta);
        if(result != LiveScore::HIT::HIT_NULL) {
            const float targetDelta = cursorDelta / (this->bi->fHitcircleDiameter / 2.0f);
            const float targetAngle = glm::degrees(std::atan2(cursorPos.y - pos.y, cursorPos.x - pos.x));

            clicks.erase(clicks.begin());
            this->onHit(result, delta, targetDelta, targetAngle);
        }
    }
}

void Circle::onHit(LiveScore::HIT result, long delta, float targetDelta, float targetAngle) {
    // sound and hit animation
    if(this->bm != nullptr && result != LiveScore::HIT::HIT_MISS) {
        const vec2 osuCoords = this->bm->pixels2OsuCoords(this->bm->osuCoords2Pixels(this->vRawPos));
        f32 pan = GameRules::osuCoords2Pan(osuCoords.x);
        this->samples.play(pan, delta);

        this->fHitAnimation = 0.001f;  // quickfix for 1 frame missing images
        anim->moveQuadOut(&this->fHitAnimation, 1.0f, GameRules::getFadeOutTime(this->bm), true);
    }

    // add it, and we are finished
    this->addHitResult(result, delta, this->is_end_of_combo, this->vRawPos, targetDelta, targetAngle);
    this->bFinished = true;
}

void Circle::onReset(long curPos) {
    HitObject::onReset(curPos);

    this->bWaiting = false;
    this->fShakeAnimation = 0.0f;

    if(this->bm != nullptr) {
        anim->deleteExistingAnimation(&this->fHitAnimation);
    }

    if(this->click_time > curPos) {
        this->bFinished = false;
        this->fHitAnimation = 0.0f;
    } else {
        this->bFinished = true;
        this->fHitAnimation = 1.0f;
    }
}

vec2 Circle::getAutoCursorPos(long /*curPos*/) const { return this->bi->osuCoords2Pixels(this->vRawPos); }

Slider::Slider(char stype, int repeat, float pixelLength, std::vector<vec2> points, std::vector<float> ticks,
               float sliderTime, float sliderTimeWithoutRepeats, long time, HitSamples hoverSamples,
               std::vector<HitSamples> edgeSamples, int comboNumber, bool isEndOfCombo, int colorCounter,
               int colorOffset, BeatmapInterface *beatmap)
    : HitObject(time, hoverSamples, comboNumber, isEndOfCombo, colorCounter, colorOffset, beatmap) {
    this->type = HitObjectType::SLIDER;

    this->cType = stype;
    this->iRepeat = repeat;
    this->fPixelLength = pixelLength;
    this->points = std::move(points);
    this->edgeSamples = std::move(edgeSamples);
    this->fSliderTime = sliderTime;
    this->fSliderTimeWithoutRepeats = sliderTimeWithoutRepeats;

    // build raw ticks
    for(float tick : ticks) {
        SLIDERTICK st;
        st.finished = false;
        st.percent = tick;
        this->ticks.push_back(st);
    }

    // build curve
    this->curve = SliderCurve::createCurve(this->cType, this->points, this->fPixelLength);

    // build repeats
    for(int i = 0; i < (this->iRepeat - 1); i++) {
        SLIDERCLICK sc;

        sc.finished = false;
        sc.successful = false;
        sc.type = 0;
        sc.sliderend = ((i % 2) == 0);  // for hit animation on repeat hit
        sc.time = this->click_time + (long)(this->fSliderTimeWithoutRepeats * (i + 1));

        this->clicks.push_back(sc);
    }

    // build ticks
    for(int i = 0; i < this->iRepeat; i++) {
        for(int t = 0; t < this->ticks.size(); t++) {
            // NOTE: repeat ticks are not necessarily symmetric.
            //
            // e.g. this slider: [1]=======*==[2]
            //
            // the '*' is where the tick is, let's say percent = 0.75
            // on repeat 0, the tick is at: click_time + 0.75*m_fSliderTimeWithoutRepeats
            // but on repeat 1, the tick is at: click_time + 1*m_fSliderTimeWithoutRepeats + (1.0 -
            // 0.75)*m_fSliderTimeWithoutRepeats this gives better readability at the cost of invalid rhythms: ticks are
            // guaranteed to always be at the same position, even in repeats so, depending on which repeat we are in
            // (even or odd), we either do (percent) or (1.0 - percent)

            const float tickPercentRelativeToRepeatFromStartAbs =
                (((i + 1) % 2) != 0 ? this->ticks[t].percent : 1.0f - this->ticks[t].percent);

            SLIDERCLICK sc;

            sc.time = this->click_time + (long)(this->fSliderTimeWithoutRepeats * i) +
                      (long)(tickPercentRelativeToRepeatFromStartAbs * this->fSliderTimeWithoutRepeats);
            sc.finished = false;
            sc.successful = false;
            sc.type = 1;
            sc.tickIndex = t;

            this->clicks.push_back(sc);
        }
    }

    this->duration = (long)this->fSliderTime;
    this->duration = this->duration >= 0 ? this->duration : 1;  // force clamp to positive range

    this->fSlidePercent = 0.0f;
    this->fActualSlidePercent = 0.0f;
    this->fSliderSnakePercent = 0.0f;
    this->fReverseArrowAlpha = 0.0f;
    this->fBodyAlpha = 0.0f;

    this->startResult = LiveScore::HIT::HIT_NULL;
    this->endResult = LiveScore::HIT::HIT_NULL;
    this->bStartFinished = false;
    this->fStartHitAnimation = 0.0f;
    this->bEndFinished = false;
    this->fEndHitAnimation = 0.0f;
    this->fEndSliderBodyFadeAnimation = 0.0f;
    this->iStrictTrackingModLastClickHeldTime = 0;
    this->iFatFingerKey = 0;
    this->bCursorLeft = true;
    this->bCursorInside = false;
    this->bHeldTillEnd = false;
    this->bHeldTillEndForLenienceHack = false;
    this->bHeldTillEndForLenienceHackCheck = false;
    this->fFollowCircleTickAnimationScale = 0.0f;
    this->fFollowCircleAnimationScale = 0.0f;
    this->fFollowCircleAnimationAlpha = 0.0f;

    this->iReverseArrowPos = 0;
    this->iCurRepeat = 0;
    this->iCurRepeatCounterForHitSounds = 0;
    this->bInReverse = false;
    this->bHideNumberAfterFirstRepeatHit = false;

    this->vao = nullptr;
}

Slider::~Slider() {
    this->onReset(0);

    SAFE_DELETE(this->curve);
    SAFE_DELETE(this->vao);
}

void Slider::draw() {
    if(this->points.size() <= 0) return;

    const float foscale = cv::circle_fade_out_scale.getFloat();
    Skin *skin = this->bm->getSkin();

    const bool isCompletelyFinished = this->bStartFinished && this->bEndFinished && this->bFinished;

    if((this->bVisible || (this->bStartFinished && !this->bFinished)) &&
       !isCompletelyFinished)  // extra possibility to avoid flicker between HitObject::m_bVisible delay and the
                               // fadeout animation below this if block
    {
        float alpha = (cv::mod_hd_slider_fast_fade.getBool() ? this->fAlpha : this->fBodyAlpha);
        float sliderSnake = cv::snaking_sliders.getBool() ? this->fSliderSnakePercent : 1.0f;

        // shrinking sliders
        float sliderSnakeStart = 0.0f;
        if(cv::slider_shrink.getBool() && this->iReverseArrowPos == 0) {
            sliderSnakeStart = (this->bInReverse ? 0.0f : this->fSlidePercent);
            if(this->bInReverse) sliderSnake = this->fSlidePercent;
        }

        // draw slider body
        if(alpha > 0.0f && cv::slider_draw_body.getBool()) this->drawBody(alpha, sliderSnakeStart, sliderSnake);

        // draw slider ticks
        Color tickColor = 0xffffffff;
        tickColor = Colors::scale(tickColor, this->fHittableDimRGBColorMultiplierPercent);
        const float tickImageScale =
            (this->bm->fHitcircleDiameter / (16.0f * (skin->isSliderScorePoint2x() ? 2.0f : 1.0f))) * 0.125f;
        for(auto &tick : this->ticks) {
            if(tick.finished || tick.percent > sliderSnake) continue;

            vec2 pos = this->bm->osuCoords2Pixels(this->curve->pointAt(tick.percent));

            g->setColor(Color(tickColor).setA(alpha));

            g->pushTransform();
            {
                g->scale(tickImageScale, tickImageScale);
                g->translate(pos.x, pos.y);
                g->drawImage(skin->getSliderScorePoint());
            }
            g->popTransform();
        }

        // draw start & end circle & reverse arrows
        if(this->points.size() > 1) {
            // HACKHACK: very dirty code
            bool sliderRepeatStartCircleFinished = (this->iRepeat < 2);
            bool sliderRepeatEndCircleFinished = false;
            bool endCircleIsAtActualSliderEnd = true;
            for(auto &click : this->clicks) {
                // repeats
                if(click.type == 0) {
                    endCircleIsAtActualSliderEnd = click.sliderend;

                    if(endCircleIsAtActualSliderEnd)
                        sliderRepeatEndCircleFinished = click.finished;
                    else
                        sliderRepeatStartCircleFinished = click.finished;
                }
            }

            const bool ifStrictTrackingModShouldDrawEndCircle =
                (!cv::mod_strict_tracking.getBool() || this->endResult != LiveScore::HIT::HIT_MISS);

            // end circle
            if((!this->bEndFinished && this->iRepeat % 2 != 0 && ifStrictTrackingModShouldDrawEndCircle) ||
               (!sliderRepeatEndCircleFinished &&
                (ifStrictTrackingModShouldDrawEndCircle || (this->iRepeat > 1 && endCircleIsAtActualSliderEnd) ||
                 (this->iRepeat > 1 && std::abs(this->iRepeat - this->iCurRepeat) > 2))))
                this->drawEndCircle(alpha, sliderSnake);

            // start circle
            if(!this->bStartFinished ||
               (!sliderRepeatStartCircleFinished &&
                (ifStrictTrackingModShouldDrawEndCircle || (this->iRepeat > 1 && !endCircleIsAtActualSliderEnd) ||
                 (this->iRepeat > 1 && std::abs(this->iRepeat - this->iCurRepeat) > 2))) ||
               (!this->bEndFinished && this->iRepeat % 2 == 0 && ifStrictTrackingModShouldDrawEndCircle))
                this->drawStartCircle(alpha);

            // reverse arrows
            if(this->fReverseArrowAlpha > 0.0f) {
                // if the combo color is nearly white, blacken the reverse arrow
                Color comboColor = skin->getComboColorForCounter(this->iColorCounter, this->iColorOffset);
                Color reverseArrowColor = 0xffffffff;
                if((comboColor.Rf() + comboColor.Gf() + comboColor.Bf()) / 3.0f >
                   cv::slider_reverse_arrow_black_threshold.getFloat())
                    reverseArrowColor = 0xff000000;

                reverseArrowColor = Colors::scale(reverseArrowColor, this->fHittableDimRGBColorMultiplierPercent);

                float div = 0.30f;
                float pulse = (div - std::fmod(std::abs(this->bm->getCurMusicPos()) / 1000.0f, div)) / div;
                pulse *= pulse;  // quad in

                if(!cv::slider_reverse_arrow_animated.getBool() || this->bm->isInMafhamRenderChunk()) pulse = 0.0f;

                // end
                if(this->iReverseArrowPos == 2 || this->iReverseArrowPos == 3) {
                    vec2 pos = this->bm->osuCoords2Pixels(this->curve->pointAt(1.0f));
                    float rotation = this->curve->getEndAngle() - cv::playfield_rotation.getFloat() -
                                     this->bm->getPlayfieldRotation();
                    if((this->bm->getModsLegacy() & LegacyFlags::HardRock)) rotation = 360.0f - rotation;
                    if(cv::playfield_mirror_horizontal.getBool()) rotation = 360.0f - rotation;
                    if(cv::playfield_mirror_vertical.getBool()) rotation = 180.0f - rotation;

                    const float osuCoordScaleMultiplier =
                        this->bm->fHitcircleDiameter / this->bm->fRawHitcircleDiameter;
                    float reverseArrowImageScale =
                        (this->bm->fRawHitcircleDiameter / (128.0f * (skin->isReverseArrow2x() ? 2.0f : 1.0f))) *
                        osuCoordScaleMultiplier;

                    reverseArrowImageScale *= 1.0f + pulse * 0.30f;

                    g->setColor(Color(reverseArrowColor).setA(this->fReverseArrowAlpha));

                    g->pushTransform();
                    {
                        g->rotate(rotation);
                        g->scale(reverseArrowImageScale, reverseArrowImageScale);
                        g->translate(pos.x, pos.y);
                        g->drawImage(skin->getReverseArrow());
                    }
                    g->popTransform();
                }

                // start
                if(this->iReverseArrowPos == 1 || this->iReverseArrowPos == 3) {
                    vec2 pos = this->bm->osuCoords2Pixels(this->curve->pointAt(0.0f));
                    float rotation = this->curve->getStartAngle() - cv::playfield_rotation.getFloat() -
                                     this->bm->getPlayfieldRotation();
                    if((this->bm->getModsLegacy() & LegacyFlags::HardRock)) rotation = 360.0f - rotation;
                    if(cv::playfield_mirror_horizontal.getBool()) rotation = 360.0f - rotation;
                    if(cv::playfield_mirror_vertical.getBool()) rotation = 180.0f - rotation;

                    const float osuCoordScaleMultiplier =
                        this->bm->fHitcircleDiameter / this->bm->fRawHitcircleDiameter;
                    float reverseArrowImageScale =
                        (this->bm->fRawHitcircleDiameter / (128.0f * (skin->isReverseArrow2x() ? 2.0f : 1.0f))) *
                        osuCoordScaleMultiplier;

                    reverseArrowImageScale *= 1.0f + pulse * 0.30f;

                    g->setColor(Color(reverseArrowColor).setA(this->fReverseArrowAlpha));

                    g->pushTransform();
                    {
                        g->rotate(rotation);
                        g->scale(reverseArrowImageScale, reverseArrowImageScale);
                        g->translate(pos.x, pos.y);
                        g->drawImage(skin->getReverseArrow());
                    }
                    g->popTransform();
                }
            }
        }
    }

    // slider body fade animation, draw start/end circle hit animation
    bool instafade_slider_body = cv::instafade_sliders.getBool();
    bool instafade_slider_head = cv::instafade.getBool();
    if(!instafade_slider_body && this->fEndSliderBodyFadeAnimation > 0.0f &&
       this->fEndSliderBodyFadeAnimation != 1.0f && !(this->bm->getModsLegacy() & LegacyFlags::Hidden)) {
        std::vector<vec2> emptyVector;
        std::vector<vec2> alwaysPoints;
        alwaysPoints.push_back(this->bm->osuCoords2Pixels(this->curve->pointAt(this->fSlidePercent)));
        if(!cv::slider_shrink.getBool())
            this->drawBody(1.0f - this->fEndSliderBodyFadeAnimation, 0, 1);
        else if(cv::slider_body_lazer_fadeout_style.getBool())
            SliderRenderer::draw(emptyVector, alwaysPoints, this->bm->fHitcircleDiameter, 0.0f, 0.0f,
                                 this->bm->getSkin()->getComboColorForCounter(this->iColorCounter, this->iColorOffset),
                                 1.0f, 1.0f - this->fEndSliderBodyFadeAnimation, this->click_time);
    }

    if(!instafade_slider_head && cv::slider_sliderhead_fadeout.getBool() && this->fStartHitAnimation > 0.0f &&
       this->fStartHitAnimation != 1.0f && !(this->bm->getModsLegacy() & LegacyFlags::Hidden)) {
        float alpha = 1.0f - this->fStartHitAnimation;

        float scale = this->fStartHitAnimation;
        scale = -scale * (scale - 2.0f);  // quad out scale

        bool drawNumber = (skin->getVersion() > 1.0f ? false : true) && this->iCurRepeat < 1;

        g->pushTransform();
        {
            g->scale((1.0f + scale * foscale), (1.0f + scale * foscale));
            if(this->iCurRepeat < 1) {
                skin->getHitCircleOverlay2()->setAnimationTimeOffset(skin->getAnimationSpeed(),
                                                                     !this->bm->isInMafhamRenderChunk()
                                                                         ? this->click_time - this->iApproachTime
                                                                         : this->bm->getCurMusicPosWithOffsets());
                skin->getSliderStartCircleOverlay2()->setAnimationTimeOffset(
                    skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk()
                                                   ? this->click_time - this->iApproachTime
                                                   : this->bm->getCurMusicPosWithOffsets());

                Circle::drawSliderStartCircle(this->bm, this->curve->pointAt(0.0f), this->combo_number,
                                              this->iColorCounter, this->iColorOffset, 1.0f, 1.0f, alpha, alpha,
                                              drawNumber);
            } else {
                skin->getHitCircleOverlay2()->setAnimationTimeOffset(
                    skin->getAnimationSpeed(),
                    !this->bm->isInMafhamRenderChunk() ? this->click_time : this->bm->getCurMusicPosWithOffsets());
                skin->getSliderEndCircleOverlay2()->setAnimationTimeOffset(
                    skin->getAnimationSpeed(),
                    !this->bm->isInMafhamRenderChunk() ? this->click_time : this->bm->getCurMusicPosWithOffsets());

                Circle::drawSliderEndCircle(this->bm, this->curve->pointAt(0.0f), this->combo_number,
                                            this->iColorCounter, this->iColorOffset, 1.0f, 1.0f, alpha, alpha,
                                            drawNumber);
            }
        }
        g->popTransform();
    }

    if(!instafade_slider_head && this->fEndHitAnimation > 0.0f && this->fEndHitAnimation != 1.0f &&
       !(this->bm->getModsLegacy() & LegacyFlags::Hidden)) {
        float alpha = 1.0f - this->fEndHitAnimation;

        float scale = this->fEndHitAnimation;
        scale = -scale * (scale - 2.0f);  // quad out scale

        g->pushTransform();
        {
            g->scale((1.0f + scale * foscale), (1.0f + scale * foscale));
            {
                skin->getHitCircleOverlay2()->setAnimationTimeOffset(skin->getAnimationSpeed(),
                                                                     !this->bm->isInMafhamRenderChunk()
                                                                         ? this->click_time - this->iFadeInTime
                                                                         : this->bm->getCurMusicPosWithOffsets());
                skin->getSliderEndCircleOverlay2()->setAnimationTimeOffset(skin->getAnimationSpeed(),
                                                                           !this->bm->isInMafhamRenderChunk()
                                                                               ? this->click_time - this->iFadeInTime
                                                                               : this->bm->getCurMusicPosWithOffsets());

                Circle::drawSliderEndCircle(this->bm, this->curve->pointAt(1.0f), this->combo_number,
                                            this->iColorCounter, this->iColorOffset, 1.0f, 1.0f, alpha, 0.0f, false);
            }
        }
        g->popTransform();
    }

    HitObject::draw();
}

void Slider::draw2() { this->draw2(true, false); }

void Slider::draw2(bool drawApproachCircle, bool drawOnlyApproachCircle) {
    HitObject::draw2();

    Skin *skin = this->bm->getSkin();

    // HACKHACK: so much code duplication aaaaaaah
    if((this->bVisible || (this->bStartFinished && !this->bFinished)) &&
       drawApproachCircle)  // extra possibility to avoid flicker between HitObject::m_bVisible delay and the fadeout
                            // animation below this if block
    {
        if(this->points.size() > 1) {
            // HACKHACK: very dirty code
            bool sliderRepeatStartCircleFinished = this->iRepeat < 2;
            for(auto &click : this->clicks) {
                if(click.type == 0) {
                    if(!click.sliderend) sliderRepeatStartCircleFinished = click.finished;
                }
            }

            // start circle
            if(!this->bStartFinished || !sliderRepeatStartCircleFinished ||
               (!this->bEndFinished && this->iRepeat % 2 == 0)) {
                Circle::drawApproachCircle(this->bm, this->curve->pointAt(0.0f), this->combo_number,
                                           this->iColorCounter, this->iColorOffset,
                                           this->fHittableDimRGBColorMultiplierPercent, this->fApproachScale,
                                           this->fAlphaForApproachCircle, this->bOverrideHDApproachCircle);
            }
        }
    }

    if(drawApproachCircle && drawOnlyApproachCircle) return;

    // draw followcircle
    // HACKHACK: this is not entirely correct (due to m_bHeldTillEnd, if held within 300 range but then released, will
    // flash followcircle at the end)
    bool is_holding_click = this->isClickHeldSlider();
    is_holding_click |= ModMasks::legacy_eq(this->bm->getModsLegacy(), LegacyFlags::Autoplay);
    is_holding_click |= ModMasks::legacy_eq(this->bm->getModsLegacy(), LegacyFlags::Relax);

    bool should_draw_followcircle = (this->bVisible && this->bCursorInside && is_holding_click);
    should_draw_followcircle |= (this->bFinished && this->fFollowCircleAnimationAlpha > 0.0f && this->bHeldTillEnd);

    if(should_draw_followcircle) {
        vec2 point = this->bm->osuCoords2Pixels(this->vCurPointRaw);

        // HACKHACK: this is shit
        float tickAnimation =
            (this->fFollowCircleTickAnimationScale < 0.1f ? this->fFollowCircleTickAnimationScale / 0.1f
                                                          : (1.0f - this->fFollowCircleTickAnimationScale) / 0.9f);
        if(this->fFollowCircleTickAnimationScale < 0.1f) {
            tickAnimation = -tickAnimation * (tickAnimation - 2.0f);
            tickAnimation = std::clamp<float>(tickAnimation / 0.02f, 0.0f, 1.0f);
        }
        float tickAnimationScale = 1.0f + tickAnimation * cv::slider_followcircle_tick_pulse_scale.getFloat();

        g->setColor(Color(0xffffffff).setA(this->fFollowCircleAnimationAlpha));

        skin->getSliderFollowCircle2()->setAnimationTimeOffset(skin->getAnimationSpeed(), this->click_time);
        skin->getSliderFollowCircle2()->drawRaw(
            point,
            (this->bm->fSliderFollowCircleDiameter / skin->getSliderFollowCircle2()->getSizeBaseRaw().x) *
                tickAnimationScale * this->fFollowCircleAnimationScale *
                0.85f);  // this is a bit strange, but seems to work perfectly with 0.85
    }

    const bool isCompletelyFinished = this->bStartFinished && this->bEndFinished && this->bFinished;

    // draw sliderb on top of everything
    if((this->bVisible || (this->bStartFinished && !this->bFinished)) &&
       !isCompletelyFinished)  // extra possibility in the if-block to avoid flicker between HitObject::m_bVisible
                               // delay and the fadeout animation below this if-block
    {
        if(this->fSlidePercent > 0.0f) {
            // draw sliderb
            vec2 point = this->bm->osuCoords2Pixels(this->vCurPointRaw);
            vec2 c1 = this->bm->osuCoords2Pixels(this->curve->pointAt(
                this->fSlidePercent + 0.01f <= 1.0f ? this->fSlidePercent : this->fSlidePercent - 0.01f));
            vec2 c2 = this->bm->osuCoords2Pixels(this->curve->pointAt(
                this->fSlidePercent + 0.01f <= 1.0f ? this->fSlidePercent + 0.01f : this->fSlidePercent));
            float ballAngle = glm::degrees(std::atan2(c2.y - c1.y, c2.x - c1.x));
            if(skin->getSliderBallFlip()) ballAngle += (this->iCurRepeat % 2 == 0) ? 0 : 180;

            g->setColor(skin->getAllowSliderBallTint()
                            ? (cv::slider_ball_tint_combo_color.getBool()
                                   ? skin->getComboColorForCounter(this->iColorCounter, this->iColorOffset)
                                   : skin->getSliderBallColor())
                            : rgb(255, 255, 255));
            g->pushTransform();
            {
                g->rotate(ballAngle);
                skin->getSliderb()->setAnimationTimeOffset(skin->getAnimationSpeed(), this->click_time);
                skin->getSliderb()->drawRaw(point,
                                            this->bm->fHitcircleDiameter / skin->getSliderb()->getSizeBaseRaw().x);
            }
            g->popTransform();
        }
    }
}

void Slider::drawStartCircle(float /*alpha*/) {
    Skin *skin = this->bm->getSkin();

    if(this->bStartFinished) {
        skin->getHitCircleOverlay2()->setAnimationTimeOffset(
            skin->getAnimationSpeed(),
            !this->bm->isInMafhamRenderChunk() ? this->click_time : this->bm->getCurMusicPosWithOffsets());
        skin->getSliderEndCircleOverlay2()->setAnimationTimeOffset(
            skin->getAnimationSpeed(),
            !this->bm->isInMafhamRenderChunk() ? this->click_time : this->bm->getCurMusicPosWithOffsets());

        Circle::drawSliderEndCircle(this->bm, this->curve->pointAt(0.0f), this->combo_number, this->iColorCounter,
                                    this->iColorOffset, this->fHittableDimRGBColorMultiplierPercent, 1.0f, this->fAlpha,
                                    0.0f, false, false);
    } else {
        skin->getHitCircleOverlay2()->setAnimationTimeOffset(
            skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk() ? this->click_time - this->iApproachTime
                                                                          : this->bm->getCurMusicPosWithOffsets());
        skin->getSliderStartCircleOverlay2()->setAnimationTimeOffset(
            skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk() ? this->click_time - this->iApproachTime
                                                                          : this->bm->getCurMusicPosWithOffsets());

        Circle::drawSliderStartCircle(this->bm, this->curve->pointAt(0.0f), this->combo_number, this->iColorCounter,
                                      this->iColorOffset, this->fHittableDimRGBColorMultiplierPercent,
                                      this->fApproachScale, this->fAlpha, this->fAlpha,
                                      !this->bHideNumberAfterFirstRepeatHit, this->bOverrideHDApproachCircle);
    }
}

void Slider::drawEndCircle(float /*alpha*/, float sliderSnake) {
    Skin *skin = this->bm->getSkin();

    skin->getHitCircleOverlay2()->setAnimationTimeOffset(
        skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk() ? this->click_time - this->iFadeInTime
                                                                      : this->bm->getCurMusicPosWithOffsets());
    skin->getSliderEndCircleOverlay2()->setAnimationTimeOffset(
        skin->getAnimationSpeed(), !this->bm->isInMafhamRenderChunk() ? this->click_time - this->iFadeInTime
                                                                      : this->bm->getCurMusicPosWithOffsets());

    Circle::drawSliderEndCircle(this->bm, this->curve->pointAt(sliderSnake), this->combo_number, this->iColorCounter,
                                this->iColorOffset, this->fHittableDimRGBColorMultiplierPercent, 1.0f, this->fAlpha,
                                0.0f, false, false);
}

void Slider::drawBody(float alpha, float from, float to) {
    // smooth begin/end while snaking/shrinking
    std::vector<vec2> alwaysPoints;
    if(cv::slider_body_smoothsnake.getBool()) {
        if(cv::slider_shrink.getBool() && this->fSliderSnakePercent > 0.999f) {
            alwaysPoints.push_back(this->bm->osuCoords2Pixels(this->curve->pointAt(this->fSlidePercent)));  // curpoint
            alwaysPoints.push_back(this->bm->osuCoords2Pixels(this->getRawPosAt(
                this->click_time + this->duration + 1)));  // endpoint (because setDrawPercent() causes the last
                                                           // circle mesh to become invisible too quickly)
        }
        if(cv::snaking_sliders.getBool() && this->fSliderSnakePercent < 1.0f)
            alwaysPoints.push_back(this->bm->osuCoords2Pixels(
                this->curve->pointAt(this->fSliderSnakePercent)));  // snakeoutpoint (only while snaking out)
    }

    const Color undimmedComboColor =
        this->bm->getSkin()->getComboColorForCounter(this->iColorCounter, this->iColorOffset);

    if(osu->shouldFallBackToLegacySliderRenderer()) {
        std::vector<vec2> screenPoints = this->curve->getPoints();
        for(auto &screenPoint : screenPoints) {
            screenPoint = this->bm->osuCoords2Pixels(screenPoint);
        }

        // peppy sliders
        SliderRenderer::draw(screenPoints, alwaysPoints, this->bm->fHitcircleDiameter, from, to, undimmedComboColor,
                             this->fHittableDimRGBColorMultiplierPercent, alpha, this->click_time);
    } else {
        // vertex buffered sliders
        // as the base mesh is centered at (0, 0, 0) and in raw osu coordinates, we have to scale and translate it to
        // make it fit the actual desktop playfield
        const float scale = GameRules::getPlayfieldScaleFactor();
        vec2 translation = GameRules::getPlayfieldCenter();

        if(this->bm->hasFailed())
            translation =
                this->bm->osuCoords2Pixels(vec2(GameRules::OSU_COORD_WIDTH / 2, GameRules::OSU_COORD_HEIGHT / 2));

        if(cv::mod_fps.getBool()) translation += this->bm->getFirstPersonCursorDelta();

        SliderRenderer::draw(this->vao, alwaysPoints, translation, scale, this->bm->fHitcircleDiameter, from, to,
                             undimmedComboColor, this->fHittableDimRGBColorMultiplierPercent, alpha, this->click_time);
    }
}

void Slider::update(long curPos, f64 frame_time) {
    HitObject::update(curPos, frame_time);

    if(this->bm != nullptr) {
        // stop slide sound while paused
        if(this->bm->isPaused() || !this->bm->isPlaying() || this->bm->hasFailed()) {
            this->samples.stop();
        }

        // animations must be updated even if we are finished
        this->updateAnimations(curPos);
    }

    // all further calculations are only done while we are active
    if(this->bFinished) return;

    // slider slide percent
    this->fSlidePercent = 0.0f;
    if(curPos > this->click_time)
        this->fSlidePercent = std::clamp<float>(
            std::clamp<long>((curPos - (this->click_time)), 0, (long)this->fSliderTime) / this->fSliderTime, 0.0f,
            1.0f);

    this->fActualSlidePercent = this->fSlidePercent;

    const float sliderSnakeDuration =
        (1.0f / 3.0f) * this->iApproachTime * cv::slider_snake_duration_multiplier.getFloat();
    this->fSliderSnakePercent =
        std::min(1.0f, (curPos - (this->click_time - this->iApproachTime)) / (sliderSnakeDuration));

    const long reverseArrowFadeInStart =
        this->click_time -
        (cv::snaking_sliders.getBool() ? (this->iApproachTime - sliderSnakeDuration) : this->iApproachTime);
    const long reverseArrowFadeInEnd = reverseArrowFadeInStart + cv::slider_reverse_arrow_fadein_duration.getInt();
    this->fReverseArrowAlpha = 1.0f - std::clamp<float>(((float)(reverseArrowFadeInEnd - curPos) /
                                                         (float)(reverseArrowFadeInEnd - reverseArrowFadeInStart)),
                                                        0.0f, 1.0f);
    this->fReverseArrowAlpha *= cv::slider_reverse_arrow_alpha_multiplier.getFloat();

    this->fBodyAlpha = this->fAlpha;
    if((this->bi->getModsLegacy() & LegacyFlags::Hidden))  // hidden modifies the body alpha
    {
        this->fBodyAlpha = this->fAlphaWithoutHidden;  // fade in as usual

        // fade out over the duration of the slider, starting exactly when the default fadein finishes
        const long hiddenSliderBodyFadeOutStart =
            std::min(this->click_time,
                     this->click_time - this->iApproachTime +
                         this->iFadeInTime);  // std::min() ensures that the fade always starts at click_time
                                              // (even if the fadeintime is longer than the approachtime)
        const float fade_percent = cv::mod_hd_slider_fade_percent.getFloat();
        const long hiddenSliderBodyFadeOutEnd = this->click_time + (long)(fade_percent * this->fSliderTime);
        if(curPos >= hiddenSliderBodyFadeOutStart) {
            this->fBodyAlpha = std::clamp<float>(((float)(hiddenSliderBodyFadeOutEnd - curPos) /
                                                  (float)(hiddenSliderBodyFadeOutEnd - hiddenSliderBodyFadeOutStart)),
                                                 0.0f, 1.0f);
            this->fBodyAlpha *= this->fBodyAlpha;  // quad in body fadeout
        }
    }

    // if this slider is active, recalculate sliding/curve position and general state
    if(this->fSlidePercent > 0.0f || this->bVisible) {
        // handle reverse sliders
        this->bInReverse = false;
        this->bHideNumberAfterFirstRepeatHit = false;
        if(this->iRepeat > 1) {
            if(this->fSlidePercent > 0.0f && this->bStartFinished) this->bHideNumberAfterFirstRepeatHit = true;

            float part = 1.0f / (float)this->iRepeat;
            this->iCurRepeat = (int)(this->fSlidePercent * this->iRepeat);
            float baseSlidePercent = part * this->iCurRepeat;
            float partSlidePercent = (this->fSlidePercent - baseSlidePercent) / part;
            if(this->iCurRepeat % 2 == 0) {
                this->fSlidePercent = partSlidePercent;
                this->iReverseArrowPos = 2;
            } else {
                this->fSlidePercent = 1.0f - partSlidePercent;
                this->iReverseArrowPos = 1;
                this->bInReverse = true;
            }

            // no reverse arrow on the last repeat
            if(this->iCurRepeat == this->iRepeat - 1) this->iReverseArrowPos = 0;

            // osu style: immediately show all coming reverse arrows (even on the circle we just started from)
            if(this->iCurRepeat < this->iRepeat - 2 && this->fSlidePercent > 0.0f && this->iRepeat > 2)
                this->iReverseArrowPos = 3;
        }

        this->vCurPointRaw = this->curve->pointAt(this->fSlidePercent);
        this->vCurPoint = this->bi->osuCoords2Pixels(this->vCurPointRaw);
    } else {
        this->vCurPointRaw = this->curve->pointAt(0.0f);
        this->vCurPoint = this->bi->osuCoords2Pixels(this->vCurPointRaw);
    }

    // When fat finger key is released, remove isClickHeldSlider() restrictions
    if(this->iFatFingerKey == 1 && !this->bi->isKey1Down()) this->iFatFingerKey = 0;
    if(this->iFatFingerKey == 2 && !this->bi->isKey2Down()) this->iFatFingerKey = 0;

    // handle dynamic followradius
    float followRadius =
        this->bCursorLeft ? this->bi->fHitcircleDiameter / 2.0f : this->bi->fSliderFollowCircleDiameter / 2.0f;
    const bool isBeatmapCursorInside = (vec::length(this->bi->getCursorPos() - this->vCurPoint) < followRadius);
    const bool isAutoCursorInside =
        ((this->bi->getModsLegacy() & LegacyFlags::Autoplay) &&
         (!cv::auto_cursordance.getBool() || (vec::length(this->bi->getCursorPos() - this->vCurPoint) < followRadius)));
    this->bCursorInside = (isAutoCursorInside || isBeatmapCursorInside);
    this->bCursorLeft = !this->bCursorInside;

    // handle slider start
    if(!this->bStartFinished) {
        if((this->bi->getModsLegacy() & LegacyFlags::Autoplay)) {
            if(curPos >= this->click_time) {
                this->onHit(LiveScore::HIT::HIT_300, 0, false);
                this->bi->holding_slider = true;
            }
        } else {
            long delta = curPos - this->click_time;

            if((this->bi->getModsLegacy() & LegacyFlags::Relax)) {
                if(curPos >= this->click_time + (long)cv::relax_offset.getInt() && !this->bi->isPaused() &&
                   !this->bi->isContinueScheduled()) {
                    const vec2 pos = this->bi->osuCoords2Pixels(this->curve->pointAt(0.0f));
                    const float cursorDelta = vec::length(this->bi->getCursorPos() - pos);
                    if((cursorDelta < this->bi->fHitcircleDiameter / 2.0f &&
                        (this->bi->getModsLegacy() & LegacyFlags::Relax))) {
                        LiveScore::HIT result = this->bi->getHitResult(delta);

                        if(result != LiveScore::HIT::HIT_NULL) {
                            const float targetDelta = cursorDelta / (this->bi->fHitcircleDiameter / 2.0f);
                            const float targetAngle = glm::degrees(
                                std::atan2(this->bi->getCursorPos().y - pos.y, this->bi->getCursorPos().x - pos.x));

                            this->startResult = result;
                            this->onHit(this->startResult, delta, false, targetDelta, targetAngle);
                            this->bi->holding_slider = true;
                        }
                    }
                }
            }

            // wait for a miss
            if(delta >= 0) {
                // if this is a miss after waiting
                if(delta > (long)this->bi->getHitWindow50()) {
                    this->startResult = LiveScore::HIT::HIT_MISS;
                    this->onHit(this->startResult, delta, false);
                    this->bi->holding_slider = false;
                }
            }
        }
    }

    // handle slider end, repeats, ticks
    if(!this->bEndFinished) {
        // NOTE: we have 2 timing conditions after which we start checking for strict tracking: 1) startcircle was
        // clicked, 2) slider has started timing wise it is easily possible to hit the startcircle way before the
        // sliderball would become active, which is why the first check exists. even if the sliderball has not yet
        // started sliding, you will be punished for leaving the (still invisible) followcircle area after having
        // clicked the startcircle, always.
        const bool isTrackingStrictTrackingMod =
            ((this->bStartFinished || curPos >= this->click_time) && cv::mod_strict_tracking.getBool());

        // slider tail lenience bullshit: see
        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Objects/Slider.cs#L123 being "inside the slider"
        // (for the end of the slider) is NOT checked at the exact end of the slider, but somewhere random before,
        // because fuck you
        const long offset = (long)cv::slider_end_inside_check_offset.getInt();
        const long lenienceHackEndTime =
            std::max(this->click_time + this->duration / 2, (this->click_time + this->duration) - offset);
        const bool isTrackingCorrectly =
            (this->isClickHeldSlider() || (this->bi->getModsLegacy() & LegacyFlags::Relax)) && this->bCursorInside;
        if(isTrackingCorrectly) {
            if(isTrackingStrictTrackingMod) {
                this->iStrictTrackingModLastClickHeldTime = curPos;
                if(this->iStrictTrackingModLastClickHeldTime ==
                   0)  // (prevent frame perfect inputs from not triggering the strict tracking miss because we use != 0
                       // comparison to detect if tracking correctly at least once)
                    this->iStrictTrackingModLastClickHeldTime = 1;
            }

            // only check it at the exact point in time ...
            if(curPos >= lenienceHackEndTime) {
                // ... once (like a tick)
                if(!this->bHeldTillEndForLenienceHackCheck) {
                    // player was correctly clicking/holding inside slider at lenienceHackEndTime
                    this->bHeldTillEndForLenienceHackCheck = true;
                    this->bHeldTillEndForLenienceHack = true;
                }
            }
        } else {
            // do not allow empty clicks outside of the circle radius to prevent the
            // m_bCursorInside flag from resetting
            this->bCursorLeft = true;
        }

        // can't be "inside the slider" after lenienceHackEndTime
        // (even though the slider is still going, which is madness)
        if(curPos >= lenienceHackEndTime) this->bHeldTillEndForLenienceHackCheck = true;

        // handle strict tracking mod
        if(isTrackingStrictTrackingMod) {
            const bool wasTrackingCorrectlyAtLeastOnce = (this->iStrictTrackingModLastClickHeldTime != 0);
            if(wasTrackingCorrectlyAtLeastOnce && !isTrackingCorrectly) {
                // if past lenience end time then don't trigger a strict tracking miss,
                // since the slider is then already considered fully finished gameplay wise
                if(!this->bHeldTillEndForLenienceHack) {
                    // force miss the end once, if it has not already been force missed by notelock
                    if(this->endResult == LiveScore::HIT::HIT_NULL) {
                        // force miss endcircle
                        this->onSliderBreak();

                        this->bHeldTillEnd = false;
                        this->bHeldTillEndForLenienceHack = false;
                        this->bHeldTillEndForLenienceHackCheck = true;
                        this->endResult = LiveScore::HIT::HIT_MISS;

                        // end of combo, ignore in hiterrorbar, ignore combo, subtract health
                        this->addHitResult(this->endResult, 0, this->is_end_of_combo,
                                           this->getRawPosAt(this->click_time + this->duration), -1.0f, 0.0f, true,
                                           true, false);
                    }
                }
            }
        }

        // handle repeats and ticks
        for(auto &click : this->clicks) {
            if(!click.finished && curPos >= click.time) {
                click.finished = true;
                click.successful = (this->isClickHeldSlider() && this->bCursorInside) ||
                                   (this->bi->getModsLegacy() & LegacyFlags::Autoplay) ||
                                   ((this->bi->getModsLegacy() & LegacyFlags::Relax) && this->bCursorInside);

                if(click.type == 0) {
                    this->onRepeatHit(click.successful, click.sliderend);
                } else {
                    this->onTickHit(click.successful, click.tickIndex);
                }
            }
        }

        // handle auto, and the last circle
        if((this->bi->getModsLegacy() & LegacyFlags::Autoplay)) {
            if(curPos >= this->click_time + this->duration) {
                this->bHeldTillEnd = true;
                this->onHit(LiveScore::HIT::HIT_300, 0, true);
                this->bi->holding_slider = false;
            }
        } else {
            if(curPos >= this->click_time + this->duration) {
                // handle leftover startcircle
                {
                    // this may happen (if the slider time is shorter than the miss window of the startcircle)
                    if(this->startResult == LiveScore::HIT::HIT_NULL) {
                        // we still want to cause a sliderbreak in this case!
                        this->onSliderBreak();

                        // special case: missing the startcircle drains HIT_MISS_SLIDERBREAK health (and not HIT_MISS
                        // health)
                        this->bi->addHitResult(this, LiveScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true,
                                               true,
                                               false);  // only decrease health

                        this->startResult = LiveScore::HIT::HIT_MISS;
                    }
                }

                // handle endcircle
                bool isEndResultComingFromStrictTrackingMod = false;
                if(this->endResult == LiveScore::HIT::HIT_NULL) {
                    this->bHeldTillEnd = this->bHeldTillEndForLenienceHack;
                    this->endResult = this->bHeldTillEnd ? LiveScore::HIT::HIT_300 : LiveScore::HIT::HIT_MISS;

                    // handle total slider result (currently startcircle + repeats + ticks + endcircle)
                    // clicks = (repeats + ticks)
                    const float numMaxPossibleHits = 1 + this->clicks.size() + 1;
                    float numActualHits = 0;

                    if(this->startResult != LiveScore::HIT::HIT_MISS) numActualHits++;
                    if(this->endResult != LiveScore::HIT::HIT_MISS) numActualHits++;

                    for(auto &click : this->clicks) {
                        if(click.successful) numActualHits++;
                    }

                    const float percent = numActualHits / numMaxPossibleHits;

                    const bool allow300 = (this->bi->getModsLegacy() & LegacyFlags::ScoreV2)
                                              ? (this->startResult == LiveScore::HIT::HIT_300)
                                              : true;
                    const bool allow100 = (this->bi->getModsLegacy() & LegacyFlags::ScoreV2)
                                              ? (this->startResult == LiveScore::HIT::HIT_300 ||
                                                 this->startResult == LiveScore::HIT::HIT_100)
                                              : true;

                    // rewrite m_endResult as the whole slider result, then use it for the final onHit()
                    if(percent >= 0.999f && allow300)
                        this->endResult = LiveScore::HIT::HIT_300;
                    else if(percent >= 0.5f && allow100 && !this->bi->mod_ming3012 && !this->bi->mod_no100s)
                        this->endResult = LiveScore::HIT::HIT_100;
                    else if(percent > 0.0f && !this->bi->mod_no100s && !this->bi->mod_no50s)
                        this->endResult = LiveScore::HIT::HIT_50;
                    else
                        this->endResult = LiveScore::HIT::HIT_MISS;

                    // debugLog("percent = {:f}\n", percent);

                    if(!this->bHeldTillEnd && cv::slider_end_miss_breaks_combo.getBool()) this->onSliderBreak();
                } else
                    isEndResultComingFromStrictTrackingMod = true;

                this->onHit(this->endResult, 0, true, 0.0f, 0.0f, isEndResultComingFromStrictTrackingMod);
                this->bi->holding_slider = false;
            }
        }

        // handle sliderslide sound
        // TODO @kiwec: move this to draw()
        if(this->bm != nullptr) {
            bool sliding = this->bStartFinished && !this->bEndFinished && this->bCursorInside && this->iDelta <= 0;
            sliding &= (this->isClickHeldSlider() || (this->bm->getModsLegacy() & LegacyFlags::Autoplay) ||
                        (this->bm->getModsLegacy() & LegacyFlags::Relax));
            sliding &= !this->bm->isPaused() && !this->bm->isWaiting() && this->bm->isPlaying();
            sliding &= !this->bm->bWasSeekFrame;

            if(sliding) {
                const vec2 osuCoords = this->bm->pixels2OsuCoords(this->bm->osuCoords2Pixels(this->vCurPointRaw));
                f32 pan = GameRules::osuCoords2Pan(osuCoords.x);
                this->samples.play(pan, 0, true);
            } else {
                this->samples.stop();
            }
        }
    }
}

void Slider::updateAnimations(long curPos) {
    float animation_multiplier = this->bi->getSpeedMultiplier() / osu->getAnimationSpeedMultiplier();

    float fadein_fade_time = cv::slider_followcircle_fadein_fade_time.getFloat() * animation_multiplier;
    float fadeout_fade_time = cv::slider_followcircle_fadeout_fade_time.getFloat() * animation_multiplier;
    float fadein_scale_time = cv::slider_followcircle_fadein_scale_time.getFloat() * animation_multiplier;
    float fadeout_scale_time = cv::slider_followcircle_fadeout_scale_time.getFloat() * animation_multiplier;

    // handle followcircle animations
    this->fFollowCircleAnimationAlpha =
        std::clamp<float>((float)((curPos - this->click_time)) / 1000.0f /
                              std::clamp<float>(fadein_fade_time, 0.0f, this->duration / 1000.0f),
                          0.0f, 1.0f);
    if(this->bFinished) {
        this->fFollowCircleAnimationAlpha =
            1.0f -
            std::clamp<float>((float)((curPos - (this->click_time + this->duration))) / 1000.0f / fadeout_fade_time,
                              0.0f, 1.0f);
        this->fFollowCircleAnimationAlpha *= this->fFollowCircleAnimationAlpha;  // quad in
    }

    this->fFollowCircleAnimationScale =
        std::clamp<float>((float)((curPos - this->click_time)) / 1000.0f /
                              std::clamp<float>(fadein_scale_time, 0.0f, this->duration / 1000.0f),
                          0.0f, 1.0f);
    if(this->bFinished) {
        this->fFollowCircleAnimationScale = std::clamp<float>(
            (float)((curPos - (this->click_time + this->duration))) / 1000.0f / fadeout_scale_time, 0.0f, 1.0f);
    }
    this->fFollowCircleAnimationScale =
        -this->fFollowCircleAnimationScale * (this->fFollowCircleAnimationScale - 2.0f);  // quad out

    if(!this->bFinished)
        this->fFollowCircleAnimationScale =
            cv::slider_followcircle_fadein_scale.getFloat() +
            (1.0f - cv::slider_followcircle_fadein_scale.getFloat()) * this->fFollowCircleAnimationScale;
    else
        this->fFollowCircleAnimationScale =
            1.0f - (1.0f - cv::slider_followcircle_fadeout_scale.getFloat()) * this->fFollowCircleAnimationScale;
}

void Slider::updateStackPosition(float stackOffset) {
    if(this->curve != nullptr)
        this->curve->updateStackPosition(this->iStack * stackOffset,
                                         (this->bi->getModsLegacy() & LegacyFlags::HardRock));
}

void Slider::miss(long curPos) {
    if(this->bFinished) return;

    const long delta = curPos - this->click_time;

    // startcircle
    if(!this->bStartFinished) {
        this->startResult = LiveScore::HIT::HIT_MISS;
        this->onHit(this->startResult, delta, false);
        this->bi->holding_slider = false;
    }

    // endcircle, repeats, ticks
    if(!this->bEndFinished) {
        // repeats, ticks
        {
            for(auto &click : this->clicks) {
                if(!click.finished) {
                    click.finished = true;
                    click.successful = false;

                    if(click.type == 0)
                        this->onRepeatHit(click.successful, click.sliderend);
                    else
                        this->onTickHit(click.successful, click.tickIndex);
                }
            }
        }

        // endcircle
        {
            this->bHeldTillEnd = this->bHeldTillEndForLenienceHack;

            if(!this->bHeldTillEnd && cv::slider_end_miss_breaks_combo.getBool()) this->onSliderBreak();

            this->endResult = LiveScore::HIT::HIT_MISS;
            this->onHit(this->endResult, 0, true);
            this->bi->holding_slider = false;
        }
    }
}

vec2 Slider::getRawPosAt(long pos) const {
    if(this->curve == nullptr) return vec2(0, 0);

    if(pos <= this->click_time)
        return this->curve->pointAt(0.0f);
    else if(pos >= this->click_time + this->fSliderTime) {
        if(this->iRepeat % 2 == 0)
            return this->curve->pointAt(0.0f);
        else
            return this->curve->pointAt(1.0f);
    } else
        return this->curve->pointAt(this->getT(pos, false));
}

vec2 Slider::getOriginalRawPosAt(long pos) const {
    if(this->curve == nullptr) return vec2(0, 0);

    if(pos <= this->click_time)
        return this->curve->originalPointAt(0.0f);
    else if(pos >= this->click_time + this->fSliderTime) {
        if(this->iRepeat % 2 == 0)
            return this->curve->originalPointAt(0.0f);
        else
            return this->curve->originalPointAt(1.0f);
    } else
        return this->curve->originalPointAt(this->getT(pos, false));
}

float Slider::getT(long pos, bool raw) const {
    float t = (float)((long)pos - (long)this->click_time) / this->fSliderTimeWithoutRepeats;
    if(raw)
        return t;
    else {
        auto floorVal = (float)std::floor(t);
        return ((int)floorVal % 2 == 0) ? t - floorVal : floorVal + 1 - t;
    }
}

void Slider::onClickEvent(std::vector<Click> &clicks) {
    if(this->points.size() == 0 || this->bBlocked)
        return;  // also handle note blocking here (doesn't need fancy shake logic, since sliders don't shake in
                 // osu!stable)

    if(!this->bStartFinished) {
        const vec2 cursorPos = clicks[0].pos;
        const vec2 pos = this->bi->osuCoords2Pixels(this->curve->pointAt(0.0f));
        const float cursorDelta = vec::length(cursorPos - pos);

        if(cursorDelta < this->bi->fHitcircleDiameter / 2.0f) {
            const long delta = clicks[0].click_time - (long)this->click_time;

            LiveScore::HIT result = this->bi->getHitResult(delta);
            if(result != LiveScore::HIT::HIT_NULL) {
                const float targetDelta = cursorDelta / (this->bi->fHitcircleDiameter / 2.0f);
                const float targetAngle = glm::degrees(std::atan2(cursorPos.y - pos.y, cursorPos.x - pos.x));

                clicks.erase(clicks.begin());
                this->startResult = result;
                this->onHit(this->startResult, delta, false, targetDelta, targetAngle);
                this->bi->holding_slider = true;
            }
        }
    }
}

void Slider::onHit(LiveScore::HIT result, long delta, bool startOrEnd, float targetDelta, float targetAngle,
                   bool isEndResultFromStrictTrackingMod) {
    if(this->points.size() == 0) return;

    // start + end of a slider add +30 points, if successful

    // debugLog("startOrEnd = {:d},    m_iCurRepeat = {:d}\n", (int)startOrEnd, this->iCurRepeat);

    // sound and hit animation and also sliderbreak combo drop
    {
        if(result == LiveScore::HIT::HIT_MISS) {
            if(!isEndResultFromStrictTrackingMod) this->onSliderBreak();
        } else if(this->bm != nullptr) {
            const vec2 osuCoords = this->bm->pixels2OsuCoords(this->bm->osuCoords2Pixels(this->vCurPointRaw));
            f32 pan = GameRules::osuCoords2Pan(osuCoords.x);

            if(this->edgeSamples.size() > 0) {
                this->edgeSamples[0].play(pan, delta);
            }

            if(!startOrEnd) {
                this->fStartHitAnimation = 0.001f;  // quickfix for 1 frame missing images
                anim->moveQuadOut(&this->fStartHitAnimation, 1.0f, GameRules::getFadeOutTime(this->bm), true);
            } else {
                if(this->iRepeat % 2 != 0) {
                    this->fEndHitAnimation = 0.001f;  // quickfix for 1 frame missing images
                    anim->moveQuadOut(&this->fEndHitAnimation, 1.0f, GameRules::getFadeOutTime(this->bm), true);
                } else {
                    this->fStartHitAnimation = 0.001f;  // quickfix for 1 frame missing images
                    anim->moveQuadOut(&this->fStartHitAnimation, 1.0f, GameRules::getFadeOutTime(this->bm), true);
                }
            }
        }

        // end body fadeout
        if(this->bm != nullptr && startOrEnd) {
            this->fEndSliderBodyFadeAnimation = 0.001f;  // quickfix for 1 frame missing images
            anim->moveQuadOut(&this->fEndSliderBodyFadeAnimation, 1.0f,
                              GameRules::getFadeOutTime(this->bm) * cv::slider_body_fade_out_time_multiplier.getFloat(),
                              true);

            this->samples.stop();
        }
    }

    // add score, and we are finished
    if(!startOrEnd) {
        // startcircle

        this->bStartFinished = true;

        // The player entered the slider by fat fingering, so...
        if(this->bi->isKey1Down() && this->bi->isKey2Down()) {
            if(this->bi->bPrevKeyWasKey1) {
                // Player pressed K1 last: "fat finger" key is K2
                this->iFatFingerKey = 2;
            } else {
                // Player pressed K2 last: "fat finger" key is K1
                this->iFatFingerKey = 1;
            }
        }

        if(this->bi->getModsLegacy() & LegacyFlags::Target) {
            // not end of combo, show in hiterrorbar, use for accuracy, increase combo, increase
            // score, ignore for health, don't add object duration to result anim
            this->addHitResult(result, delta, false, this->curve->pointAt(0.0f), targetDelta, targetAngle, false, false,
                               true, false);
        } else {
            // not end of combo, show in hiterrorbar, ignore for accuracy, increase combo,
            // don't count towards score, depending on scorev2 ignore for health or not
            this->bi->addHitResult(this, result, delta, false, false, true, false, true, true);
        }

        // add bonus score + health manually
        if(result != LiveScore::HIT::HIT_MISS) {
            LiveScore::HIT resultForHealth = LiveScore::HIT::HIT_SLIDER30;

            this->bi->addHitResult(this, resultForHealth, 0, false, true, true, true, true,
                                   false);  // only increase health
            this->bi->addScorePoints(30);
        } else {
            // special case: missing the startcircle drains HIT_MISS_SLIDERBREAK health (and not HIT_MISS health)
            this->bi->addHitResult(this, LiveScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true, true,
                                   false);  // only decrease health
        }
    } else {
        // endcircle

        this->bStartFinished = true;
        this->bEndFinished = true;
        this->bFinished = true;

        if(!isEndResultFromStrictTrackingMod) {
            // special case: osu!lazer 2020 only returns 1 judgement for the whole slider, but via the startcircle. i.e.
            // we are not allowed to drain again here in neosu logic (because startcircle judgement is handled at the
            // end here)
            // XXX: remove this
            const bool isLazer2020Drain = false;

            this->addHitResult(
                result, delta, this->is_end_of_combo, this->getRawPosAt(this->click_time + this->duration), -1.0f, 0.0f,
                true, !this->bHeldTillEnd,
                isLazer2020Drain);  // end of combo, ignore in hiterrorbar, depending on heldTillEnd increase
                                    // combo or not, increase score, increase health depending on drain type

            // add bonus score + extra health manually
            if(this->bHeldTillEnd) {
                this->bi->addHitResult(this, LiveScore::HIT::HIT_SLIDER30, 0, false, true, true, true, true,
                                       false);  // only increase health
                this->bi->addScorePoints(30);
            } else {
                // special case: missing the endcircle drains HIT_MISS_SLIDERBREAK health (and not HIT_MISS health)
                // NOTE: yes, this will drain twice for the end of a slider (once for the judgement of the whole slider
                // above, and once for the endcircle here)
                this->bi->addHitResult(this, LiveScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true, true,
                                       false);  // only decrease health
            }
        }
    }

    this->iCurRepeatCounterForHitSounds++;
}

void Slider::onRepeatHit(bool successful, bool sliderend) {
    if(this->points.size() == 0) return;

    // repeat hit of a slider adds +30 points, if successful

    // sound and hit animation
    if(!successful) {
        this->onSliderBreak();
    } else if(this->bm != nullptr) {
        const vec2 osuCoords = this->bm->pixels2OsuCoords(this->bm->osuCoords2Pixels(this->vCurPointRaw));
        f32 pan = GameRules::osuCoords2Pan(osuCoords.x);

        auto nb_edge_samples = this->edgeSamples.size();
        if(sliderend) {
            // On sliderend, play the last edgeSample.
            this->edgeSamples.back().play(pan, 0);
        } else {
            // NOTE: iCurRepeatCounterForHitSounds starts at 1
            if(this->iCurRepeatCounterForHitSounds + 1 < nb_edge_samples) {
                this->edgeSamples[this->iCurRepeatCounterForHitSounds].play(pan, 0);
            } else {
                // We have more repeats than edge samples!
                // Just play whatever we can (either the last repeat sample, or the start sample)
                this->edgeSamples[nb_edge_samples - 2].play(pan, 0);
            }
        }

        float animation_multiplier = this->bm->getSpeedMultiplier() / osu->getAnimationSpeedMultiplier();
        float tick_pulse_time = cv::slider_followcircle_tick_pulse_time.getFloat() * animation_multiplier;

        this->fFollowCircleTickAnimationScale = 0.0f;
        anim->moveLinear(&this->fFollowCircleTickAnimationScale, 1.0f, tick_pulse_time, true);

        if(sliderend) {
            this->fEndHitAnimation = 0.001f;  // quickfix for 1 frame missing images
            anim->moveQuadOut(&this->fEndHitAnimation, 1.0f, GameRules::getFadeOutTime(this->bm), true);
        } else {
            this->fStartHitAnimation = 0.001f;  // quickfix for 1 frame missing images
            anim->moveQuadOut(&this->fStartHitAnimation, 1.0f, GameRules::getFadeOutTime(this->bm), true);
        }
    }

    // add score
    if(!successful) {
        // add health manually
        // special case: missing a repeat drains HIT_MISS_SLIDERBREAK health (and not HIT_MISS health)
        this->bi->addHitResult(this, LiveScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true, true,
                               false);  // only decrease health
    } else {
        this->bi->addHitResult(this, LiveScore::HIT::HIT_SLIDER30, 0, false, true, true, false, true,
                               false);  // not end of combo, ignore in hiterrorbar, ignore for accuracy, increase
                                        // combo, don't count towards score, increase health

        // add bonus score manually
        this->bi->addScorePoints(30);
    }

    this->iCurRepeatCounterForHitSounds++;
}

void Slider::onTickHit(bool successful, int tickIndex) {
    if(this->points.size() == 0) return;

    // tick hit of a slider adds +10 points, if successful

    // tick drawing visibility
    int numMissingTickClicks = 0;
    for(auto &click : this->clicks) {
        if(click.type == 1 && click.tickIndex == tickIndex && !click.finished) numMissingTickClicks++;
    }
    if(numMissingTickClicks == 0) this->ticks[tickIndex].finished = true;

    // sound and hit animation
    if(!successful) {
        this->onSliderBreak();
    } else if(this->bm != nullptr) {
        const vec2 osuCoords = this->bm->pixels2OsuCoords(this->bm->osuCoords2Pixels(this->vCurPointRaw));
        f32 pan = GameRules::osuCoords2Pan(osuCoords.x);

        {
            // NOTE: osu! wiki doesn't mention if ticks use the normal set or the addition set.
            //       in fact, it doesn't mention ticks at all.
            std::string sound_name = "SKIN_";
            switch(this->samples.getAdditionSet()) {
                case SampleSetType::NORMAL:
                    sound_name.append("NORMAL");
                    break;
                case SampleSetType::SOFT:
                    sound_name.append("SOFT");
                    break;
                case SampleSetType::DRUM:
                    sound_name.append("DRUM");
                    break;
            }
            sound_name.append("SLIDERTICK_SND");

            auto sound = resourceManager->getSound(sound_name);
            if(sound != nullptr) {
                soundEngine->play(sound, pan, 0.f, this->samples.getVolume(this->samples.getAdditionSet(), true));
            }
        }

        float animation_multiplier = this->bm->getSpeedMultiplier() / osu->getAnimationSpeedMultiplier();
        float tick_pulse_time = cv::slider_followcircle_tick_pulse_time.getFloat() * animation_multiplier;

        this->fFollowCircleTickAnimationScale = 0.0f;
        anim->moveLinear(&this->fFollowCircleTickAnimationScale, 1.0f, tick_pulse_time, true);
    }

    // add score
    if(!successful) {
        // add health manually
        // special case: missing a tick drains HIT_MISS_SLIDERBREAK health (and not HIT_MISS health)
        this->bi->addHitResult(this, LiveScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true, true,
                               false);  // only decrease health
    } else {
        this->bi->addHitResult(this, LiveScore::HIT::HIT_SLIDER10, 0, false, true, true, false, true,
                               false);  // not end of combo, ignore in hiterrorbar, ignore for accuracy, increase
                                        // combo, don't count towards score, increase health

        // add bonus score manually
        this->bi->addScorePoints(10);
    }
}

void Slider::onSliderBreak() { this->bi->addSliderBreak(); }

void Slider::onReset(long curPos) {
    HitObject::onReset(curPos);

    if(this->bm != nullptr) {
        this->samples.stop();

        anim->deleteExistingAnimation(&this->fFollowCircleTickAnimationScale);
        anim->deleteExistingAnimation(&this->fStartHitAnimation);
        anim->deleteExistingAnimation(&this->fEndHitAnimation);
        anim->deleteExistingAnimation(&this->fEndSliderBodyFadeAnimation);
    }

    this->iStrictTrackingModLastClickHeldTime = 0;
    this->iFatFingerKey = 0;
    this->bCursorLeft = true;
    this->bHeldTillEnd = false;
    this->bHeldTillEndForLenienceHack = false;
    this->bHeldTillEndForLenienceHackCheck = false;
    this->startResult = LiveScore::HIT::HIT_NULL;
    this->endResult = LiveScore::HIT::HIT_NULL;

    this->iCurRepeatCounterForHitSounds = 0;

    if(this->click_time > curPos) {
        this->bStartFinished = false;
        this->fStartHitAnimation = 0.0f;
        this->bEndFinished = false;
        this->bFinished = false;
        this->fEndHitAnimation = 0.0f;
        this->fEndSliderBodyFadeAnimation = 0.0f;
    } else if(curPos < this->click_time + this->duration) {
        this->bStartFinished = true;
        this->fStartHitAnimation = 1.0f;

        this->bEndFinished = false;
        this->bFinished = false;
        this->fEndHitAnimation = 0.0f;
        this->fEndSliderBodyFadeAnimation = 0.0f;
    } else {
        this->bStartFinished = true;
        this->fStartHitAnimation = 1.0f;
        this->bEndFinished = true;
        this->bFinished = true;
        this->fEndHitAnimation = 1.0f;
        this->fEndSliderBodyFadeAnimation = 1.0f;
    }

    for(auto &click : this->clicks) {
        if(curPos > click.time) {
            click.finished = true;
            click.successful = true;
        } else {
            click.finished = false;
            click.successful = false;
        }
    }

    for(int i = 0; i < this->ticks.size(); i++) {
        int numMissingTickClicks = 0;
        for(auto &click : this->clicks) {
            if(click.type == 1 && click.tickIndex == i && !click.finished) numMissingTickClicks++;
        }
        this->ticks[i].finished = numMissingTickClicks == 0;
    }
}

void Slider::rebuildVertexBuffer(bool useRawCoords) {
    // base mesh (background) (raw unscaled, size in raw osu coordinates centered at (0, 0, 0))
    // this mesh needs to be scaled and translated appropriately since we are not 1:1 with the playfield
    std::vector<vec2> osuCoordPoints = this->curve->getPoints();
    if(!useRawCoords) {
        for(auto &osuCoordPoint : osuCoordPoints) {
            osuCoordPoint = this->bi->osuCoords2LegacyPixels(osuCoordPoint);
        }
    }
    SAFE_DELETE(this->vao);
    this->vao = SliderRenderer::generateVAO(osuCoordPoints, this->bi->fRawHitcircleDiameter);
}

bool Slider::isClickHeldSlider() {
    // osu! has a weird slider quirk, that I'll explain in detail here.
    // When holding K1 before the slider, tapping K2 on slider head, and releasing K2 later,
    // the slider is no longer considered being "held" until K2 is pressed again, or K1 is released and pressed again.

    // The reason this exists is to prevent people from holding K1 the whole map and tapping with K2.
    // Holding is part of the rhythm flow, and this is a rhythm game right?

    if(this->iFatFingerKey == 0) return this->bi->isClickHeld();
    if(this->iFatFingerKey == 1) return this->bi->isKey2Down();
    if(this->iFatFingerKey == 2) return this->bi->isKey1Down();

    return false;  // unreachable
}

Spinner::Spinner(int x, int y, long time, HitSamples samples, bool isEndOfCombo, long endTime,
                 BeatmapInterface *beatmap)
    : HitObject(time, samples, -1, isEndOfCombo, -1, -1, beatmap) {
    this->type = HitObjectType::SPINNER;

    this->vOriginalRawPos = vec2(x, y);
    this->vRawPos = this->vOriginalRawPos;
    this->duration = endTime - time;
    this->fRotationsNeeded = -1.0f;

    int minVel = 12;
    int maxVel = 48;
    int minTime = 2000;
    int maxTime = 5000;
    this->iMaxStoredDeltaAngles = std::clamp<int>(
        (int)((endTime - time - minTime) * (maxVel - minVel) / (maxTime - minTime) + minVel), minVel, maxVel);
    this->storedDeltaAngles = new float[this->iMaxStoredDeltaAngles];
    for(int i = 0; i < this->iMaxStoredDeltaAngles; i++) {
        this->storedDeltaAngles[i] = 0.0f;
    }
    this->iDeltaAngleIndex = 0;

    this->fPercent = 0.0f;

    this->fDrawRot = 0.0f;
    this->fRotations = 0.0f;
    this->fDeltaOverflow = 0.0f;
    this->fSumDeltaAngle = 0.0f;
    this->fDeltaAngleOverflow = 0.0f;
    this->fRPM = 0.0f;
    this->fLastMouseAngle = 0.0f;
    this->fRatio = 0.0f;

    // spinners don't need misaims
    this->bMisAim = true;

    // spinners don't use AR-dependent fadein, instead they always fade in with hardcoded 400 ms (see
    // GameRules::getFadeInTime())
    this->bUseFadeInTimeAsApproachTime = !cv::spinner_use_ar_fadein.getBool();
}

Spinner::~Spinner() {
    this->onReset(0);

    delete[] this->storedDeltaAngles;
    this->storedDeltaAngles = nullptr;
}

void Spinner::draw() {
    HitObject::draw();
    const float fadeOutMultiplier = cv::spinner_fade_out_time_multiplier.getFloat();
    const long fadeOutTimeMS = (long)(GameRules::getFadeOutTime(this->bm) * 1000.0f * fadeOutMultiplier);
    const long deltaEnd = this->iDelta + this->duration;
    if((this->bFinished || !this->bVisible) && (deltaEnd > 0 || (deltaEnd < -fadeOutTimeMS))) return;

    Skin *skin = this->bm->getSkin();
    vec2 center = this->bm->osuCoords2Pixels(this->vRawPos);

    const float alphaMultiplier =
        std::clamp<float>((deltaEnd < 0 ? 1.0f - ((float)std::abs(deltaEnd) / (float)fadeOutTimeMS) : 1.0f), 0.0f,
                          1.0f);  // only used for fade out anim atm

    const float globalScale = 1.0f;        // adjustments
    const float globalBaseSkinSize = 667;  // the width of spinner-bottom.png in the default skin
    const float globalBaseSize = this->bm->getPlayfieldSize().y;

    const float clampedRatio = std::clamp<float>(this->fRatio, 0.0f, 1.0f);
    float finishScaleRatio = clampedRatio;
    finishScaleRatio = -finishScaleRatio * (finishScaleRatio - 2);
    const float finishScale =
        0.80f +
        finishScaleRatio *
            0.20f;  // the spinner grows until reaching 100% during spinning, depending on how many spins are left

    if(skin->getSpinnerBackground() != skin->getMissingTexture() || skin->getVersion() < 2.0f)  // old style
    {
        // draw spinner circle
        if(skin->getSpinnerCircle() != skin->getMissingTexture()) {
            const float spinnerCircleScale =
                globalBaseSize / (globalBaseSkinSize * (skin->isSpinnerCircle2x() ? 2.0f : 1.0f));

            g->setColor(Color(0xffffffff).setA(this->fAlphaWithoutHidden * alphaMultiplier));

            g->pushTransform();
            {
                g->rotate(this->fDrawRot);
                g->scale(spinnerCircleScale * globalScale, spinnerCircleScale * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerCircle());
            }
            g->popTransform();
        }

        // draw approach circle
        if(!(this->bi->getModsLegacy() & LegacyFlags::Hidden) && this->fPercent > 0.0f) {
            const float spinnerApproachCircleImageScale =
                globalBaseSize / ((globalBaseSkinSize / 2) * (skin->isSpinnerApproachCircle2x() ? 2.0f : 1.0f));

            g->setColor(Color(skin->getSpinnerApproachCircleColor()).setA(this->fAlphaWithoutHidden * alphaMultiplier));

            g->pushTransform();
            {
                g->scale(spinnerApproachCircleImageScale * this->fPercent * globalScale,
                         spinnerApproachCircleImageScale * this->fPercent * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerApproachCircle());
            }
            g->popTransform();
        }
    } else  // new style
    {
        // bottom
        if(skin->getSpinnerBottom() != skin->getMissingTexture()) {
            const float spinnerBottomImageScale =
                globalBaseSize / (globalBaseSkinSize * (skin->isSpinnerBottom2x() ? 2.0f : 1.0f));

            g->setColor(Color(0xffffffff).setA(this->fAlphaWithoutHidden * alphaMultiplier));

            g->pushTransform();
            {
                g->rotate(this->fDrawRot / 7.0f);
                g->scale(spinnerBottomImageScale * finishScale * globalScale,
                         spinnerBottomImageScale * finishScale * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerBottom());
            }
            g->popTransform();
        }

        // top
        if(skin->getSpinnerTop() != skin->getMissingTexture()) {
            const float spinnerTopImageScale =
                globalBaseSize / (globalBaseSkinSize * (skin->isSpinnerTop2x() ? 2.0f : 1.0f));

            g->setColor(Color(0xffffffff).setA(this->fAlphaWithoutHidden * alphaMultiplier));

            g->pushTransform();
            {
                g->rotate(this->fDrawRot / 2.0f);
                g->scale(spinnerTopImageScale * finishScale * globalScale,
                         spinnerTopImageScale * finishScale * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerTop());
            }
            g->popTransform();
        }

        // middle
        if(skin->getSpinnerMiddle2() != skin->getMissingTexture()) {
            const float spinnerMiddle2ImageScale =
                globalBaseSize / (globalBaseSkinSize * (skin->isSpinnerMiddle22x() ? 2.0f : 1.0f));

            g->setColor(Color(0xffffffff).setA(this->fAlphaWithoutHidden * alphaMultiplier));

            g->pushTransform();
            {
                g->rotate(this->fDrawRot);
                g->scale(spinnerMiddle2ImageScale * finishScale * globalScale,
                         spinnerMiddle2ImageScale * finishScale * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerMiddle2());
            }
            g->popTransform();
        }
        if(skin->getSpinnerMiddle() != skin->getMissingTexture()) {
            const float spinnerMiddleImageScale =
                globalBaseSize / (globalBaseSkinSize * (skin->isSpinnerMiddle2x() ? 2.0f : 1.0f));

            g->setColor(
                argb(this->fAlphaWithoutHidden * alphaMultiplier, 1.f, (1.f * this->fPercent), (1.f * this->fPercent)));
            g->pushTransform();
            {
                g->rotate(this->fDrawRot / 2.0f);  // apparently does not rotate in osu
                g->scale(spinnerMiddleImageScale * finishScale * globalScale,
                         spinnerMiddleImageScale * finishScale * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerMiddle());
            }
            g->popTransform();
        }

        // approach circle
        if(!(this->bi->getModsLegacy() & LegacyFlags::Hidden) && this->fPercent > 0.0f) {
            const float spinnerApproachCircleImageScale =
                globalBaseSize / ((globalBaseSkinSize / 2) * (skin->isSpinnerApproachCircle2x() ? 2.0f : 1.0f));

            g->setColor(Color(skin->getSpinnerApproachCircleColor()).setA(this->fAlphaWithoutHidden * alphaMultiplier));

            g->pushTransform();
            {
                g->scale(spinnerApproachCircleImageScale * this->fPercent * globalScale,
                         spinnerApproachCircleImageScale * this->fPercent * globalScale);
                g->translate(center.x, center.y);
                g->drawImage(skin->getSpinnerApproachCircle());
            }
            g->popTransform();
        }
    }

    // "CLEAR!"
    if(this->fRatio >= 1.0f) {
        const float spinnerClearImageScale = Osu::getImageScale(skin->getSpinnerClear(), 80);

        g->setColor(Color(0xffffffff).setA(alphaMultiplier));

        g->pushTransform();
        {
            g->scale(spinnerClearImageScale, spinnerClearImageScale);
            g->translate(center.x, center.y - this->bm->getPlayfieldSize().y * 0.25f);
            g->drawImage(skin->getSpinnerClear());
        }
        g->popTransform();
    }

    // "SPIN!"
    if(clampedRatio < 0.03f) {
        const float spinerSpinImageScale = Osu::getImageScale(skin->getSpinnerSpin(), 80);

        g->setColor(Color(0xffffffff).setA(this->fAlphaWithoutHidden * alphaMultiplier));

        g->pushTransform();
        {
            g->scale(spinerSpinImageScale, spinerSpinImageScale);
            g->translate(center.x, center.y + this->bm->getPlayfieldSize().y * 0.30f);
            g->drawImage(skin->getSpinnerSpin());
        }
        g->popTransform();
    }

    // draw RPM
    if(this->iDelta < 0) {
        McFont *rpmFont = resourceManager->getFont("FONT_DEFAULT");
        const float stringWidth = rpmFont->getStringWidth("RPM: 477");
        g->setColor(Color(0xffffffff)
                        .setA(this->fAlphaWithoutHidden * this->fAlphaWithoutHidden * this->fAlphaWithoutHidden *
                              alphaMultiplier));

        g->pushTransform();
        {
            g->translate(
                (int)(osu->getScreenWidth() / 2 - stringWidth / 2),
                (int)(osu->getScreenHeight() - 5 + (5 + rpmFont->getHeight()) * (1.0f - this->fAlphaWithoutHidden)));
            g->drawString(rpmFont, UString::format("RPM: %i", (int)(this->fRPM + 0.4f)));
        }
        g->popTransform();
    }
}

void Spinner::update(long curPos, f64 frame_time) {
    HitObject::update(curPos, frame_time);

    // stop spinner sound and don't update() while paused
    if(this->bi->isPaused() || !this->bi->isPlaying() || (this->bm && this->bm->hasFailed())) {
        if(this->bm != nullptr) {
            this->bm->getSkin()->stopSpinnerSpinSound();
        }
        return;
    }

    // if we have not been clicked yet, check if we are in the timeframe of a miss, also handle auto and relax
    if(!this->bFinished) {
        // handle spinner ending
        if(curPos >= this->click_time + this->duration) {
            this->onHit();
            return;
        }

        // Skip calculations
        if(frame_time == 0.0) {
            return;
        }

        this->fRotationsNeeded = GameRules::getSpinnerRotationsForSpeedMultiplier(this->bi, this->duration);

        const float DELTA_UPDATE_TIME = (frame_time * 1000.0f);
        const float AUTO_MULTIPLIER = (1.0f / 20.0f);

        // scale percent calculation
        long delta = (long)this->click_time - (long)curPos;
        this->fPercent = 1.0f - std::clamp<float>((float)delta / -(float)(this->duration), 0.0f, 1.0f);

        // handle auto, mouse spinning movement
        float angleDiff = 0;
        if((this->bi->getModsLegacy() & LegacyFlags::Autoplay) ||
           (this->bi->getModsLegacy() & LegacyFlags::Autopilot) || (this->bi->getModsLegacy() & LegacyFlags::SpunOut)) {
            angleDiff = frame_time * 1000.0f * AUTO_MULTIPLIER * this->bi->getSpeedMultiplier();
        } else {  // user spin
            vec2 mouseDelta = this->bi->getCursorPos() - this->bi->osuCoords2Pixels(this->vRawPos);
            const auto currentMouseAngle = (float)std::atan2(mouseDelta.y, mouseDelta.x);
            angleDiff = (currentMouseAngle - this->fLastMouseAngle);

            if(std::abs(angleDiff) > 0.001f)
                this->fLastMouseAngle = currentMouseAngle;
            else
                angleDiff = 0;
        }

        // handle spinning
        // HACKHACK: rewrite this
        if(delta <= 0) {
            bool isSpinning = this->bi->isClickHeld() || (this->bi->getModsLegacy() & LegacyFlags::Autoplay) ||
                              (this->bi->getModsLegacy() & LegacyFlags::Relax) ||
                              (this->bi->getModsLegacy() & LegacyFlags::SpunOut);

            this->fDeltaOverflow += frame_time * 1000.0f;

            if(angleDiff < -PI)
                angleDiff += 2 * PI;
            else if(angleDiff > PI)
                angleDiff -= 2 * PI;

            if(isSpinning) this->fDeltaAngleOverflow += angleDiff;

            while(this->fDeltaOverflow >= DELTA_UPDATE_TIME) {
                // spin caused by the cursor
                float deltaAngle = 0;
                if(isSpinning) {
                    deltaAngle = this->fDeltaAngleOverflow * DELTA_UPDATE_TIME / this->fDeltaOverflow;
                    this->fDeltaAngleOverflow -= deltaAngle;
                    // deltaAngle = std::clamp<float>(deltaAngle, -MAX_ANG_DIFF, MAX_ANG_DIFF);
                }

                this->fDeltaOverflow -= DELTA_UPDATE_TIME;

                this->fSumDeltaAngle -= this->storedDeltaAngles[this->iDeltaAngleIndex];
                this->fSumDeltaAngle += deltaAngle;
                this->storedDeltaAngles[this->iDeltaAngleIndex++] = deltaAngle;
                this->iDeltaAngleIndex %= this->iMaxStoredDeltaAngles;

                float rotationAngle = this->fSumDeltaAngle / this->iMaxStoredDeltaAngles;
                float rotationPerSec = rotationAngle * (1000.0f / DELTA_UPDATE_TIME) / (2.0f * PI);

                f32 decay = pow(0.01f, (f32)frame_time);
                this->fRPM = this->fRPM * decay + (1.0 - decay) * std::abs(rotationPerSec) * 60;
                this->fRPM = std::min(this->fRPM, 477.0f);

                if(std::abs(rotationAngle) > 0.0001f) this->rotate(rotationAngle);
            }

            this->fRatio = this->fRotations / (this->fRotationsNeeded * 360.0f);
        }
    }
}

void Spinner::onReset(long curPos) {
    HitObject::onReset(curPos);

    if(this->bm != nullptr) {
        this->bm->getSkin()->stopSpinnerSpinSound();
    }

    this->fRPM = 0.0f;
    this->fDrawRot = 0.0f;
    this->fRotations = 0.0f;
    this->fDeltaOverflow = 0.0f;
    this->fSumDeltaAngle = 0.0f;
    this->iDeltaAngleIndex = 0;
    this->fDeltaAngleOverflow = 0.0f;
    this->fRatio = 0.0f;

    // spinners don't need misaims
    this->bMisAim = true;

    for(int i = 0; i < this->iMaxStoredDeltaAngles; i++) {
        this->storedDeltaAngles[i] = 0.0f;
    }

    if(curPos > this->click_time + this->duration)
        this->bFinished = true;
    else
        this->bFinished = false;
}

void Spinner::onHit() {
    // calculate hit result
    LiveScore::HIT result = LiveScore::HIT::HIT_NULL;
    if(this->fRatio >= 1.0f || (this->bi->getModsLegacy() & LegacyFlags::Autoplay))
        result = LiveScore::HIT::HIT_300;
    else if(this->fRatio >= 0.9f && !cv::mod_ming3012.getBool() && !cv::mod_no100s.getBool())
        result = LiveScore::HIT::HIT_100;
    else if(this->fRatio >= 0.75f && !cv::mod_no100s.getBool() && !cv::mod_no50s.getBool())
        result = LiveScore::HIT::HIT_50;
    else
        result = LiveScore::HIT::HIT_MISS;

    // sound
    if(this->bm != nullptr && result != LiveScore::HIT::HIT_MISS) {
        const vec2 osuCoords = this->bm->pixels2OsuCoords(this->bm->osuCoords2Pixels(this->vRawPos));
        f32 pan = GameRules::osuCoords2Pan(osuCoords.x);
        this->samples.play(pan, 0);
    }

    // add it, and we are finished
    this->addHitResult(result, 0, this->is_end_of_combo, this->vRawPos, -1.0f);
    this->bFinished = true;

    if(this->bm != nullptr) {
        this->bm->getSkin()->stopSpinnerSpinSound();
    }
}

void Spinner::rotate(float rad) {
    this->fDrawRot += glm::degrees(rad);

    rad = std::abs(rad);
    const float newRotations = this->fRotations + glm::degrees(rad);

    // added one whole rotation
    if(std::floor(newRotations / 360.0f) > this->fRotations / 360.0f) {
        if((int)(newRotations / 360.0f) > (int)(this->fRotationsNeeded) + 1) {
            // extra rotations and bonus sound
            if(this->bm != nullptr && !this->bm->bWasSeekFrame) {
                this->bm->getSkin()->playSpinnerBonusSound();
            }
            this->bi->addHitResult(this, LiveScore::HIT::HIT_SPINNERBONUS, 0, false, true, true, true, true,
                                   false);  // only increase health
            this->bi->addHitResult(this, LiveScore::HIT::HIT_SPINNERBONUS, 0, false, true, true, true, true,
                                   false);  // HACKHACK: compensating for rotation logic differences
            this->bi->addScorePoints(1100, true);
        } else {
            // normal whole rotation
            this->bi->addHitResult(this, LiveScore::HIT::HIT_SPINNERSPIN, 0, false, true, true, true, true,
                                   false);  // only increase health
            this->bi->addHitResult(this, LiveScore::HIT::HIT_SPINNERSPIN, 0, false, true, true, true, true,
                                   false);  // HACKHACK: compensating for rotation logic differences
            this->bi->addScorePoints(100, true);
        }
    }

    // spinner sound
    if(this->bm != nullptr && !this->bm->bWasSeekFrame) {
        this->bm->getSkin()->playSpinnerSpinSound();

        const float frequency = 20000.0f + (int)(std::clamp<float>(this->fRatio, 0.0f, 2.5f) * 40000.0f);
        this->bm->getSkin()->getSpinnerSpinSound()->setFrequency(frequency);
    }

    this->fRotations = newRotations;
}

vec2 Spinner::getAutoCursorPos(long curPos) const {
    // calculate point
    long delta = 0;
    if(curPos <= this->click_time)
        delta = 0;
    else if(curPos >= this->click_time + this->duration)
        delta = this->duration;
    else
        delta = curPos - this->click_time;

    vec2 actualPos = this->bi->osuCoords2Pixels(this->vRawPos);
    const float AUTO_MULTIPLIER = (1.0f / 20.0f);
    float multiplier =
        ((this->bi->getModsLegacy() & LegacyFlags::Autoplay) || (this->bi->getModsLegacy() & LegacyFlags::Autopilot))
            ? AUTO_MULTIPLIER
            : 1.0f;
    float angle = (delta * multiplier) - PI / 2.0f;
    float r = GameRules::getPlayfieldSize().y / 10.0f;  // XXX: slow?
    return vec2((float)(actualPos.x + r * std::cos(angle)), (float)(actualPos.y + r * std::sin(angle)));
}
