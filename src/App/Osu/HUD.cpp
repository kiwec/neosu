// Copyright (c) 2016, PG, All rights reserved.
#include "HUD.h"

#include <algorithm>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoUsers.h"
#include "BeatmapInterface.h"
#include "CBaseUIContainer.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "HitObjects.h"
#include "Lobby.h"
#include "ModFPoSu.h"
#include "Mouse.h"
#include "Osu.h"
#include "RankingScreen.h"
#include "ResourceManager.h"
#include "ScoreboardSlot.h"
#include "Shader.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser/SongBrowser.h"
#include "UIAvatar.h"
#include "VertexArrayObject.h"
#include "score.h"

#include "shaders.h"

HUD::HUD() : OsuScreen() {
    // resources
    this->tempFont = resourceManager->getFont("FONT_DEFAULT");
    this->cursorTrailShader = resourceManager->createShader(
        std::string(reinterpret_cast<const char *>(cursortrail_vsh), cursortrail_vsh_size()),
        std::string(reinterpret_cast<const char *>(cursortrail_fsh), cursortrail_fsh_size()), "cursortrail");

    this->cursorTrail.reserve(cv::cursor_trail_max_size.getInt() * 2);
    this->cursorTrailVAO = resourceManager->createVertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_QUADS,
                                                                    Graphics::USAGE_TYPE::USAGE_DYNAMIC);

    this->fCurFps = 60.0f;
    this->fCurFpsSmooth = 60.0f;
    this->fFpsUpdate = 0.0f;

    this->fInputoverlayK1AnimScale = 1.0f;
    this->fInputoverlayK2AnimScale = 1.0f;
    this->fInputoverlayM1AnimScale = 1.0f;
    this->fInputoverlayM2AnimScale = 1.0f;

    this->fInputoverlayK1AnimColor = 0.0f;
    this->fInputoverlayK2AnimColor = 0.0f;
    this->fInputoverlayM1AnimColor = 0.0f;
    this->fInputoverlayM2AnimColor = 0.0f;

    this->fAccuracyXOffset = 0.0f;
    this->fAccuracyYOffset = 0.0f;
    this->fScoreHeight = 0.0f;

    this->fComboAnim1 = 0.0f;
    this->fComboAnim2 = 0.0f;

    this->fCursorExpandAnim = 1.0f;

    this->fHealth = 1.0f;
    this->fScoreBarBreakAnim = 0.0f;
    this->fKiScaleAnim = 0.8f;
}

HUD::~HUD() {}

void HUD::draw() {
    const auto &pf = osu->getMapInterface();

    if(cv::draw_hud.getBool()) {
        if(cv::draw_inputoverlay.getBool()) {
            const bool isAutoClicking = (osu->getModAuto() || osu->getModRelax());
            if(!isAutoClicking)
                this->drawInputOverlay(osu->getScore()->getKeyCount(1), osu->getScore()->getKeyCount(2),
                                       osu->getScore()->getKeyCount(3), osu->getScore()->getKeyCount(4));
        }

        this->drawFancyScoreboard();

        g->pushTransform();
        {
            if(osu->getModTarget() && cv::draw_target_heatmap.getBool()) {
                g->translate(0, pf->fHitcircleDiameter *
                                    (1.0f / (cv::hud_scale.getFloat() * cv::hud_statistics_scale.getFloat())));
            }

            auto map = pf->beatmap;
            this->drawStatistics(
                osu->getScore()->getNumMisses(), osu->getScore()->getNumSliderBreaks(), pf->iMaxPossibleCombo,
                this->live_stars, map->pp.total_stars, pf->getMostCommonBPM(), pf->getApproachRateForSpeedMultiplier(),
                pf->getCS(), pf->getOverallDifficultyForSpeedMultiplier(), pf->getHP(), pf->getNPS(), pf->getND(),
                osu->getScore()->getUnstableRate(), this->live_pp, map->pp.pp,
                ((int)pf->getHitWindow300() - 0.5f) * (1.0f / pf->getSpeedMultiplier()),  // see InfoLabel::update()
                osu->getScore()->getHitErrorAvgCustomMin(), osu->getScore()->getHitErrorAvgCustomMax());
        }
        g->popTransform();

        // health anim
        const double currentHealth = pf->getHealth();
        const double elapsedMS = engine->getFrameTime() * 1000.0;
        const double frameAimTime = 1000.0 / 60.0;
        const double frameRatio = elapsedMS / frameAimTime;
        if(this->fHealth < currentHealth) {
            this->fHealth = std::min(1.0, this->fHealth + std::abs(currentHealth - this->fHealth) / 4.0 * frameRatio);
        } else if(this->fHealth > currentHealth) {
            this->fHealth = std::max(0.0, this->fHealth - std::abs(this->fHealth - currentHealth) / 6.0 * frameRatio);
        }

        if(cv::hud_scorebar_hide_during_breaks.getBool()) {
            if(!anim->isAnimating(&this->fScoreBarBreakAnim) && !pf->isWaiting()) {
                if(this->fScoreBarBreakAnim == 0.0f && pf->isInBreak()) {
                    anim->moveLinear(&this->fScoreBarBreakAnim, 1.0f, cv::hud_scorebar_hide_anim_duration.getFloat(),
                                     true);
                } else if(this->fScoreBarBreakAnim == 1.0f && !pf->isInBreak()) {
                    anim->moveLinear(&this->fScoreBarBreakAnim, 0.0f, cv::hud_scorebar_hide_anim_duration.getFloat(),
                                     true);
                }
            }
        } else {
            this->fScoreBarBreakAnim = 0.0f;
        }

        // NOTE: special case for FPoSu, if players manually set fposu_draw_scorebarbg_on_top to 1
        if(cv::draw_scorebarbg.getBool() && cv::mod_fposu.getBool() && cv::fposu_draw_scorebarbg_on_top.getBool())
            this->drawScorebarBg(
                cv::hud_scorebar_hide_during_breaks.getBool() ? (1.0f - pf->getBreakBackgroundFadeAnim()) : 1.0f,
                this->fScoreBarBreakAnim);

        if(cv::draw_scorebar.getBool())
            this->drawHPBar(
                this->fHealth,
                cv::hud_scorebar_hide_during_breaks.getBool() ? (1.0f - pf->getBreakBackgroundFadeAnim()) : 1.0f,
                this->fScoreBarBreakAnim);

        // NOTE: moved to draw behind hitobjects in Beatmap::draw()
        if(cv::mod_fposu.getBool()) {
            if(cv::draw_hiterrorbar.getBool() &&
               (pf == nullptr || (!pf->isSpinnerActive() || !cv::hud_hiterrorbar_hide_during_spinner.getBool())) &&
               !pf->isLoading()) {
                this->drawHitErrorBar(pf->getHitWindow300(), pf->getHitWindow100(), pf->getHitWindow50(),
                                      GameRules::getHitWindowMiss(), osu->getScore()->getUnstableRate());
            }
        }

        if(cv::draw_score.getBool()) this->drawScore(osu->getScore()->getScore());

        if(cv::draw_combo.getBool()) this->drawCombo(osu->getScore()->getCombo());

        // dynamic hud scaling updates
        this->fScoreHeight = osu->getSkin()->getScore0()->getHeight() * this->getScoreScale();

        if(cv::draw_progressbar.getBool()) this->drawProgressBar(pf->getPercentFinishedPlayable(), pf->isWaiting());

        if(cv::draw_accuracy.getBool()) this->drawAccuracy(osu->getScore()->getAccuracy() * 100.0f);

        if(osu->getModTarget() && cv::draw_target_heatmap.getBool()) this->drawTargetHeatmap(pf->fHitcircleDiameter);
    } else if(!cv::hud_shift_tab_toggles_everything.getBool()) {
        if(cv::draw_inputoverlay.getBool()) {
            const bool isAutoClicking = (osu->getModAuto() || osu->getModRelax());
            if(!isAutoClicking)
                this->drawInputOverlay(osu->getScore()->getKeyCount(1), osu->getScore()->getKeyCount(2),
                                       osu->getScore()->getKeyCount(3), osu->getScore()->getKeyCount(4));
        }

        // NOTE: moved to draw behind hitobjects in Beatmap::draw()
        if(cv::mod_fposu.getBool()) {
            if(cv::draw_hiterrorbar.getBool() &&
               (pf == nullptr || (!pf->isSpinnerActive() || !cv::hud_hiterrorbar_hide_during_spinner.getBool())) &&
               !pf->isLoading()) {
                this->drawHitErrorBar(pf->getHitWindow300(), pf->getHitWindow100(), pf->getHitWindow50(),
                                      GameRules::getHitWindowMiss(), osu->getScore()->getUnstableRate());
            }
        }
    }

    if(pf->shouldFlashSectionPass()) this->drawSectionPass(pf->shouldFlashSectionPass());
    if(pf->shouldFlashSectionFail()) this->drawSectionFail(pf->shouldFlashSectionFail());

    if(pf->shouldFlashWarningArrows()) this->drawWarningArrows(pf->fHitcircleDiameter);

    if(pf->isContinueScheduled() && cv::draw_continue.getBool())
        this->drawContinue(pf->getContinueCursorPoint(), pf->fHitcircleDiameter);

    if(cv::draw_scrubbing_timeline.getBool() && osu->isSeeking()) {
        static std::vector<BREAK> breaks;
        breaks.clear();

        if(cv::draw_scrubbing_timeline_breaks.getBool()) {
            const unsigned long lengthPlayableMS = pf->getLengthPlayable();
            const unsigned long startTimePlayableMS = pf->getStartTimePlayable();
            const unsigned long endTimePlayableMS = startTimePlayableMS + lengthPlayableMS;

            const std::vector<DatabaseBeatmap::BREAK> &beatmapBreaks = pf->getBreaks();

            breaks.reserve(beatmapBreaks.size());

            for(auto bk : beatmapBreaks) {
                // ignore breaks after last hitobject
                if(/*bk.endTime <= (int)startTimePlayableMS ||*/ bk.startTime >=
                   (int)(startTimePlayableMS + lengthPlayableMS))
                    continue;

                BREAK bk2;

                bk2.startPercent = (float)(bk.startTime) / (float)(endTimePlayableMS);
                bk2.endPercent = (float)(bk.endTime) / (float)(endTimePlayableMS);

                // debugLog("{:d}: s = {:f}, e = {:f}\n", i, bk2.startPercent, bk2.endPercent);

                breaks.push_back(bk2);
            }
        }

        // Fix percent to include time before first hitobject (HACK)
        f32 true_percent = pf->getPercentFinishedPlayable();
        if(!pf->isWaiting()) {
            f32 true_length = pf->getStartTimePlayable() + pf->getLengthPlayable();
            true_percent = std::clamp(pf->getTime() / true_length, 0.f, 1.f);
        }

        this->drawScrubbingTimeline(pf->getTime(), pf->getLengthPlayable(), pf->getStartTimePlayable(), true_percent,
                                    breaks);
    }

    if(!osu->isSkipScheduled() && pf->isInSkippableSection() &&
       ((cv::skip_intro_enabled.getBool() && pf->iCurrentHitObjectIndex < 1) ||
        (cv::skip_breaks_enabled.getBool() && pf->iCurrentHitObjectIndex > 0)))
        this->drawSkip();

    u32 nb_spectators =
        BanchoState::spectating ? BanchoState::fellow_spectators.size() : BanchoState::spectators.size();
    if(nb_spectators > 0 && cv::draw_spectator_list.getBool()) {
        // XXX: maybe draw player names? avatars?
        const UString str = UString::format("%d spectators", nb_spectators);

        g->pushTransform();
        McFont *font = osu->getSongBrowserFont();
        const float height = roundf(osu->getVirtScreenHeight() * 0.07f);
        const float scale = (height / font->getHeight()) * 0.315f;
        g->scale(scale, scale);
        g->translate(30.f * scale, osu->getVirtScreenHeight() / 2.f - ((height * 2.5f) + font->getHeight() * scale));

        if(cv::background_dim.getFloat() < 0.7f) {
            g->translate(1, 1);
            g->setColor(0x66000000);
            g->drawString(font, str);
            g->translate(-1, -1);
        }

        g->setColor(0xffffffff);
        g->drawString(font, str);
        g->popTransform();
    }

    // target heatmap cleanup
    if(osu->getModTarget()) {
        if(this->targets.size() > 0 && engine->getTime() > this->targets[0].time) {
            this->targets.erase(this->targets.begin());
        }
    }
}

void HUD::drawDummy() {
    this->drawPlayfieldBorder(GameRules::getPlayfieldCenter(), GameRules::getPlayfieldSize(), 0);

    if(cv::draw_scorebarbg.getBool()) this->drawScorebarBg(1.0f, 0.0f);

    if(cv::draw_scorebar.getBool()) this->drawHPBar(1.0, 1.0f, 0.0);

    if(cv::draw_inputoverlay.getBool()) this->drawInputOverlay(0, 0, 0, 0);

    SCORE_ENTRY scoreEntry;
    scoreEntry.name = BanchoState::get_username().c_str();
    scoreEntry.combo = 420;
    scoreEntry.score = 12345678;
    scoreEntry.accuracy = 1.0f;
    scoreEntry.dead = false;
    scoreEntry.highlight = true;
    if((cv::draw_scoreboard.getBool() && !BanchoState::is_playing_a_multi_map()) ||
       (cv::draw_scoreboard_mp.getBool() && BanchoState::is_playing_a_multi_map())) {
        static std::vector<SCORE_ENTRY> scoreEntries;
        scoreEntries.clear();
        {
            scoreEntries.push_back(scoreEntry);
        }
    }

    this->drawSkip();

    this->drawStatistics(0, 0, 727, 2.3f, 5.5f, 180, 9.0f, 4.0f, 8.0f, 6.0f, 4, 6, 90.0f, 123, 1234, 25, -5, 15);

    this->drawWarningArrows();

    if(cv::draw_combo.getBool()) this->drawCombo(scoreEntry.combo);

    if(cv::draw_score.getBool()) this->drawScore(scoreEntry.score);

    this->fScoreHeight = 0.0f;

    if(cv::draw_progressbar.getBool()) this->drawProgressBar(0.25f, false);

    if(cv::draw_accuracy.getBool()) this->drawAccuracy(scoreEntry.accuracy * 100.0f);

    if(cv::draw_hiterrorbar.getBool()) this->drawHitErrorBar(50, 100, 150, 400, 70);
}

void HUD::drawCursor(vec2 pos, float alphaMultiplier, bool secondTrail, bool updateAndDrawTrail) {
    if(cv::draw_cursor_ripples.getBool() && (!cv::mod_fposu.getBool() || !osu->isInPlayMode())) {
        this->drawCursorRipples();
    }

    if(updateAndDrawTrail) {
        auto &trail = secondTrail ? this->cursorTrail2 : this->cursorTrail;
        this->drawCursorTrailInt(this->cursorTrailShader, trail, pos, alphaMultiplier, false);
    }

    Image *cursor = osu->getSkin()->getCursor();
    const float scale = this->getCursorScaleFactor() * (osu->getSkin()->isCursor2x() ? 0.5f : 1.0f);
    const float animatedScale = scale * (osu->getSkin()->getCursorExpand() ? this->fCursorExpandAnim : 1.0f);

    // draw cursor
    g->setColor(Color(0xffffffff).setA(cv::cursor_alpha.getFloat() * alphaMultiplier));

    g->pushTransform();
    {
        g->scale(animatedScale * cv::cursor_scale.getFloat(), animatedScale * cv::cursor_scale.getFloat());

        if(!osu->getSkin()->getCursorCenter())
            g->translate((cursor->getWidth() / 2.0f) * animatedScale * cv::cursor_scale.getFloat(),
                         (cursor->getHeight() / 2.0f) * animatedScale * cv::cursor_scale.getFloat());

        if(osu->getSkin()->getCursorRotate()) g->rotate(fmod(engine->getTime() * 37.0f, 360.0f));

        g->translate(pos.x, pos.y);
        g->drawImage(cursor);
    }
    g->popTransform();

    // draw cursor middle
    if(osu->getSkin()->getCursorMiddle() != osu->getSkin()->getMissingTexture()) {
        g->setColor(Color(0xffffffff).setA(cv::cursor_alpha.getFloat() * alphaMultiplier));

        g->pushTransform();
        {
            g->scale(scale * cv::cursor_scale.getFloat(), scale * cv::cursor_scale.getFloat());
            g->translate(pos.x, pos.y, 0.05f);

            if(!osu->getSkin()->getCursorCenter())
                g->translate(
                    (osu->getSkin()->getCursorMiddle()->getWidth() / 2.0f) * scale * cv::cursor_scale.getFloat(),
                    (osu->getSkin()->getCursorMiddle()->getHeight() / 2.0f) * scale * cv::cursor_scale.getFloat());

            g->drawImage(osu->getSkin()->getCursorMiddle());
        }
        g->popTransform();
    }

    // cursor ripples cleanup
    if(this->cursorRipples.size() > 0 && engine->getTime() > this->cursorRipples[0].time) {
        this->cursorRipples.erase(this->cursorRipples.begin());
    }
}

void HUD::drawCursorTrail(vec2 pos, float alphaMultiplier, bool secondTrail) {
    const bool fposuTrailJumpFix =
        (cv::mod_fposu.getBool() && osu->isInPlayMode() && !osu->getFPoSu()->isCrosshairIntersectingScreen());

    this->drawCursorTrailInt(this->cursorTrailShader, secondTrail ? this->cursorTrail2 : this->cursorTrail, pos,
                             alphaMultiplier, fposuTrailJumpFix);
}

void HUD::drawCursorTrailInt(Shader *trailShader, std::vector<CURSORTRAIL> &trail, vec2 pos, float alphaMultiplier,
                             bool emptyTrailFrame) {
    Image *trailImage = osu->getSkin()->getCursorTrail();

    if(cv::draw_cursor_trail.getBool() && trailImage->isReady()) {
        const bool smoothCursorTrail =
            osu->getSkin()->useSmoothCursorTrail() || cv::cursor_trail_smooth_force.getBool();

        const float trailWidth =
            trailImage->getWidth() * this->getCursorTrailScaleFactor() * cv::cursor_scale.getFloat();
        const float trailHeight =
            trailImage->getHeight() * this->getCursorTrailScaleFactor() * cv::cursor_scale.getFloat();

        if(smoothCursorTrail) this->cursorTrailVAO->empty();

        // add the sample for the current frame
        if(!emptyTrailFrame) this->addCursorTrailPosition(trail, pos);

        // this loop draws the old style trail, and updates the alpha values for each segment, and fills the vao for the
        // new style trail
        const float trailLength =
            smoothCursorTrail ? cv::cursor_trail_smooth_length.getFloat() : cv::cursor_trail_length.getFloat();
        int i = trail.size() - 1;
        while(i >= 0) {
            trail[i].alpha =
                std::clamp<float>(((trail[i].time - engine->getTime()) / trailLength) * alphaMultiplier, 0.0f, 1.0f) *
                cv::cursor_trail_alpha.getFloat();

            if(smoothCursorTrail) {
                const float scaleAnimTrailWidthHalf = (trailWidth / 2) * trail[i].scale;
                const float scaleAnimTrailHeightHalf = (trailHeight / 2) * trail[i].scale;

                const vec3 topLeft = vec3(trail[i].pos.x - scaleAnimTrailWidthHalf,
                                          trail[i].pos.y - scaleAnimTrailHeightHalf, trail[i].alpha);
                this->cursorTrailVAO->addVertex(topLeft);
                this->cursorTrailVAO->addTexcoord(0, 0);

                const vec3 topRight = vec3(trail[i].pos.x + scaleAnimTrailWidthHalf,
                                           trail[i].pos.y - scaleAnimTrailHeightHalf, trail[i].alpha);
                this->cursorTrailVAO->addVertex(topRight);
                this->cursorTrailVAO->addTexcoord(1, 0);

                const vec3 bottomRight = vec3(trail[i].pos.x + scaleAnimTrailWidthHalf,
                                              trail[i].pos.y + scaleAnimTrailHeightHalf, trail[i].alpha);
                this->cursorTrailVAO->addVertex(bottomRight);
                this->cursorTrailVAO->addTexcoord(1, 1);

                const vec3 bottomLeft = vec3(trail[i].pos.x - scaleAnimTrailWidthHalf,
                                             trail[i].pos.y + scaleAnimTrailHeightHalf, trail[i].alpha);
                this->cursorTrailVAO->addVertex(bottomLeft);
                this->cursorTrailVAO->addTexcoord(0, 1);
            } else  // old style trail
            {
                if(trail[i].alpha > 0.0f) this->drawCursorTrailRaw(trail[i].alpha, trail[i].pos);
            }

            i--;
        }

        // draw new style continuous smooth trail
        if(smoothCursorTrail) {
            trailShader->enable();
            {
                trailShader->setUniform1f("time", engine->getTime());

                trailImage->bind();
                {
                    g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ADDITIVE);
                    {
                        g->setColor(0xffffffff);
                        g->drawVAO(this->cursorTrailVAO);
                    }
                    g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
                }
                trailImage->unbind();
            }
            trailShader->disable();
        }
    }

    // trail cleanup
    while((trail.size() > 1 && engine->getTime() > trail[0].time) ||
          trail.size() > cv::cursor_trail_max_size.getInt())  // always leave at least 1 previous entry in there
    {
        trail.erase(trail.begin());
    }
}

void HUD::drawCursorTrailRaw(float alpha, vec2 pos) {
    Image *trailImage = osu->getSkin()->getCursorTrail();
    const float scale = this->getCursorTrailScaleFactor();
    const float animatedScale =
        scale *
        (osu->getSkin()->getCursorExpand() && cv::cursor_trail_expand.getBool() ? this->fCursorExpandAnim : 1.0f) *
        cv::cursor_trail_scale.getFloat();

    g->setColor(Color(0xffffffff).setA(alpha));

    g->pushTransform();
    {
        g->scale(animatedScale * cv::cursor_scale.getFloat(), animatedScale * cv::cursor_scale.getFloat());
        g->translate(pos.x, pos.y);
        g->drawImage(trailImage);
    }
    g->popTransform();
}

void HUD::drawCursorRipples() {
    if(osu->getSkin()->getCursorRipple() == osu->getSkin()->getMissingTexture()) return;

    // allow overscale/underscale as usual
    // this does additionally scale with the resolution (which osu doesn't do for some reason for cursor ripples)
    const float normalized2xScale = (osu->getSkin()->isCursorRipple2x() ? 0.5f : 1.0f);
    const float imageScale = Osu::getImageScale(vec2(520.0f, 520.0f), 233.0f);

    const float normalizedWidth = osu->getSkin()->getCursorRipple()->getWidth() * normalized2xScale * imageScale;
    const float normalizedHeight = osu->getSkin()->getCursorRipple()->getHeight() * normalized2xScale * imageScale;

    const float duration = std::max(cv::cursor_ripple_duration.getFloat(), 0.0001f);
    const float fadeDuration = std::max(
        cv::cursor_ripple_duration.getFloat() - cv::cursor_ripple_anim_start_fadeout_delay.getFloat(), 0.0001f);

    if(cv::cursor_ripple_additive.getBool()) g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ADDITIVE);

    g->setColor(argb(255, std::clamp<int>(cv::cursor_ripple_tint_r.getInt(), 0, 255),
                     std::clamp<int>(cv::cursor_ripple_tint_g.getInt(), 0, 255),
                     std::clamp<int>(cv::cursor_ripple_tint_b.getInt(), 0, 255)));
    osu->getSkin()->getCursorRipple()->bind();
    {
        for(auto &cursorRipple : this->cursorRipples) {
            const vec2 &pos = cursorRipple.pos;
            const float &time = cursorRipple.time;

            const float animPercent = 1.0f - std::clamp<float>((time - engine->getTime()) / duration, 0.0f, 1.0f);
            const float fadePercent = 1.0f - std::clamp<float>((time - engine->getTime()) / fadeDuration, 0.0f, 1.0f);

            const float scale =
                std::lerp(cv::cursor_ripple_anim_start_scale.getFloat(), cv::cursor_ripple_anim_end_scale.getFloat(),
                          1.0f - (1.0f - animPercent) * (1.0f - animPercent));  // quad out

            g->setAlpha(cv::cursor_ripple_alpha.getFloat() * (1.0f - fadePercent));
            g->drawQuad(pos.x - normalizedWidth * scale / 2, pos.y - normalizedHeight * scale / 2,
                        normalizedWidth * scale, normalizedHeight * scale);
        }
    }
    osu->getSkin()->getCursorRipple()->unbind();

    if(cv::cursor_ripple_additive.getBool()) g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
}

void HUD::drawFps() {
    if(cv::hud_fps_smoothing.getBool()) {
        const float smooth = pow(0.05, engine->getFrameTime());
        this->fCurFpsSmooth = smooth * this->fCurFpsSmooth + (1.0f - smooth) * (1.0f / engine->getFrameTime());
        if(engine->getTime() > this->fFpsUpdate || std::abs(this->fCurFpsSmooth - this->fCurFps) > 2.0f) {
            this->fFpsUpdate = engine->getTime() + 0.25f;
            this->fCurFps = this->fCurFpsSmooth;
        }
    } else {
        this->fCurFps = (1.0f / engine->getFrameTime());
    }

    auto font = this->tempFont;
    auto fps = this->fCurFps;

    static double old_worst_frametime = 0.0;
    static double new_worst_frametime = 0.0;
    static double current_second = 0.0;
    if(current_second + 1.0 > engine->getTime()) {
        new_worst_frametime = std::max(new_worst_frametime, engine->getFrameTime());
    } else {
        old_worst_frametime = new_worst_frametime;
        new_worst_frametime = 0.f;
        current_second = engine->getTime();
    }

    fps = std::round(fps);
    const UString fpsString = UString::format("%i fps", (int)(fps));
    const UString msString = UString::format("%.1f ms", old_worst_frametime * 1000.0f);

    const float dpiScale = Osu::getUIScale();

    const int margin = std::round(3.0f * dpiScale);
    const int shadowOffset = std::round(1.0f * dpiScale);

    // shadow
    g->setColor(0xff000000);
    g->pushTransform();
    {
        g->translate(osu->getVirtScreenWidth() - font->getStringWidth(fpsString) - margin + shadowOffset,
                     osu->getVirtScreenHeight() - margin - font->getHeight() - margin + shadowOffset);
        g->drawString(font, fpsString);
    }
    g->popTransform();
    g->pushTransform();
    {
        g->translate(osu->getVirtScreenWidth() - font->getStringWidth(msString) - margin + shadowOffset,
                     osu->getVirtScreenHeight() - margin + shadowOffset);
        g->drawString(font, msString);
    }
    g->popTransform();

    // top

    g->pushTransform();
    {
        if(fps >= 200)
            g->setColor(0xffffffff);
        else if(fps >= 120)
            g->setColor(0xffdddd00);
        else {
            const float pulse = std::abs(std::sin(engine->getTime() * 4));
            g->setColor(argb(1.0f, 1.0f, 0.26f * pulse, 0.26f * pulse));
        }

        g->translate(osu->getVirtScreenWidth() - font->getStringWidth(fpsString) - margin,
                     osu->getVirtScreenHeight() - margin - font->getHeight() - margin);
        g->drawString(font, fpsString);
    }
    g->popTransform();
    g->pushTransform();
    {
        if(old_worst_frametime <= 0.005) {
            g->setColor(0xffffffff);
        } else if(old_worst_frametime <= 0.008) {
            g->setColor(0xffdddd00);
        } else {
            const float pulse = std::abs(std::sin(engine->getTime() * 4));
            g->setColor(argb(1.0f, 1.0f, 0.26f * pulse, 0.26f * pulse));
        }

        g->translate(osu->getVirtScreenWidth() - font->getStringWidth(msString) - margin,
                     osu->getVirtScreenHeight() - margin);
        g->drawString(font, msString);
    }
    g->popTransform();
}

void HUD::drawPlayfieldBorder(vec2 playfieldCenter, vec2 playfieldSize, float hitcircleDiameter) {
    this->drawPlayfieldBorder(playfieldCenter, playfieldSize, hitcircleDiameter,
                              cv::hud_playfield_border_size.getInt());
}

void HUD::drawPlayfieldBorder(vec2 playfieldCenter, vec2 playfieldSize, float hitcircleDiameter, float borderSize) {
    if(borderSize <= 0.0f) return;

    vec2 playfieldBorderTopLeft =
        vec2((int)(playfieldCenter.x - playfieldSize.x / 2 - hitcircleDiameter / 2 - borderSize),
             (int)(playfieldCenter.y - playfieldSize.y / 2 - hitcircleDiameter / 2 - borderSize));
    vec2 playfieldBorderSize =
        vec2((int)(playfieldSize.x + hitcircleDiameter), (int)(playfieldSize.y + hitcircleDiameter));

    const Color innerColor = 0x44ffffff;
    const Color outerColor = 0x00000000;

    g->pushTransform();
    {
        g->translate(0, 0, 0.2f);

        // top
        {
            static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
            vao.empty();

            vao.addVertex(playfieldBorderTopLeft);
            vao.addColor(outerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(playfieldBorderSize.x + borderSize * 2, 0));
            vao.addColor(outerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(playfieldBorderSize.x + borderSize, borderSize));
            vao.addColor(innerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(borderSize, borderSize));
            vao.addColor(innerColor);

            g->drawVAO(&vao);
        }

        // left
        {
            static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
            vao.empty();

            vao.addVertex(playfieldBorderTopLeft);
            vao.addColor(outerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(borderSize, borderSize));
            vao.addColor(innerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(borderSize, playfieldBorderSize.y + borderSize));
            vao.addColor(innerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(0, playfieldBorderSize.y + 2 * borderSize));
            vao.addColor(outerColor);

            g->drawVAO(&vao);
        }

        // right
        {
            static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
            vao.empty();

            vao.addVertex(playfieldBorderTopLeft + vec2(playfieldBorderSize.x + 2 * borderSize, 0));
            vao.addColor(outerColor);
            vao.addVertex(playfieldBorderTopLeft +
                          vec2(playfieldBorderSize.x + 2 * borderSize, playfieldBorderSize.y + 2 * borderSize));
            vao.addColor(outerColor);
            vao.addVertex(playfieldBorderTopLeft +
                          vec2(playfieldBorderSize.x + borderSize, playfieldBorderSize.y + borderSize));
            vao.addColor(innerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(playfieldBorderSize.x + borderSize, borderSize));
            vao.addColor(innerColor);

            g->drawVAO(&vao);
        }

        // bottom
        {
            static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);
            vao.empty();

            vao.addVertex(playfieldBorderTopLeft + vec2(borderSize, playfieldBorderSize.y + borderSize));
            vao.addColor(innerColor);
            vao.addVertex(playfieldBorderTopLeft +
                          vec2(playfieldBorderSize.x + borderSize, playfieldBorderSize.y + borderSize));
            vao.addColor(innerColor);
            vao.addVertex(playfieldBorderTopLeft +
                          vec2(playfieldBorderSize.x + 2 * borderSize, playfieldBorderSize.y + 2 * borderSize));
            vao.addColor(outerColor);
            vao.addVertex(playfieldBorderTopLeft + vec2(0, playfieldBorderSize.y + 2 * borderSize));
            vao.addColor(outerColor);

            g->drawVAO(&vao);
        }
    }
    g->popTransform();
}

void HUD::drawLoadingSmall(const UString &text) {
    const float scale = Osu::getImageScale(osu->getSkin()->getLoadingSpinner(), 29);

    g->setColor(0xffffffff);
    g->pushTransform();
    {
        g->rotate(engine->getTime() * 180, 0, 0, 1);
        g->scale(scale, scale);
        g->translate(osu->getVirtScreenWidth() / 2, osu->getVirtScreenHeight() / 2);
        g->drawImage(osu->getSkin()->getLoadingSpinner());
    }
    g->popTransform();

    float spinner_height = osu->getSkin()->getLoadingSpinner()->getHeight() * scale;
    g->setColor(0x44ffffff);
    g->pushTransform();
    {
        g->translate((int)(osu->getVirtScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(text) / 2),
                     osu->getVirtScreenHeight() / 2 + 2.f * spinner_height);
        g->drawString(osu->getSubTitleFont(), text);
    }
    g->popTransform();
}

void HUD::drawBeatmapImportSpinner() {
    const float scale = Osu::getImageScale(osu->getSkin()->getBeatmapImportSpinner(), 100);

    g->setColor(0xffffffff);
    g->pushTransform();
    {
        g->rotate(engine->getTime() * 180, 0, 0, 1);
        g->scale(scale, scale);
        g->translate(osu->getVirtScreenWidth() / 2, osu->getVirtScreenHeight() / 2);
        g->drawImage(osu->getSkin()->getBeatmapImportSpinner());
    }
    g->popTransform();
}

void HUD::drawScoreNumber(unsigned long long number, float scale, bool drawLeadingZeroes) {
    // get digits
    static std::vector<int> digits;
    digits.clear();
    while(number >= 10) {
        int curDigit = number % 10;
        number /= 10;

        digits.insert(digits.begin(), curDigit);
    }
    digits.insert(digits.begin(), number);
    if(digits.size() == 1) {
        if(drawLeadingZeroes) digits.insert(digits.begin(), 0);
    }

    // draw them
    // NOTE: just using the width here is incorrect, but it is the quickest solution instead of painstakingly
    // reverse-engineering how osu does it
    float lastWidth = osu->getSkin()->getScore0()->getWidth();
    for(int digit : digits) {
        switch(digit) {
            case 0:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore0());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 1:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore1());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 2:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore2());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 3:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore3());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 4:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore4());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 5:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore5());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 6:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore6());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 7:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore7());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 8:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore8());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 9:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getScore9());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
        }

        g->translate(-osu->getSkin()->getScoreOverlap() * (osu->getSkin()->isScore02x() ? 2 : 1) * scale, 0);
    }
}

void HUD::drawComboNumber(unsigned long long number, float scale, bool drawLeadingZeroes) {
    // get digits
    static std::vector<int> digits;
    digits.clear();
    while(number >= 10) {
        int curDigit = number % 10;
        number /= 10;

        digits.insert(digits.begin(), curDigit);
    }
    digits.insert(digits.begin(), number);
    if(digits.size() == 1) {
        if(drawLeadingZeroes) digits.insert(digits.begin(), 0);
    }

    // draw them
    // NOTE: just using the width here is incorrect, but it is the quickest solution instead of painstakingly
    // reverse-engineering how osu does it
    float lastWidth = osu->getSkin()->getCombo0()->getWidth();
    for(int digit : digits) {
        switch(digit) {
            case 0:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo0());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 1:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo1());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 2:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo2());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 3:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo3());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 4:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo4());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 5:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo5());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 6:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo6());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 7:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo7());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 8:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo8());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
            case 9:
                g->translate(lastWidth * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getCombo9());
                g->translate(lastWidth * 0.5f * scale, 0);
                break;
        }

        g->translate(-osu->getSkin()->getComboOverlap() * (osu->getSkin()->isCombo02x() ? 2 : 1) * scale, 0);
    }
}

void HUD::drawComboSimple(int combo, float scale) {
    g->pushTransform();
    {
        this->drawComboNumber(combo, scale);

        // draw 'x' at the end
        if(osu->getSkin()->getComboX() != osu->getSkin()->getMissingTexture()) {
            g->translate(osu->getSkin()->getComboX()->getWidth() * 0.5f * scale, 0);
            g->drawImage(osu->getSkin()->getComboX());
        }
    }
    g->popTransform();
}

void HUD::drawCombo(int combo) {
    g->setColor(0xffffffff);

    const int offset = 5;

    // draw back (anim)
    float animScaleMultiplier = 1.0f + this->fComboAnim2 * cv::combo_anim2_size.getFloat();
    float scale = osu->getImageScale(osu->getSkin()->getCombo0(), 32) * animScaleMultiplier * cv::hud_scale.getFloat() *
                  cv::hud_combo_scale.getFloat();
    if(this->fComboAnim2 > 0.01f) {
        g->setAlpha(this->fComboAnim2 * 0.65f);
        g->pushTransform();
        {
            g->scale(scale, scale);
            g->translate(offset, osu->getVirtScreenHeight() - osu->getSkin()->getCombo0()->getHeight() * scale / 2.0f,
                         0.0f);
            this->drawComboNumber(combo, scale);

            // draw 'x' at the end
            if(osu->getSkin()->getComboX() != osu->getSkin()->getMissingTexture()) {
                g->translate(osu->getSkin()->getComboX()->getWidth() * 0.5f * scale, 0);
                g->drawImage(osu->getSkin()->getComboX());
            }
        }
        g->popTransform();
    }

    // draw front
    g->setAlpha(1.0f);
    const float animPercent = (this->fComboAnim1 < 1.0f ? this->fComboAnim1 : 2.0f - this->fComboAnim1);
    animScaleMultiplier = 1.0f + (0.5f * animPercent * animPercent) * cv::combo_anim1_size.getFloat();
    scale = osu->getImageScale(osu->getSkin()->getCombo0(), 32) * animScaleMultiplier * cv::hud_scale.getFloat() *
            cv::hud_combo_scale.getFloat();
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(offset, osu->getVirtScreenHeight() - osu->getSkin()->getCombo0()->getHeight() * scale / 2.0f,
                     0.0f);
        this->drawComboNumber(combo, scale);

        // draw 'x' at the end
        if(osu->getSkin()->getComboX() != osu->getSkin()->getMissingTexture()) {
            g->translate(osu->getSkin()->getComboX()->getWidth() * 0.5f * scale, 0);
            g->drawImage(osu->getSkin()->getComboX());
        }
    }
    g->popTransform();
}

void HUD::drawScore(unsigned long long score) {
    g->setColor(0xffffffff);

    int numDigits = 1;
    unsigned long long scoreCopy = score;
    while(scoreCopy >= 10) {
        scoreCopy /= 10;
        numDigits++;
    }

    const float scale = this->getScoreScale();
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(
            osu->getVirtScreenWidth() - osu->getSkin()->getScore0()->getWidth() * scale * numDigits +
                osu->getSkin()->getScoreOverlap() * (osu->getSkin()->isScore02x() ? 2 : 1) * scale * (numDigits - 1),
            osu->getSkin()->getScore0()->getHeight() * scale / 2);
        this->drawScoreNumber(score, scale, false);
    }
    g->popTransform();
}

void HUD::drawScorebarBg(float alpha, float breakAnim) {
    if(osu->getSkin()->getScorebarBg()->isMissingTexture()) return;

    const float scale = cv::hud_scale.getFloat() * cv::hud_scorebar_scale.getFloat();
    const float ratio = Osu::getImageScale(vec2(1, 1), 1.0f);

    const vec2 breakAnimOffset = vec2(0, -20.0f * breakAnim) * ratio;
    g->setColor(Color(0xffffffff).setA(alpha * (1.0f - breakAnim)));

    osu->getSkin()->getScorebarBg()->draw(
        (osu->getSkin()->getScorebarBg()->getSize() / 2.0f) * scale + (breakAnimOffset * scale), scale);
}

void HUD::drawSectionPass(float alpha) {
    if(!osu->getSkin()->getSectionPassImage()->isMissingTexture()) {
        g->setColor(Color(0xffffffff).setA(alpha));

        osu->getSkin()->getSectionPassImage()->draw(osu->getVirtScreenSize() / 2.f);
    }
}

void HUD::drawSectionFail(float alpha) {
    if(!osu->getSkin()->getSectionFailImage()->isMissingTexture()) {
        g->setColor(Color(0xffffffff).setA(alpha));

        osu->getSkin()->getSectionFailImage()->draw(osu->getVirtScreenSize() / 2.f);
    }
}

void HUD::drawHPBar(double health, float alpha, float breakAnim) {
    const bool useNewDefault = !osu->getSkin()->getScorebarMarker()->isMissingTexture();

    const float scale = cv::hud_scale.getFloat() * cv::hud_scorebar_scale.getFloat();
    const float ratio = Osu::getImageScale(vec2(1, 1), 1.0f);

    const vec2 colourOffset = (useNewDefault ? vec2(7.5f, 7.8f) : vec2(3.0f, 10.0f)) * ratio;
    const float currentXPosition = (colourOffset.x + (health * osu->getSkin()->getScorebarColour()->getSize().x));
    const vec2 markerOffset =
        (useNewDefault ? vec2(currentXPosition, (8.125f + 2.5f) * ratio) : vec2(currentXPosition, 10.0f * ratio));
    const vec2 breakAnimOffset = vec2(0, -20.0f * breakAnim) * ratio;

    // lerp color depending on health
    if(useNewDefault) {
        if(health < 0.2) {
            const float factor = std::max(0.0, (0.2 - health) / 0.2);
            const float value = std::lerp(0.0f, 1.0f, factor);
            g->setColor(argb(1.0f, value, 0.0f, 0.0f));
        } else if(health < 0.5) {
            const float factor = std::max(0.0, (0.5 - health) / 0.5);
            const float value = std::lerp(1.0f, 0.0f, factor);
            g->setColor(argb(1.0f, value, value, value));
        } else
            g->setColor(0xffffffff);
    } else
        g->setColor(0xffffffff);

    if(breakAnim != 0.0f || alpha != 1.0f) g->setAlpha(alpha * (1.0f - breakAnim));

    // draw health bar fill
    {
        osu->getSkin()->getScorebarColour()->setDrawClipWidthPercent(health);
        osu->getSkin()->getScorebarColour()->draw((osu->getSkin()->getScorebarColour()->getSize() / 2.0f * scale) +
                                                      (colourOffset * scale) + (breakAnimOffset * scale),
                                                  scale);
    }

    // draw ki
    {
        SkinImage *ki = nullptr;

        if(useNewDefault)
            ki = osu->getSkin()->getScorebarMarker();
        else if(osu->getSkin()->getScorebarColour()->isFromDefaultSkin() ||
                !osu->getSkin()->getScorebarKi()->isFromDefaultSkin()) {
            if(health < 0.2)
                ki = osu->getSkin()->getScorebarKiDanger2();
            else if(health < 0.5)
                ki = osu->getSkin()->getScorebarKiDanger();
            else
                ki = osu->getSkin()->getScorebarKi();
        }

        if(ki != nullptr && !ki->isMissingTexture()) {
            if(!useNewDefault || health >= 0.2) {
                ki->draw((markerOffset * scale) + (breakAnimOffset * scale), scale * this->fKiScaleAnim);
            }
        }
    }
}

void HUD::drawAccuracySimple(float accuracy, float scale) {
    // get integer & fractional parts of the number
    const int accuracyInt = (int)accuracy;
    const int accuracyFrac =
        std::clamp<int>(((int)(std::round((accuracy - accuracyInt) * 100.0f))), 0, 99);  // round up

    // draw it
    g->pushTransform();
    {
        this->drawScoreNumber(accuracyInt, scale, true);

        // draw dot '.' between the integer and fractional part
        if(osu->getSkin()->getScoreDot() != osu->getSkin()->getMissingTexture()) {
            g->setColor(0xffffffff);
            g->translate(osu->getSkin()->getScoreDot()->getWidth() * 0.5f * scale, 0);
            g->drawImage(osu->getSkin()->getScoreDot());
            g->translate(osu->getSkin()->getScoreDot()->getWidth() * 0.5f * scale, 0);
            g->translate(-osu->getSkin()->getScoreOverlap() * (osu->getSkin()->isScore02x() ? 2 : 1) * scale, 0);
        }

        this->drawScoreNumber(accuracyFrac, scale, true);

        // draw '%' at the end
        if(osu->getSkin()->getScorePercent() != osu->getSkin()->getMissingTexture()) {
            g->setColor(0xffffffff);
            g->translate(osu->getSkin()->getScorePercent()->getWidth() * 0.5f * scale, 0);
            g->drawImage(osu->getSkin()->getScorePercent());
        }
    }
    g->popTransform();
}

void HUD::drawAccuracy(float accuracy) {
    g->setColor(0xffffffff);

    // get integer & fractional parts of the number
    const int accuracyInt = (int)accuracy;
    const int accuracyFrac =
        std::clamp<int>(((int)(std::round((accuracy - accuracyInt) * 100.0f))), 0, 99);  // round up

    // draw it
    const int offset = 5;
    const float scale = osu->getImageScale(osu->getSkin()->getScore0(), 13) * cv::hud_scale.getFloat() *
                        cv::hud_accuracy_scale.getFloat();
    g->pushTransform();
    {
        const int numDigits = (accuracyInt > 99 ? 5 : 4);
        const float xOffset =
            osu->getSkin()->getScore0()->getWidth() * scale * numDigits +
            (osu->getSkin()->getScoreDot() != osu->getSkin()->getMissingTexture()
                 ? osu->getSkin()->getScoreDot()->getWidth()
                 : 0) *
                scale +
            (osu->getSkin()->getScorePercent() != osu->getSkin()->getMissingTexture()
                 ? osu->getSkin()->getScorePercent()->getWidth()
                 : 0) *
                scale -
            osu->getSkin()->getScoreOverlap() * (osu->getSkin()->isScore02x() ? 2 : 1) * scale * (numDigits + 1);

        this->fAccuracyXOffset = osu->getVirtScreenWidth() - xOffset - offset;
        this->fAccuracyYOffset = (cv::draw_score.getBool() ? this->fScoreHeight : 0.0f) +
                                 osu->getSkin()->getScore0()->getHeight() * scale / 2 + offset * 2;

        g->scale(scale, scale);
        g->translate(this->fAccuracyXOffset, this->fAccuracyYOffset);

        this->drawScoreNumber(accuracyInt, scale, true);

        // draw dot '.' between the integer and fractional part
        if(osu->getSkin()->getScoreDot() != osu->getSkin()->getMissingTexture()) {
            g->setColor(0xffffffff);
            g->translate(osu->getSkin()->getScoreDot()->getWidth() * 0.5f * scale, 0);
            g->drawImage(osu->getSkin()->getScoreDot());
            g->translate(osu->getSkin()->getScoreDot()->getWidth() * 0.5f * scale, 0);
            g->translate(-osu->getSkin()->getScoreOverlap() * (osu->getSkin()->isScore02x() ? 2 : 1) * scale, 0);
        }

        this->drawScoreNumber(accuracyFrac, scale, true);

        // draw '%' at the end
        if(osu->getSkin()->getScorePercent() != osu->getSkin()->getMissingTexture()) {
            g->setColor(0xffffffff);
            g->translate(osu->getSkin()->getScorePercent()->getWidth() * 0.5f * scale, 0);
            g->drawImage(osu->getSkin()->getScorePercent());
        }
    }
    g->popTransform();
}

void HUD::drawSkip() {
    const float scale = cv::hud_scale.getFloat();

    g->setColor(0xffffffff);
    osu->getSkin()->getPlaySkip()->draw(
        osu->getVirtScreenSize() - (osu->getSkin()->getPlaySkip()->getSize() / 2.f) * scale, cv::hud_scale.getFloat());
}

void HUD::drawWarningArrow(vec2 pos, bool flipVertically, bool originLeft) {
    const float scale = cv::hud_scale.getFloat() * osu->getImageScale(osu->getSkin()->getPlayWarningArrow(), 78);

    g->pushTransform();
    {
        g->scale(flipVertically ? -scale : scale, scale);
        g->translate(pos.x + (flipVertically ? (-osu->getSkin()->getPlayWarningArrow()->getWidth() * scale / 2.0f)
                                             : (osu->getSkin()->getPlayWarningArrow()->getWidth() * scale / 2.0f)) *
                                 (originLeft ? 1.0f : -1.0f),
                     pos.y, 0.0f);
        g->drawImage(osu->getSkin()->getPlayWarningArrow());
    }
    g->popTransform();
}

void HUD::drawWarningArrows(float /*hitcircleDiameter*/) {
    const float divider = 18.0f;
    const float part = GameRules::getPlayfieldSize().y * (1.0f / divider);

    g->setColor(0xffffffff);
    this->drawWarningArrow(
        vec2(osu->getUIScale(28), GameRules::getPlayfieldCenter().y - GameRules::getPlayfieldSize().y / 2 + part * 2),
        false);
    this->drawWarningArrow(vec2(osu->getUIScale(28), GameRules::getPlayfieldCenter().y -
                                                         GameRules::getPlayfieldSize().y / 2 + part * 2 + part * 13),
                           false);

    this->drawWarningArrow(vec2(osu->getVirtScreenWidth() - osu->getUIScale(28),
                                GameRules::getPlayfieldCenter().y - GameRules::getPlayfieldSize().y / 2 + part * 2),
                           true);
    this->drawWarningArrow(
        vec2(osu->getVirtScreenWidth() - osu->getUIScale(28),
             GameRules::getPlayfieldCenter().y - GameRules::getPlayfieldSize().y / 2 + part * 2 + part * 13),
        true);
}

std::vector<SCORE_ENTRY> HUD::getCurrentScores() {
    static std::vector<SCORE_ENTRY> scores;
    scores.clear();

    const auto &pf = osu->getMapInterface();

    if(BanchoState::is_in_a_multi_room()) {
        for(auto &i : BanchoState::room.slots) {
            auto slot = &i;
            if(!slot->is_player_playing() && !slot->has_finished_playing()) continue;

            if(slot->player_id == BanchoState::get_uid()) {
                // Update local player slot instantly
                // (not including fields that won't be used for the HUD)
                slot->num300 = (u16)osu->getScore()->getNum300s();
                slot->num100 = (u16)osu->getScore()->getNum100s();
                slot->num50 = (u16)osu->getScore()->getNum50s();
                slot->num_miss = (u16)osu->getScore()->getNumMisses();
                slot->current_combo = (u16)osu->getScore()->getCombo();
                slot->total_score = (i32)osu->getScore()->getScore();
                slot->current_hp = pf->getHealth() * 200;
            }

            auto user_info = BANCHO::User::get_user_info(slot->player_id, true);

            SCORE_ENTRY scoreEntry;
            scoreEntry.entry_id = slot->player_id;
            scoreEntry.player_id = slot->player_id;
            scoreEntry.name = user_info->name;
            scoreEntry.combo = slot->current_combo;
            scoreEntry.score = slot->total_score;
            scoreEntry.dead = (slot->current_hp == 0);
            scoreEntry.highlight = (slot->player_id == BanchoState::get_uid());

            if(slot->has_quit()) {
                slot->current_hp = 0;
                scoreEntry.name = UString::format("%s [quit]", user_info->name.toUtf8());
            } else if(pf->isInSkippableSection() && pf->iCurrentHitObjectIndex < 1) {
                if(slot->skipped) {
                    // XXX: Draw pretty "Skip" image instead
                    scoreEntry.name = UString::format("%s [skip]", user_info->name.toUtf8());
                }
            }

            // hit_score != total_score: total_score also accounts for spinner bonus & mods
            u64 hit_score = 300 * slot->num300 + 100 * slot->num100 + 50 * slot->num50;
            u64 max_score = 300 * (slot->num300 + slot->num100 + slot->num50 + slot->num_miss);
            scoreEntry.accuracy = max_score > 0 ? hit_score / max_score : 0.f;

            scores.push_back(std::move(scoreEntry));
        }
    } else {
        std::scoped_lock lock(db->scores_mtx);
        std::vector<FinishedScore> *singleplayer_scores = &((*db->getScores())[this->beatmap_md5]);
        bool is_online = cv::songbrowser_scores_filteringtype.getString() != "Local";
        if(is_online) {
            auto search = db->online_scores.find(this->beatmap_md5);
            if(search != db->online_scores.end()) {
                singleplayer_scores = &search->second;
            }
        }

        int nb_slots = 0;
        const auto &scores_ref = *singleplayer_scores;
        for(const auto &score : scores_ref) {
            SCORE_ENTRY scoreEntry;
            scoreEntry.entry_id = -(nb_slots + 1);
            scoreEntry.player_id = score.player_id;
            scoreEntry.name = score.playerName.c_str();
            scoreEntry.combo = score.comboMax;
            scoreEntry.score = score.score;
            scoreEntry.accuracy =
                LiveScore::calculateAccuracy(score.num300s, score.num100s, score.num50s, score.numMisses);
            scoreEntry.dead = false;
            scoreEntry.highlight = false;
            scores.push_back(std::move(scoreEntry));
            nb_slots++;
        }

        SCORE_ENTRY playerScoreEntry;
        if(osu->getModAuto() || (osu->getModAutopilot() && osu->getModRelax())) {
            playerScoreEntry.name = "neosu";
        } else if(pf->is_watching || BanchoState::spectating) {
            playerScoreEntry.name = osu->watched_user_name;
            playerScoreEntry.player_id = osu->watched_user_id;
        } else {
            playerScoreEntry.name = BanchoState::get_username().c_str();
            playerScoreEntry.player_id = BanchoState::get_uid();
        }
        playerScoreEntry.entry_id = 0;
        playerScoreEntry.combo = osu->getScore()->getComboMax();
        playerScoreEntry.score = osu->getScore()->getScore();
        playerScoreEntry.accuracy = osu->getScore()->getAccuracy();
        playerScoreEntry.dead = osu->getScore()->isDead();
        playerScoreEntry.highlight = true;
        scores.push_back(std::move(playerScoreEntry));
        nb_slots++;
    }

    WinCondition sorting_type =
        BanchoState::is_in_a_multi_room() ? (WinCondition)BanchoState::room.win_condition : SCOREV1;
    std::ranges::sort(scores, [sorting_type](const SCORE_ENTRY &a, const SCORE_ENTRY &b) {
        if(sorting_type == ACCURACY) {
            return a.accuracy > b.accuracy;
        } else if(sorting_type == COMBO) {
            // NOTE: I'm aware that 'combo' in SCORE_ENTRY represents the current combo.
            //       That's how the win condition actually works, though. lol
            return a.combo > b.combo;
        } else {
            return a.score > b.score;
        }
    });

    return scores;
}

void HUD::resetScoreboard() {
    DatabaseBeatmap *map = osu->getMapInterface()->beatmap;
    if(map == nullptr) return;

    this->beatmap_md5 = map->getMD5Hash();
    this->player_slot = nullptr;
    this->slots.clear();

    int player_entry_id = BanchoState::is_in_a_multi_room() ? BanchoState::get_uid() : 0;
    auto scores = this->getCurrentScores();
    int i = 0;
    for(const auto &score : scores) {
        auto slot = std::make_shared<ScoreboardSlot>(score, i);
        if(score.entry_id == player_entry_id) {
            this->player_slot = slot;
        }
        this->slots.push_back(slot);
        i++;
    }

    this->updateScoreboard(false);
}

void HUD::updateScoreboard(bool animate) {
    DatabaseBeatmap *map = osu->getMapInterface()->beatmap;
    if(map == nullptr) return;

    if(!cv::scoreboard_animations.getBool()) {
        animate = false;
    }

    // Update player slot first
    auto new_scores = this->getCurrentScores();
    for(int i = 0; i < new_scores.size(); i++) {
        if(new_scores[i].entry_id != this->player_slot->score.entry_id) continue;

        this->player_slot->updateIndex(i, true, animate);
        this->player_slot->score = new_scores[i];
        break;
    }

    // Update other slots
    for(int i = 0; i < new_scores.size(); i++) {
        if(new_scores[i].entry_id == this->player_slot->score.entry_id) continue;

        for(const auto &slot : this->slots) {
            if(slot->score.entry_id != new_scores[i].entry_id) continue;

            slot->updateIndex(i, false, animate);
            slot->score = new_scores[i];
            break;
        }
    }
}

void HUD::drawFancyScoreboard() {
    for(const auto &slot : this->slots) {
        slot->draw();
    }
}

void HUD::drawContinue(vec2 cursor, float hitcircleDiameter) {
    Image *unpause = osu->getSkin()->getUnpause();
    const float unpauseScale = Osu::getImageScale(unpause, 80);

    Image *cursorImage = osu->getSkin()->getDefaultCursor();
    const float cursorScale =
        Osu::getImageScaleToFitResolution(cursorImage, vec2(hitcircleDiameter, hitcircleDiameter));

    // bleh
    if(cursor.x < cursorImage->getWidth() || cursor.y < cursorImage->getHeight() ||
       cursor.x > osu->getVirtScreenWidth() - cursorImage->getWidth() ||
       cursor.y > osu->getVirtScreenHeight() - cursorImage->getHeight())
        cursor = osu->getVirtScreenSize() / 2.f;

    // base
    g->setColor(argb(255, 255, 153, 51));
    g->pushTransform();
    {
        g->scale(cursorScale, cursorScale);
        g->translate(cursor.x, cursor.y);
        g->drawImage(cursorImage);
    }
    g->popTransform();

    // pulse animation
    const float cursorAnimPulsePercent = std::clamp<float>(fmod(engine->getTime(), 1.35f), 0.0f, 1.0f);
    g->setColor(argb((short)(255.0f * (1.0f - cursorAnimPulsePercent)), 255, 153, 51));
    g->pushTransform();
    {
        g->scale(cursorScale * (1.0f + cursorAnimPulsePercent), cursorScale * (1.0f + cursorAnimPulsePercent));
        g->translate(cursor.x, cursor.y);
        g->drawImage(cursorImage);
    }
    g->popTransform();

    // unpause click message
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        g->scale(unpauseScale, unpauseScale);
        g->translate(cursor.x + 20 + (unpause->getWidth() / 2) * unpauseScale,
                     cursor.y + 20 + (unpause->getHeight() / 2) * unpauseScale);
        g->drawImage(unpause);
    }
    g->popTransform();
}

void HUD::drawHitErrorBar(BeatmapInterface *pf) {
    if(cv::draw_hud.getBool() || !cv::hud_shift_tab_toggles_everything.getBool()) {
        if(cv::draw_hiterrorbar.getBool() &&
           (!pf->isSpinnerActive() || !cv::hud_hiterrorbar_hide_during_spinner.getBool()) && !pf->isLoading())
            this->drawHitErrorBar(pf->getHitWindow300(), pf->getHitWindow100(), pf->getHitWindow50(),
                                  GameRules::getHitWindowMiss(), osu->getScore()->getUnstableRate());
    }
}

void HUD::drawHitErrorBar(float hitWindow300, float hitWindow100, float hitWindow50, float hitWindowMiss, int ur) {
    const vec2 center = vec2(osu->getVirtScreenWidth() / 2.0f,
                             osu->getVirtScreenHeight() -
                                 osu->getVirtScreenHeight() * 2.15f * cv::hud_hiterrorbar_height_percent.getFloat() *
                                     cv::hud_scale.getFloat() * cv::hud_hiterrorbar_scale.getFloat() -
                                 osu->getVirtScreenHeight() * cv::hud_hiterrorbar_offset_percent.getFloat());

    if(cv::draw_hiterrorbar_bottom.getBool()) {
        g->pushTransform();
        {
            const vec2 localCenter = vec2(center.x, center.y - (osu->getVirtScreenHeight() *
                                                                cv::hud_hiterrorbar_offset_bottom_percent.getFloat()));

            this->drawHitErrorBarInt2(localCenter, ur);
            g->translate(localCenter.x, localCenter.y);
            this->drawHitErrorBarInt(hitWindow300, hitWindow100, hitWindow50, hitWindowMiss);
        }
        g->popTransform();
    }

    if(cv::draw_hiterrorbar_top.getBool()) {
        g->pushTransform();
        {
            const vec2 localCenter =
                vec2(center.x, osu->getVirtScreenHeight() - center.y +
                                   (osu->getVirtScreenHeight() * cv::hud_hiterrorbar_offset_top_percent.getFloat()));

            g->scale(1, -1);
            // drawHitErrorBarInt2(localCenter, ur);
            g->translate(localCenter.x, localCenter.y);
            this->drawHitErrorBarInt(hitWindow300, hitWindow100, hitWindow50, hitWindowMiss);
        }
        g->popTransform();
    }

    if(cv::draw_hiterrorbar_left.getBool()) {
        g->pushTransform();
        {
            const vec2 localCenter =
                vec2(osu->getVirtScreenHeight() - center.y +
                         (osu->getVirtScreenWidth() * cv::hud_hiterrorbar_offset_left_percent.getFloat()),
                     osu->getVirtScreenHeight() / 2.0f);

            g->rotate(90);
            // drawHitErrorBarInt2(localCenter, ur);
            g->translate(localCenter.x, localCenter.y);
            this->drawHitErrorBarInt(hitWindow300, hitWindow100, hitWindow50, hitWindowMiss);
        }
        g->popTransform();
    }

    if(cv::draw_hiterrorbar_right.getBool()) {
        g->pushTransform();
        {
            const vec2 localCenter =
                vec2(osu->getVirtScreenWidth() - (osu->getVirtScreenHeight() - center.y) -
                         (osu->getVirtScreenWidth() * cv::hud_hiterrorbar_offset_right_percent.getFloat()),
                     osu->getVirtScreenHeight() / 2.0f);

            g->scale(-1, 1);
            g->rotate(-90);
            // drawHitErrorBarInt2(localCenter, ur);
            g->translate(localCenter.x, localCenter.y);
            this->drawHitErrorBarInt(hitWindow300, hitWindow100, hitWindow50, hitWindowMiss);
        }
        g->popTransform();
    }
}

void HUD::drawHitErrorBarInt(float hitWindow300, float hitWindow100, float hitWindow50, float hitWindowMiss) {
    const float alpha = cv::hud_hiterrorbar_alpha.getFloat();
    if(alpha <= 0.0f) return;

    const float alphaEntry = alpha * cv::hud_hiterrorbar_entry_alpha.getFloat();
    const int alphaCenterlineInt =
        std::clamp<int>((int)(alpha * cv::hud_hiterrorbar_centerline_alpha.getFloat() * 255.0f), 0, 255);
    const int alphaBarInt = std::clamp<int>((int)(alpha * cv::hud_hiterrorbar_bar_alpha.getFloat() * 255.0f), 0, 255);

    const Color color300 = argb(alphaBarInt, std::clamp<int>(cv::hud_hiterrorbar_entry_300_r.getInt(), 0, 255),
                                std::clamp<int>(cv::hud_hiterrorbar_entry_300_g.getInt(), 0, 255),
                                std::clamp<int>(cv::hud_hiterrorbar_entry_300_b.getInt(), 0, 255));
    const Color color100 = argb(alphaBarInt, std::clamp<int>(cv::hud_hiterrorbar_entry_100_r.getInt(), 0, 255),
                                std::clamp<int>(cv::hud_hiterrorbar_entry_100_g.getInt(), 0, 255),
                                std::clamp<int>(cv::hud_hiterrorbar_entry_100_b.getInt(), 0, 255));
    const Color color50 = argb(alphaBarInt, std::clamp<int>(cv::hud_hiterrorbar_entry_50_r.getInt(), 0, 255),
                               std::clamp<int>(cv::hud_hiterrorbar_entry_50_g.getInt(), 0, 255),
                               std::clamp<int>(cv::hud_hiterrorbar_entry_50_b.getInt(), 0, 255));
    const Color colorMiss = argb(alphaBarInt, std::clamp<int>(cv::hud_hiterrorbar_entry_miss_r.getInt(), 0, 255),
                                 std::clamp<int>(cv::hud_hiterrorbar_entry_miss_g.getInt(), 0, 255),
                                 std::clamp<int>(cv::hud_hiterrorbar_entry_miss_b.getInt(), 0, 255));

    vec2 size = vec2(osu->getVirtScreenWidth() * cv::hud_hiterrorbar_width_percent.getFloat(),
                     osu->getVirtScreenHeight() * cv::hud_hiterrorbar_height_percent.getFloat()) *
                cv::hud_scale.getFloat() * cv::hud_hiterrorbar_scale.getFloat();
    if(cv::hud_hiterrorbar_showmisswindow.getBool())
        size = vec2(osu->getVirtScreenWidth() * cv::hud_hiterrorbar_width_percent_with_misswindow.getFloat(),
                    osu->getVirtScreenHeight() * cv::hud_hiterrorbar_height_percent.getFloat()) *
               cv::hud_scale.getFloat() * cv::hud_hiterrorbar_scale.getFloat();

    const vec2 center = vec2(0, 0);  // NOTE: moved to drawHitErrorBar()

    const float entryHeight = size.y * cv::hud_hiterrorbar_bar_height_scale.getFloat();
    const float entryWidth = size.y * cv::hud_hiterrorbar_bar_width_scale.getFloat();

    float totalHitWindowLength = hitWindow50;
    if(cv::hud_hiterrorbar_showmisswindow.getBool()) totalHitWindowLength = hitWindowMiss;

    const float percent50 = hitWindow50 / totalHitWindowLength;
    const float percent100 = hitWindow100 / totalHitWindowLength;
    const float percent300 = hitWindow300 / totalHitWindowLength;

    // draw background bar with color indicators for 300s, 100s and 50s (and the miss window)
    if(alphaBarInt > 0) {
        const bool half = cv::mod_halfwindow.getBool();
        const bool halfAllow300s = cv::mod_halfwindow_allow_300s.getBool();

        if(cv::hud_hiterrorbar_showmisswindow.getBool()) {
            g->setColor(colorMiss);
            g->fillRect(center.x - size.x / 2.0f, center.y - size.y / 2.0f, size.x, size.y);
        }

        if(!cv::mod_no100s.getBool() && !cv::mod_no50s.getBool()) {
            g->setColor(color50);
            g->fillRect(center.x - size.x * percent50 / 2.0f, center.y - size.y / 2.0f,
                        size.x * percent50 * (half ? 0.5f : 1.0f), size.y);
        }

        if(!cv::mod_ming3012.getBool() && !cv::mod_no100s.getBool()) {
            g->setColor(color100);
            g->fillRect(center.x - size.x * percent100 / 2.0f, center.y - size.y / 2.0f,
                        size.x * percent100 * (half ? 0.5f : 1.0f), size.y);
        }

        g->setColor(color300);
        g->fillRect(center.x - size.x * percent300 / 2.0f, center.y - size.y / 2.0f,
                    size.x * percent300 * (half && !halfAllow300s ? 0.5f : 1.0f), size.y);
    }

    // draw hit errors
    {
        if(cv::hud_hiterrorbar_entry_additive.getBool()) g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ADDITIVE);

        const bool modMing3012 = cv::mod_ming3012.getBool();
        const float hitFadeDuration = cv::hud_hiterrorbar_entry_hit_fade_time.getFloat();
        const float missFadeDuration = cv::hud_hiterrorbar_entry_miss_fade_time.getFloat();
        for(int i = this->hiterrors.size() - 1; i >= 0; i--) {
            const float percent =
                std::clamp<float>((float)this->hiterrors[i].delta / (float)totalHitWindowLength, -5.0f, 5.0f);
            float fade = std::clamp<float>(
                (this->hiterrors[i].time - engine->getTime()) /
                    (this->hiterrors[i].miss || this->hiterrors[i].misaim ? missFadeDuration : hitFadeDuration),
                0.0f, 1.0f);
            fade *= fade;  // quad out

            Color barColor;
            {
                if(this->hiterrors[i].miss || this->hiterrors[i].misaim)
                    barColor = colorMiss;
                else
                    barColor = (std::abs(percent) <= percent300
                                    ? color300
                                    : (std::abs(percent) <= percent100 && !modMing3012 ? color100 : color50));
            }

            g->setColor(Color(barColor).setA(alphaEntry * fade));

            float missHeightMultiplier = 1.0f;
            if(this->hiterrors[i].miss) missHeightMultiplier = 1.5f;
            if(this->hiterrors[i].misaim) missHeightMultiplier = 4.0f;

            g->fillRect(center.x - (entryWidth / 2.0f) + percent * (size.x / 2.0f),
                        center.y - (entryHeight * missHeightMultiplier) / 2.0f, entryWidth,
                        (entryHeight * missHeightMultiplier));
        }

        if(cv::hud_hiterrorbar_entry_additive.getBool()) g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
    }

    // white center line
    if(alphaCenterlineInt > 0) {
        g->setColor(argb(alphaCenterlineInt, std::clamp<int>(cv::hud_hiterrorbar_centerline_r.getInt(), 0, 255),
                         std::clamp<int>(cv::hud_hiterrorbar_centerline_g.getInt(), 0, 255),
                         std::clamp<int>(cv::hud_hiterrorbar_centerline_b.getInt(), 0, 255)));
        g->fillRect(center.x - entryWidth / 2.0f / 2.0f, center.y - entryHeight / 2.0f, entryWidth / 2.0f, entryHeight);
    }
}

void HUD::drawHitErrorBarInt2(vec2 center, int ur) {
    const float alpha = cv::hud_hiterrorbar_alpha.getFloat() * cv::hud_hiterrorbar_ur_alpha.getFloat();
    if(alpha <= 0.0f) return;

    const float dpiScale = Osu::getUIScale();

    const float hitErrorBarSizeY = osu->getVirtScreenHeight() * cv::hud_hiterrorbar_height_percent.getFloat() *
                                   cv::hud_scale.getFloat() * cv::hud_hiterrorbar_scale.getFloat();
    const float entryHeight = hitErrorBarSizeY * cv::hud_hiterrorbar_bar_height_scale.getFloat();

    if(cv::draw_hiterrorbar_ur.getBool()) {
        g->pushTransform();
        {
            UString urText = UString::format("%i UR", ur);
            McFont *urTextFont = osu->getSongBrowserFont();

            const float hitErrorBarScale = cv::hud_scale.getFloat() * cv::hud_hiterrorbar_scale.getFloat();
            const float urTextScale = hitErrorBarScale * cv::hud_hiterrorbar_ur_scale.getFloat() * 0.5f;
            const float urTextWidth = urTextFont->getStringWidth(urText) * urTextScale;
            const float urTextHeight = urTextFont->getHeight() * hitErrorBarScale;

            g->scale(urTextScale, urTextScale);
            g->translate(
                (int)(center.x + (-urTextWidth / 2.0f) +
                      (urTextHeight) * (cv::hud_hiterrorbar_ur_offset_x_percent.getFloat()) * dpiScale) +
                    1,
                (int)(center.y + (urTextHeight) * (cv::hud_hiterrorbar_ur_offset_y_percent.getFloat()) * dpiScale -
                      entryHeight / 1.25f) +
                    1);

            // shadow
            g->setColor(Color(0xff000000).setA(alpha));

            g->drawString(urTextFont, urText);

            g->translate(-1, -1);

            // text
            g->setColor(Color(0xffffffff).setA(alpha));

            g->drawString(urTextFont, urText);
        }
        g->popTransform();
    }
}

void HUD::drawProgressBar(float percent, bool waiting) {
    if(!cv::draw_accuracy.getBool()) this->fAccuracyXOffset = osu->getVirtScreenWidth();

    const float num_segments = 15 * 8;
    const int offset = 20;
    const float radius = osu->getUIScale(10.5f) * cv::hud_scale.getFloat() * cv::hud_progressbar_scale.getFloat();
    const float circularMetreScale =
        ((2 * radius) / osu->getSkin()->getCircularmetre()->getWidth()) * 1.3f;  // hardcoded 1.3 multiplier?!
    const float actualCircularMetreScale = ((2 * radius) / osu->getSkin()->getCircularmetre()->getWidth());
    vec2 center = vec2(this->fAccuracyXOffset - radius - offset, this->fAccuracyYOffset);

    // clamp to top edge of screen
    if(center.y - (osu->getSkin()->getCircularmetre()->getHeight() * actualCircularMetreScale + 5) / 2.0f < 0)
        center.y += std::abs(center.y -
                             (osu->getSkin()->getCircularmetre()->getHeight() * actualCircularMetreScale + 5) / 2.0f);

    // clamp to bottom edge of score numbers
    if(cv::draw_score.getBool() && center.y - radius < this->fScoreHeight) center.y = this->fScoreHeight + radius;

    const float theta = 2 * PI / float(num_segments);
    const float s = sinf(theta);  // precalculate the sine and cosine
    const float c = cosf(theta);
    float t;
    float x = 0;
    float y = -radius;  // we start at the top

    if(waiting)
        g->setColor(0xaa00f200);
    else
        g->setColor(0xaaf2f2f2);

    {
        static VertexArrayObject vao;
        vao.empty();

        vec2 prevVertex{0.f};
        for(int i = 0; i < num_segments + 1; i++) {
            float curPercent = (i * (360.0f / num_segments)) / 360.0f;
            if(curPercent > percent) break;

            // build current vertex
            vec2 curVertex = vec2(x + center.x, y + center.y);

            // add vertex, triangle strip style (counter clockwise)
            if(i > 0) {
                vao.addVertex(curVertex);
                vao.addVertex(prevVertex);
                vao.addVertex(center);
            }

            // apply the rotation
            t = x;
            x = c * x - s * y;
            y = s * t + c * y;

            // save
            prevVertex = curVertex;
        }

        // draw it
        /// g->setAntialiasing(true); // commented for now
        g->drawVAO(&vao);
        /// g->setAntialiasing(false);
    }

    // draw circularmetre
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        g->scale(circularMetreScale, circularMetreScale);
        g->translate(center.x, center.y, 0.65f);
        g->drawImage(osu->getSkin()->getCircularmetre());
    }
    g->popTransform();
}

void HUD::drawStatistics(int misses, int sliderbreaks, int maxPossibleCombo, float liveStars, float totalStars, int bpm,
                         float ar, float cs, float od, float hp, int nps, int nd, int ur, float pp, float ppfc,
                         float hitWindow300, int hitdeltaMin, int hitdeltaMax) {
    McFont *font = osu->getTitleFont();
    const float offsetScale = Osu::getImageScale(vec2(1.0f, 1.0f), 1.0f);
    const float scale = cv::hud_statistics_scale.getFloat() * cv::hud_scale.getFloat();
    const float yDelta = (font->getHeight() + 10) * cv::hud_statistics_spacing_scale.getFloat();

    static constexpr Color shadowColor = rgb(0, 0, 0);
    static constexpr Color textColor = rgb(255, 255, 255);

    font->beginBatch();

    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(cv::hud_statistics_offset_x.getInt(),
                     (int)(font->getHeight() * scale) + (cv::hud_statistics_offset_y.getInt() * offsetScale));

        float currentY = 0;

        auto addStatistic = [&](const UString &text, float xOffset, float yOffset) {
            if(text.length() < 1) return;

            const vec3 shadowPos(xOffset, currentY + yOffset, 0.25f);
            font->addToBatch(text, shadowPos, shadowColor);

            const vec3 textPos(xOffset - 2, currentY + yOffset - 2, 0.325f);
            font->addToBatch(text, textPos, textColor);

            currentY += yDelta;
        };

        if(cv::draw_statistics_pp.getBool())
            addStatistic((cv::hud_statistics_pp_decimal_places.getInt() < 1
                              ? UString::fmt("{:d}pp", (int)std::round(pp))
                              : (cv::hud_statistics_pp_decimal_places.getInt() > 1 ? UString::fmt("{:.2f}pp", pp)
                                                                                   : UString::fmt("{:.1f}pp", pp))),
                         cv::hud_statistics_pp_offset_x.getInt(), cv::hud_statistics_pp_offset_y.getInt());

        if(cv::draw_statistics_perfectpp.getBool())
            addStatistic(
                (cv::hud_statistics_pp_decimal_places.getInt() < 1
                     ? UString::fmt("SS: {:d}pp", (int)std::round(ppfc))
                     : (cv::hud_statistics_pp_decimal_places.getInt() > 1 ? UString::fmt("SS: {:.2f}pp", ppfc)
                                                                          : UString::fmt("SS: {:.1f}pp", ppfc))),
                cv::hud_statistics_perfectpp_offset_x.getInt(), cv::hud_statistics_perfectpp_offset_y.getInt());

        if(cv::draw_statistics_misses.getBool())
            addStatistic(UString::fmt("Miss: {:d}", misses), cv::hud_statistics_misses_offset_x.getInt(),
                         cv::hud_statistics_misses_offset_y.getInt());

        if(cv::draw_statistics_sliderbreaks.getBool())
            addStatistic(UString::fmt("SBrk: {:d}", sliderbreaks), cv::hud_statistics_sliderbreaks_offset_x.getInt(),
                         cv::hud_statistics_sliderbreaks_offset_y.getInt());

        if(cv::draw_statistics_maxpossiblecombo.getBool())
            addStatistic(UString::fmt("FC: {:d}x", maxPossibleCombo),
                         cv::hud_statistics_maxpossiblecombo_offset_x.getInt(),
                         cv::hud_statistics_maxpossiblecombo_offset_y.getInt());

        if(cv::draw_statistics_livestars.getBool())
            addStatistic(UString::fmt("{:.3g}***", liveStars), cv::hud_statistics_livestars_offset_x.getInt(),
                         cv::hud_statistics_livestars_offset_y.getInt());

        if(cv::draw_statistics_totalstars.getBool())
            addStatistic(UString::fmt("{:.3g}*", totalStars), cv::hud_statistics_totalstars_offset_x.getInt(),
                         cv::hud_statistics_totalstars_offset_y.getInt());

        if(cv::draw_statistics_bpm.getBool())
            addStatistic(UString::fmt("BPM: {:d}", bpm), cv::hud_statistics_bpm_offset_x.getInt(),
                         cv::hud_statistics_bpm_offset_y.getInt());

        if(cv::draw_statistics_ar.getBool()) {
            ar = std::round(ar * 100.0f) / 100.0f;
            addStatistic(UString::fmt("AR: {:g}", ar), cv::hud_statistics_ar_offset_x.getInt(),
                         cv::hud_statistics_ar_offset_y.getInt());
        }

        if(cv::draw_statistics_cs.getBool()) {
            cs = std::round(cs * 100.0f) / 100.0f;
            addStatistic(UString::fmt("CS: {:g}", cs), cv::hud_statistics_cs_offset_x.getInt(),
                         cv::hud_statistics_cs_offset_y.getInt());
        }

        if(cv::draw_statistics_od.getBool()) {
            od = std::round(od * 100.0f) / 100.0f;
            addStatistic(UString::fmt("OD: {:g}", od), cv::hud_statistics_od_offset_x.getInt(),
                         cv::hud_statistics_od_offset_y.getInt());
        }

        if(cv::draw_statistics_hp.getBool()) {
            hp = std::round(hp * 100.0f) / 100.0f;
            addStatistic(UString::fmt("HP: {:g}", hp), cv::hud_statistics_hp_offset_x.getInt(),
                         cv::hud_statistics_hp_offset_y.getInt());
        }

        if(cv::draw_statistics_hitwindow300.getBool())
            addStatistic(UString::fmt("300: +-{:d}ms", (int)hitWindow300),
                         cv::hud_statistics_hitwindow300_offset_x.getInt(),
                         cv::hud_statistics_hitwindow300_offset_y.getInt());

        if(cv::draw_statistics_nps.getBool())
            addStatistic(UString::fmt("NPS: {:d}", nps), cv::hud_statistics_nps_offset_x.getInt(),
                         cv::hud_statistics_nps_offset_y.getInt());

        if(cv::draw_statistics_nd.getBool())
            addStatistic(UString::fmt("ND: {:d}", nd), cv::hud_statistics_nd_offset_x.getInt(),
                         cv::hud_statistics_nd_offset_y.getInt());

        if(cv::draw_statistics_ur.getBool())
            addStatistic(UString::fmt("UR: {:d}", ur), cv::hud_statistics_ur_offset_x.getInt(),
                         cv::hud_statistics_ur_offset_y.getInt());

        if(cv::draw_statistics_hitdelta.getBool())
            addStatistic(UString::fmt("-{:d}ms +{:d}ms", std::abs(hitdeltaMin), hitdeltaMax),
                         cv::hud_statistics_hitdelta_offset_x.getInt(), cv::hud_statistics_hitdelta_offset_y.getInt());
    }
    font->flushBatch();
    g->popTransform();
}

void HUD::drawTargetHeatmap(float hitcircleDiameter) {
    const vec2 center = vec2((int)(hitcircleDiameter / 2.0f + 5.0f), (int)(hitcircleDiameter / 2.0f + 5.0f));

    // constexpr const COLORPART brightnessSub = 0;
    static constexpr Color color300 = rgb(0, 255, 255);
    static constexpr Color color100 = rgb(0, 255, 0);
    static constexpr Color color50 = rgb(255, 165, 0);
    static constexpr Color colorMiss = rgb(255, 0, 0);

    Circle::drawCircle(osu->getSkin(), center, hitcircleDiameter, rgb(50, 50, 50));

    const int size = hitcircleDiameter * 0.075f;
    for(auto &target : this->targets) {
        const float delta = target.delta;

        const float overlap = 0.15f;
        Color color;
        if(delta < cv::mod_target_300_percent.getFloat() - overlap)
            color = color300;
        else if(delta < cv::mod_target_300_percent.getFloat() + overlap) {
            const float factor300 = (cv::mod_target_300_percent.getFloat() + overlap - delta) / (2.0f * overlap);
            const float factor100 = 1.0f - factor300;
            color = argb(1.0f, color300.Rf() * factor300 + color100.Rf() * factor100,
                         color300.Gf() * factor300 + color100.Gf() * factor100,
                         color300.Bf() * factor300 + color100.Bf() * factor100);
        } else if(delta < cv::mod_target_100_percent.getFloat() - overlap)
            color = color100;
        else if(delta < cv::mod_target_100_percent.getFloat() + overlap) {
            const float factor100 = (cv::mod_target_100_percent.getFloat() + overlap - delta) / (2.0f * overlap);
            const float factor50 = 1.0f - factor100;
            color = argb(1.0f, color100.Rf() * factor100 + color50.Rf() * factor50,
                         color100.Gf() * factor100 + color50.Gf() * factor50,
                         color100.Bf() * factor100 + color50.Bf() * factor50);
        } else if(delta < cv::mod_target_50_percent.getFloat())
            color = color50;
        else
            color = colorMiss;

        g->setColor(Color(color).setA(std::clamp<float>((target.time - engine->getTime()) / 3.5f, 0.0f, 1.0f)));

        const float theta = glm::radians(target.angle);
        const float cs = std::cos(theta);
        const float sn = std::sin(theta);

        vec2 up = vec2(-1, 0);
        vec2 offset{0.f};
        offset.x = up.x * cs - up.y * sn;
        offset.y = up.x * sn + up.y * cs;
        offset = vec::normalize(offset);
        offset *= (delta * (hitcircleDiameter / 2.0f));

        // g->fillRect(center.x-size/2 - offset.x, center.y-size/2 - offset.y, size, size);

        const float imageScale = osu->getImageScaleToFitResolution(osu->getSkin()->getCircleFull(), vec2(size, size));
        g->pushTransform();
        {
            g->scale(imageScale, imageScale);
            g->translate(center.x - offset.x, center.y - offset.y);
            g->drawImage(osu->getSkin()->getCircleFull());
        }
        g->popTransform();
    }
}

void HUD::drawScrubbingTimeline(u32 beatmapTime, u32 beatmapLengthPlayable, u32 beatmapStartTimePlayable,
                                f32 beatmapPercentFinishedPlayable, const std::vector<BREAK> &breaks) {
    static vec2 last_cursor_pos = mouse->getPos();
    static f64 last_cursor_movement = engine->getTime();
    vec2 new_cursor_pos = mouse->getPos();
    f64 new_cursor_movement = engine->getTime();
    if(last_cursor_pos.x != new_cursor_pos.x || last_cursor_pos.y != new_cursor_pos.y) {
        last_cursor_pos = new_cursor_pos;
        last_cursor_movement = new_cursor_movement;
    }

    // Auto-hide scrubbing timeline when watching a replay
    f64 galpha = 1.0f;
    if(osu->getMapInterface()->is_watching) {
        f64 time_since_last_move = new_cursor_movement - (last_cursor_movement + 1.0f);
        galpha = fmax(0.f, fmin(1.0f - time_since_last_move, 1.0f));
    }

    f32 dpiScale = Osu::getUIScale();
    vec2 cursorPos = mouse->getPos();
    cursorPos.y = osu->getVirtScreenHeight() * 0.8;

    Color grey = 0xffbbbbbb;
    Color greyTransparent = 0xbbbbbbbb;
    Color greyDark = 0xff777777;
    Color green = 0xff00ff00;

    McFont *timeFont = osu->getSubTitleFont();

    f32 breakHeight = 15 * dpiScale;
    f32 currentTimeTopTextOffset = 7 * dpiScale;
    f32 currentTimeLeftRightTextOffset = 5 * dpiScale;
    f32 startAndEndTimeTextOffset = 5 * dpiScale + breakHeight;
    u32 endTimeMS = beatmapStartTimePlayable + beatmapLengthPlayable;
    if(endTimeMS == 0) return;

    // draw strain graph
    if(cv::draw_scrubbing_timeline_strain_graph.getBool()) {
        const std::vector<f64> &aimStrains = osu->getMapInterface()->aimStrains;
        const std::vector<f64> &speedStrains = osu->getMapInterface()->speedStrains;

        u32 nb_strains = aimStrains.size();
        if(aimStrains.size() > 0 && aimStrains.size() == speedStrains.size()) {
            // get highest strain values for normalization
            f64 highestAimStrain = 0.0;
            f64 highestSpeedStrain = 0.0;
            f64 highestStrain = 0.0;
            i32 highestStrainIndex = -1;
            for(i32 i = 0; i < aimStrains.size(); i++) {
                f64 aimStrain = aimStrains[i];
                f64 speedStrain = speedStrains[i];
                f64 strain = aimStrain + speedStrain;

                if(strain > highestStrain) {
                    highestStrain = strain;
                    highestStrainIndex = i;
                }
                if(aimStrain > highestAimStrain) highestAimStrain = aimStrain;
                if(speedStrain > highestSpeedStrain) highestSpeedStrain = speedStrain;
            }

            // draw strain bar graph
            if(highestAimStrain > 0.0 && highestSpeedStrain > 0.0 && highestStrain > 0.0) {
                f32 offsetX = (f32)beatmapStartTimePlayable / (f32)endTimeMS * (f32)osu->getVirtScreenWidth();
                f32 drawable_area = (f32)osu->getVirtScreenWidth() - offsetX;
                f32 strainWidth = drawable_area / (f32)nb_strains;
                f32 strainHeightMultiplier = cv::hud_scrubbing_timeline_strains_height.getFloat() * dpiScale;

                f32 alpha = cv::hud_scrubbing_timeline_strains_alpha.getFloat() * galpha;
                Color aimStrainColor = argb(alpha, cv::hud_scrubbing_timeline_strains_aim_color_r.getInt() / 255.0f,
                                            cv::hud_scrubbing_timeline_strains_aim_color_g.getInt() / 255.0f,
                                            cv::hud_scrubbing_timeline_strains_aim_color_b.getInt() / 255.0f);
                Color speedStrainColor = argb(alpha, cv::hud_scrubbing_timeline_strains_speed_color_r.getInt() / 255.0f,
                                              cv::hud_scrubbing_timeline_strains_speed_color_g.getInt() / 255.0f,
                                              cv::hud_scrubbing_timeline_strains_speed_color_b.getInt() / 255.0f);

                g->setDepthBuffer(true);
                for(int i = 0; i < aimStrains.size(); i++) {
                    f64 aimStrain = (aimStrains[i]) / highestStrain;
                    f64 speedStrain = (speedStrains[i]) / highestStrain;
                    // f64 strain = (aimStrains[i] + speedStrains[i]) / highestStrain;

                    f64 aimStrainHeight = aimStrain * strainHeightMultiplier;
                    f64 speedStrainHeight = speedStrain * strainHeightMultiplier;
                    // f64 strainHeight = strain * strainHeightMultiplier;

                    g->setColor(aimStrainColor);
                    g->fillRect(i * strainWidth + offsetX, cursorPos.y - aimStrainHeight,
                                std::max(1.0f, std::round(strainWidth + 0.5f)), aimStrainHeight);

                    g->setColor(speedStrainColor);
                    g->fillRect(i * strainWidth + offsetX, cursorPos.y - aimStrainHeight - speedStrainHeight,
                                std::max(1.0f, std::round(strainWidth + 0.5f)), speedStrainHeight + 1);
                }
                g->setDepthBuffer(false);

                // highlight highest total strain value (+- section block)
                if(highestStrainIndex > -1) {
                    f64 aimStrain = (aimStrains[highestStrainIndex]) / highestStrain;
                    f64 speedStrain = (speedStrains[highestStrainIndex]) / highestStrain;
                    // f64 strain = (aimStrains[i] + speedStrains[i]) / highestStrain;

                    f64 aimStrainHeight = aimStrain * strainHeightMultiplier;
                    f64 speedStrainHeight = speedStrain * strainHeightMultiplier;
                    // f64 strainHeight = strain * strainHeightMultiplier;

                    vec2 topLeftCenter = vec2(highestStrainIndex * strainWidth + offsetX + strainWidth / 2.0f,
                                              cursorPos.y - aimStrainHeight - speedStrainHeight);

                    f32 margin = 5.0f * dpiScale;

                    g->setColor(Color(0xffffffff).setA(alpha));

                    g->drawRect(topLeftCenter.x - margin * strainWidth, topLeftCenter.y - margin * strainWidth,
                                strainWidth * 2 * margin,
                                aimStrainHeight + speedStrainHeight + 2 * margin * strainWidth);
                }
            }
        }
    }

    // breaks
    g->setColor(Color(greyTransparent).setA(galpha));

    for(auto i : breaks) {
        i32 width =
            std::max((i32)(osu->getVirtScreenWidth() * std::clamp<f32>(i.endPercent - i.startPercent, 0.0f, 1.0f)), 2);
        g->fillRect(osu->getVirtScreenWidth() * i.startPercent, cursorPos.y + 1, width, breakHeight);
    }

    // line
    g->setColor(Color(0xff000000).setA(galpha));

    g->drawLine(0, cursorPos.y + 1, osu->getVirtScreenWidth(), cursorPos.y + 1);
    g->setColor(Color(grey).setA(galpha));

    g->drawLine(0, cursorPos.y, osu->getVirtScreenWidth(), cursorPos.y);

    // current time triangle
    vec2 triangleTip = vec2(osu->getVirtScreenWidth() * beatmapPercentFinishedPlayable, cursorPos.y);
    g->pushTransform();
    {
        g->translate(triangleTip.x + 1, triangleTip.y - osu->getSkin()->getSeekTriangle()->getHeight() / 2.0f + 1);
        g->setColor(Color(0xff000000).setA(galpha));

        g->drawImage(osu->getSkin()->getSeekTriangle());
        g->translate(-1, -1);
        g->setColor(Color(green).setA(galpha));

        g->drawImage(osu->getSkin()->getSeekTriangle());
    }
    g->popTransform();

    // current time text
    UString currentTimeText = UString::format("%i:%02i", (beatmapTime / 1000) / 60, (beatmapTime / 1000) % 60);
    g->pushTransform();
    {
        g->translate(std::clamp<f32>(triangleTip.x - timeFont->getStringWidth(currentTimeText) / 2.0f,
                                     currentTimeLeftRightTextOffset,
                                     osu->getVirtScreenWidth() - timeFont->getStringWidth(currentTimeText) -
                                         currentTimeLeftRightTextOffset) +
                         1,
                     triangleTip.y - osu->getSkin()->getSeekTriangle()->getHeight() - currentTimeTopTextOffset + 1);
        g->setColor(Color(0xff000000).setA(galpha));

        g->drawString(timeFont, currentTimeText);
        g->translate(-1, -1);
        g->setColor(Color(green).setA(galpha));

        g->drawString(timeFont, currentTimeText);
    }
    g->popTransform();

    // start time text
    UString startTimeText =
        UString::format("(%i:%02i)", (beatmapStartTimePlayable / 1000) / 60, (beatmapStartTimePlayable / 1000) % 60);
    g->pushTransform();
    {
        g->translate((i32)(startAndEndTimeTextOffset + 1),
                     (i32)(triangleTip.y + startAndEndTimeTextOffset + timeFont->getHeight() + 1));
        g->setColor(Color(0xff000000).setA(galpha));

        g->drawString(timeFont, startTimeText);
        g->translate(-1, -1);
        g->setColor(Color(greyDark).setA(galpha));

        g->drawString(timeFont, startTimeText);
    }
    g->popTransform();

    // end time text
    UString endTimeText = UString::format("%i:%02i", (endTimeMS / 1000) / 60, (endTimeMS / 1000) % 60);
    g->pushTransform();
    {
        g->translate(
            (i32)(osu->getVirtScreenWidth() - timeFont->getStringWidth(endTimeText) - startAndEndTimeTextOffset + 1),
            (i32)(triangleTip.y + startAndEndTimeTextOffset + timeFont->getHeight() + 1));
        g->setColor(Color(0xff000000).setA(galpha));

        g->drawString(timeFont, endTimeText);
        g->translate(-1, -1);
        g->setColor(Color(greyDark).setA(galpha));

        g->drawString(timeFont, endTimeText);
    }
    g->popTransform();

    // quicksave time triangle & text
    if(osu->getQuickSaveTime() != 0.0f) {
        f32 quickSavePercent = std::clamp<f32>(osu->getQuickSaveTime() / (f32)endTimeMS, 0.f, 1.f);
        triangleTip = vec2(osu->getVirtScreenWidth() * quickSavePercent, cursorPos.y);
        g->pushTransform();
        {
            g->rotate(180);
            g->translate(triangleTip.x + 1, triangleTip.y + osu->getSkin()->getSeekTriangle()->getHeight() / 2.0f + 1);
            g->setColor(Color(0xff000000).setA(galpha));

            g->drawImage(osu->getSkin()->getSeekTriangle());
            g->translate(-1, -1);
            g->setColor(Color(grey).setA(galpha));

            g->drawImage(osu->getSkin()->getSeekTriangle());
        }
        g->popTransform();

        // end time text
        u32 quickSaveTimeMS = osu->getQuickSaveTime();
        UString endTimeText = UString::format("%i:%02i", (quickSaveTimeMS / 1000) / 60, (quickSaveTimeMS / 1000) % 60);
        g->pushTransform();
        {
            g->translate((i32)(std::clamp<f32>(triangleTip.x - timeFont->getStringWidth(currentTimeText) / 2.0f,
                                               currentTimeLeftRightTextOffset,
                                               osu->getVirtScreenWidth() - timeFont->getStringWidth(currentTimeText) -
                                                   currentTimeLeftRightTextOffset) +
                               1),
                         (i32)(triangleTip.y + startAndEndTimeTextOffset + timeFont->getHeight() * 2.2f + 1 +
                               currentTimeTopTextOffset *
                                   std::max(1.0f, this->getCursorScaleFactor() * cv::cursor_scale.getFloat()) *
                                   cv::hud_scrubbing_timeline_hover_tooltip_offset_multiplier.getFloat()));
            g->setColor(Color(0xff000000).setA(galpha));

            g->drawString(timeFont, endTimeText);
            g->translate(-1, -1);
            g->setColor(Color(grey).setA(galpha));

            g->drawString(timeFont, endTimeText);
        }
        g->popTransform();
    }

    // current time hover text
    u32 hoverTimeMS = std::clamp<f32>((cursorPos.x / (f32)osu->getVirtScreenWidth()), 0.0f, 1.0f) * endTimeMS;
    UString hoverTimeText = UString::format("%i:%02i", (hoverTimeMS / 1000) / 60, (hoverTimeMS / 1000) % 60);
    triangleTip = vec2(cursorPos.x, cursorPos.y);
    g->pushTransform();
    {
        g->translate(
            (i32)std::clamp<f32>(triangleTip.x - timeFont->getStringWidth(currentTimeText) / 2.0f,
                                 currentTimeLeftRightTextOffset,
                                 osu->getVirtScreenWidth() - timeFont->getStringWidth(currentTimeText) -
                                     currentTimeLeftRightTextOffset) +
                1,
            (i32)(triangleTip.y - osu->getSkin()->getSeekTriangle()->getHeight() - timeFont->getHeight() * 1.2f -
                  currentTimeTopTextOffset *
                      std::max(1.0f, this->getCursorScaleFactor() * cv::cursor_scale.getFloat()) *
                      cv::hud_scrubbing_timeline_hover_tooltip_offset_multiplier.getFloat() * 2.0f -
                  1));
        g->setColor(Color(0xff000000).setA(galpha));

        g->drawString(timeFont, hoverTimeText);
        g->translate(-1, -1);
        g->setColor(Color(0xff666666).setA(galpha));

        g->drawString(timeFont, hoverTimeText);
    }
    g->popTransform();
}

void HUD::drawInputOverlay(int numK1, int numK2, int numM1, int numM2) {
    SkinImage *inputoverlayBackground = osu->getSkin()->getInputoverlayBackground();
    SkinImage *inputoverlayKey = osu->getSkin()->getInputoverlayKey();

    const float scale = cv::hud_scale.getFloat() * cv::hud_inputoverlay_scale.getFloat();  // global scaler
    const float oScale = inputoverlayBackground->getResolutionScale() *
                         1.6f;  // for converting harcoded osu offset pixels to screen pixels
    const float offsetScale = Osu::getImageScale(vec2(1.0f, 1.0f),
                                                 1.0f);  // for scaling the x/y offset convars relative to screen size

    const float xStartOffset = cv::hud_inputoverlay_offset_x.getFloat() * offsetScale;
    const float yStartOffset = cv::hud_inputoverlay_offset_y.getFloat() * offsetScale;

    const float xStart = osu->getVirtScreenWidth() - xStartOffset;
    const float yStart = osu->getVirtScreenHeight() / 2 - (40.0f * oScale) * scale + yStartOffset;

    // background
    {
        const float xScale = 1.05f + 0.001f;
        const float rot = 90.0f;

        const float xOffset = (inputoverlayBackground->getSize().y / 2);
        const float yOffset = (inputoverlayBackground->getSize().x / 2) * xScale;

        g->setColor(0xffffffff);
        g->pushTransform();
        {
            g->scale(xScale, 1.0f);
            g->rotate(rot);
            inputoverlayBackground->draw(vec2(xStart - xOffset * scale + 1, yStart + yOffset * scale), scale);
        }
        g->popTransform();
    }

    // keys
    {
        const float textFontHeightPercent = 0.3f;
        const Color colorIdle = argb(255, 255, 255, 255);
        const Color colorKeyboard = argb(255, 255, 222, 0);
        const Color colorMouse = argb(255, 248, 0, 158);

        McFont *textFont = osu->getSongBrowserFont();
        McFont *textFontBold = osu->getSongBrowserFontBold();

        for(int i = 0; i < 4; i++) {
            textFont = osu->getSongBrowserFont();  // reset

            UString text;
            Color color = colorIdle;
            float animScale = 1.0f;
            float animColor = 0.0f;
            switch(i) {
                case 0:
                    text = numK1 > 0 ? UString::format("%i", numK1) : UString("K1");
                    color = colorKeyboard;
                    animScale = this->fInputoverlayK1AnimScale;
                    animColor = this->fInputoverlayK1AnimColor;
                    if(numK1 > 0) textFont = textFontBold;
                    break;
                case 1:
                    text = numK2 > 0 ? UString::format("%i", numK2) : UString("K2");
                    color = colorKeyboard;
                    animScale = this->fInputoverlayK2AnimScale;
                    animColor = this->fInputoverlayK2AnimColor;
                    if(numK2 > 0) textFont = textFontBold;
                    break;
                case 2:
                    text = numM1 > 0 ? UString::format("%i", numM1) : UString("M1");
                    color = colorMouse;
                    animScale = this->fInputoverlayM1AnimScale;
                    animColor = this->fInputoverlayM1AnimColor;
                    if(numM1 > 0) textFont = textFontBold;
                    break;
                case 3:
                    text = numM2 > 0 ? UString::format("%i", numM2) : UString("M2");
                    color = colorMouse;
                    animScale = this->fInputoverlayM2AnimScale;
                    animColor = this->fInputoverlayM2AnimColor;
                    if(numM2 > 0) textFont = textFontBold;
                    break;
            }

            // key
            const vec2 pos =
                vec2(xStart - (15.0f * oScale) * scale + 1, yStart + (19.0f * oScale + i * 29.5f * oScale) * scale);
            g->setColor(argb(1.0f, (1.0f - animColor) * colorIdle.Rf() + animColor * color.Rf(),
                             (1.0f - animColor) * colorIdle.Gf() + animColor * color.Gf(),
                             (1.0f - animColor) * colorIdle.Bf() + animColor * color.Bf()));
            inputoverlayKey->draw(pos, scale * animScale);

            // text
            const float keyFontScale =
                (inputoverlayKey->getSizeBase().y * textFontHeightPercent) / textFont->getHeight();
            const float stringWidth = textFont->getStringWidth(text) * keyFontScale;
            const float stringHeight = textFont->getHeight() * keyFontScale;
            g->setColor(osu->getSkin()->getInputOverlayText());
            g->pushTransform();
            {
                g->scale(keyFontScale * scale * animScale, keyFontScale * scale * animScale);
                g->translate(pos.x - (stringWidth / 2.0f) * scale * animScale,
                             pos.y + (stringHeight / 2.0f) * scale * animScale);
                g->drawString(textFont, text);
            }
            g->popTransform();
        }
    }
}

float HUD::getCursorScaleFactor() {
    // FUCK OSU hardcoded piece of shit code
    const float spriteRes = 768.0f;

    float mapScale = 1.0f;
    if(cv::automatic_cursor_size.getBool() && osu->isInPlayMode())
        mapScale = 1.0f - 0.7f * (float)(osu->getMapInterface()->getCS() - 4.0f) / 5.0f;

    return ((float)osu->getVirtScreenHeight() / spriteRes) * mapScale;
}

float HUD::getCursorTrailScaleFactor() {
    return this->getCursorScaleFactor() * (osu->getSkin()->isCursorTrail2x() ? 0.5f : 1.0f);
}

float HUD::getScoreScale() {
    return osu->getImageScale(osu->getSkin()->getScore0(), 13 * 1.5f) * cv::hud_scale.getFloat() *
           cv::hud_score_scale.getFloat();
}

void HUD::animateCombo() {
    this->fComboAnim1 = 0.0f;
    this->fComboAnim2 = 1.0f;

    anim->moveLinear(&this->fComboAnim1, 2.0f, cv::combo_anim1_duration.getFloat(), true);
    anim->moveQuadOut(&this->fComboAnim2, 0.0f, cv::combo_anim2_duration.getFloat(), 0.0f, true);
}

void HUD::addHitError(long delta, bool miss, bool misaim) {
    // add entry
    {
        HITERROR h;

        h.delta = delta;
        h.time = engine->getTime() + (miss || misaim ? cv::hud_hiterrorbar_entry_miss_fade_time.getFloat()
                                                     : cv::hud_hiterrorbar_entry_hit_fade_time.getFloat());
        h.miss = miss;
        h.misaim = misaim;

        this->hiterrors.push_back(h);
    }

    // remove old
    for(int i = 0; i < this->hiterrors.size(); i++) {
        if(engine->getTime() > this->hiterrors[i].time) {
            this->hiterrors.erase(this->hiterrors.begin() + i);
            i--;
        }
    }

    if(this->hiterrors.size() > cv::hud_hiterrorbar_max_entries.getInt())
        this->hiterrors.erase(this->hiterrors.begin());
}

void HUD::addTarget(float delta, float angle) {
    TARGET t;
    t.time = engine->getTime() + 3.5f;
    t.delta = delta;
    t.angle = angle;

    this->targets.push_back(t);
}

void HUD::animateInputoverlay(int key, bool down) {
    if(!cv::draw_inputoverlay.getBool() || (!cv::draw_hud.getBool() && cv::hud_shift_tab_toggles_everything.getBool()))
        return;

    float *animScale = &this->fInputoverlayK1AnimScale;
    float *animColor = &this->fInputoverlayK1AnimColor;

    switch(key) {
        case 1:
            animScale = &this->fInputoverlayK1AnimScale;
            animColor = &this->fInputoverlayK1AnimColor;
            break;
        case 2:
            animScale = &this->fInputoverlayK2AnimScale;
            animColor = &this->fInputoverlayK2AnimColor;
            break;
        case 3:
            animScale = &this->fInputoverlayM1AnimScale;
            animColor = &this->fInputoverlayM1AnimColor;
            break;
        case 4:
            animScale = &this->fInputoverlayM2AnimScale;
            animColor = &this->fInputoverlayM2AnimColor;
            break;
    }

    if(down) {
        // scale
        *animScale = 1.0f;
        anim->moveQuadOut(animScale, cv::hud_inputoverlay_anim_scale_multiplier.getFloat(),
                          cv::hud_inputoverlay_anim_scale_duration.getFloat(), true);

        // color
        *animColor = 1.0f;
        anim->deleteExistingAnimation(animColor);
    } else {
        // scale
        // NOTE: osu is running the keyup anim in parallel, but only allowing it to override once the keydown anim has
        // finished, and with some weird speedup?
        const float remainingDuration = anim->getRemainingDuration(animScale);
        anim->moveQuadOut(animScale, 1.0f,
                          cv::hud_inputoverlay_anim_scale_duration.getFloat() -
                              std::min(remainingDuration * 1.4f, cv::hud_inputoverlay_anim_scale_duration.getFloat()),
                          remainingDuration);

        // color
        anim->moveLinear(animColor, 0.0f, cv::hud_inputoverlay_anim_color_duration.getFloat(), true);
    }
}

void HUD::addCursorRipple(vec2 pos) {
    if(!cv::draw_cursor_ripples.getBool()) return;

    CURSORRIPPLE ripple;
    ripple.pos = pos;
    ripple.time = engine->getTime() + cv::cursor_ripple_duration.getFloat();

    this->cursorRipples.push_back(ripple);
}

void HUD::animateCursorExpand() {
    this->fCursorExpandAnim = 1.0f;
    anim->moveQuadOut(&this->fCursorExpandAnim, cv::cursor_expand_scale_multiplier.getFloat(),
                      cv::cursor_expand_duration.getFloat(), 0.0f, true);
}

void HUD::animateCursorShrink() {
    anim->moveQuadOut(&this->fCursorExpandAnim, 1.0f, cv::cursor_expand_duration.getFloat(), 0.0f, true);
}

void HUD::animateKiBulge() {
    this->fKiScaleAnim = 1.2f;
    anim->moveLinear(&this->fKiScaleAnim, 0.8f, 0.150f, true);
}

void HUD::animateKiExplode() {
    // TODO: scale + fadeout of extra ki image additive, duration = 0.120, quad out:
    // if additive: fade from 0.5 alpha to 0, scale from 1.0 to 2.0
    // if not additive: fade from 1.0 alpha to 0, scale from 1.0 to 1.6
}

void HUD::addCursorTrailPosition(std::vector<CURSORTRAIL> &trail, vec2 pos) {
    if(pos.x < -osu->getVirtScreenWidth() || pos.x > osu->getVirtScreenWidth() * 2 ||
       pos.y < -osu->getVirtScreenHeight() || pos.y > osu->getVirtScreenHeight() * 2)
        return;  // fuck oob trails

    Image *trailImage = osu->getSkin()->getCursorTrail();

    const bool smoothCursorTrail = osu->getSkin()->useSmoothCursorTrail() || cv::cursor_trail_smooth_force.getBool();

    const float scaleAnim =
        (osu->getSkin()->getCursorExpand() && cv::cursor_trail_expand.getBool() ? this->fCursorExpandAnim : 1.0f) *
        cv::cursor_trail_scale.getFloat();
    const float trailWidth =
        trailImage->getWidth() * this->getCursorTrailScaleFactor() * scaleAnim * cv::cursor_scale.getFloat();

    CURSORTRAIL ct;
    ct.pos = pos;
    ct.time = engine->getTime() +
              (smoothCursorTrail ? cv::cursor_trail_smooth_length.getFloat() : cv::cursor_trail_length.getFloat());
    ct.alpha = 1.0f;
    ct.scale = scaleAnim;

    if(smoothCursorTrail) {
        // interpolate mid points between the last point and the current point
        if(trail.size() > 0) {
            const vec2 prevPos = trail[trail.size() - 1].pos;
            const float prevTime = trail[trail.size() - 1].time;
            const float prevScale = trail[trail.size() - 1].scale;

            vec2 delta = pos - prevPos;
            const int numMidPoints = (int)(vec::length(delta) / (trailWidth / cv::cursor_trail_smooth_div.getFloat()));
            if(numMidPoints > 0) {
                const vec2 step = vec::normalize(delta) * (trailWidth / cv::cursor_trail_smooth_div.getFloat());
                const float timeStep = (ct.time - prevTime) / (float)(numMidPoints);
                const float scaleStep = (ct.scale - prevScale) / (float)(numMidPoints);
                for(int i = std::clamp<int>(numMidPoints - cv::cursor_trail_max_size.getInt() / 2, 0,
                                            cv::cursor_trail_max_size.getInt());
                    i < numMidPoints; i++)  // limit to half the maximum new mid points per frame
                {
                    CURSORTRAIL mid;
                    mid.pos = prevPos + step * (i + 1);
                    mid.time = prevTime + timeStep * (i + 1);
                    mid.alpha = 1.0f;
                    mid.scale = prevScale + scaleStep * (i + 1);
                    trail.push_back(mid);
                }
            }
        } else {
            trail.push_back(ct);
        }
    } else if((trail.size() > 0 && engine->getTime() > trail[trail.size() - 1].time -
                                                           cv::cursor_trail_length.getFloat() +
                                                           cv::cursor_trail_spacing.getFloat() / 1000.f) ||
              trail.size() == 0) {
        if(trail.size() > 0 && trail[trail.size() - 1].pos == pos && !cv::always_render_cursor_trail.getBool()) {
            trail[trail.size() - 1].time = ct.time;
            trail[trail.size() - 1].alpha = 1.0f;
            trail[trail.size() - 1].scale = ct.scale;
        } else {
            trail.push_back(ct);
        }
    }

    // early cleanup
    while(trail.size() > cv::cursor_trail_max_size.getInt()) {
        trail.erase(trail.begin());
    }
}

void HUD::resetHitErrorBar() { this->hiterrors.clear(); }

McRect HUD::getSkipClickRect() {
    const float skipScale = cv::hud_scale.getFloat();
    return McRect(osu->getVirtScreenWidth() - osu->getSkin()->getPlaySkip()->getSize().x * skipScale,
                  osu->getVirtScreenHeight() - osu->getSkin()->getPlaySkip()->getSize().y * skipScale,
                  osu->getSkin()->getPlaySkip()->getSize().x * skipScale,
                  osu->getSkin()->getPlaySkip()->getSize().y * skipScale);
}
