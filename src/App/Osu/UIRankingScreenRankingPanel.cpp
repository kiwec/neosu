// Copyright (c) 2016, PG, All rights reserved.
#include "UIRankingScreenRankingPanel.h"

#include "ConVar.h"
#include "Engine.h"
#include "HUD.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SkinImage.h"
#include "score.h"

UIRankingScreenRankingPanel::UIRankingScreenRankingPanel() : CBaseUIImage("", 0, 0, 0, 0, "") {
    this->setImage(osu->getSkin()->getRankingPanel());
    this->setDrawFrame(true);

    this->iScore = 0;
    this->iNum300s = 0;
    this->iNum300gs = 0;
    this->iNum100s = 0;
    this->iNum100ks = 0;
    this->iNum50s = 0;
    this->iNumMisses = 0;
    this->iCombo = 0;
    this->fAccuracy = 0.0f;
    this->bPerfect = false;
}

void UIRankingScreenRankingPanel::draw() {
    CBaseUIImage::draw();
    if(!this->bVisible) return;

    const float uiScale = /*cv::ui_scale.getFloat()*/ 1.0f;  // NOTE: commented for now, doesn't really work due to
                                                            // legacy layout expectations

    const float globalScoreScale = (osu->getSkin()->getVersion() > 1.0f ? 1.3f : 1.05f) * uiScale;

    const int globalYOffsetRaw = -1;
    const int globalYOffset = osu->getUIScale(globalYOffsetRaw);

    // draw score
    g->setColor(0xffffffff);
    float scale = osu->getImageScale(osu->getSkin()->getScore0(), 20.0f) * globalScoreScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(this->vPos.x + osu->getUIScale(111.0f) * uiScale,
                     this->vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale +
                         (osu->getUIScale(11.0f) + globalYOffset) * uiScale);
        osu->getHUD()->drawScoreNumber(this->iScore, scale);
    }
    g->popTransform();

    // draw hit images
    const Vector2 hitImageStartPos = Vector2(40, 100 + globalYOffsetRaw);
    const Vector2 hitGridOffsetX = Vector2(200, 0);
    const Vector2 hitGridOffsetY = Vector2(0, 60);

    this->drawHitImage(osu->getSkin()->getHit300(), scale, hitImageStartPos);
    this->drawHitImage(osu->getSkin()->getHit100(), scale, hitImageStartPos + hitGridOffsetY);
    this->drawHitImage(osu->getSkin()->getHit50(), scale, hitImageStartPos + hitGridOffsetY * 2);
    this->drawHitImage(osu->getSkin()->getHit300g(), scale, hitImageStartPos + hitGridOffsetX);
    this->drawHitImage(osu->getSkin()->getHit100k(), scale, hitImageStartPos + hitGridOffsetX + hitGridOffsetY);
    this->drawHitImage(osu->getSkin()->getHit0(), scale, hitImageStartPos + hitGridOffsetX + hitGridOffsetY * 2);

    // draw numHits
    const Vector2 numHitStartPos = hitImageStartPos + Vector2(40, osu->getSkin()->getVersion() > 1.0f ? -16 : -25);
    scale = osu->getImageScale(osu->getSkin()->getScore0(), 17.0f) * globalScoreScale;

    this->drawNumHits(this->iNum300s, scale, numHitStartPos);
    this->drawNumHits(this->iNum100s, scale, numHitStartPos + hitGridOffsetY);
    this->drawNumHits(this->iNum50s, scale, numHitStartPos + hitGridOffsetY * 2);

    this->drawNumHits(this->iNum300gs, scale, numHitStartPos + hitGridOffsetX);
    this->drawNumHits(this->iNum100ks, scale, numHitStartPos + hitGridOffsetX + hitGridOffsetY);
    this->drawNumHits(this->iNumMisses, scale, numHitStartPos + hitGridOffsetX + hitGridOffsetY * 2);

    const int row4 = 260;
    const int row4ImageOffset = (osu->getSkin()->getVersion() > 1.0f ? 20 : 8) - 20;

    // draw combo
    scale = osu->getImageScale(osu->getSkin()->getScore0(), 17.0f) * globalScoreScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(this->vPos.x + osu->getUIScale(15.0f) * uiScale,
                     this->vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale +
                         (osu->getUIScale(row4 + 10) + globalYOffset) * uiScale);
        osu->getHUD()->drawComboSimple(this->iCombo, scale);
    }
    g->popTransform();

    // draw maxcombo label
    Vector2 hardcodedOsuRankingMaxComboImageSize =
        Vector2(162, 50) * (osu->getSkin()->isRankingMaxCombo2x() ? 2.0f : 1.0f);
    scale = osu->getImageScale(hardcodedOsuRankingMaxComboImageSize, 32.0f) * uiScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(this->vPos.x + osu->getSkin()->getRankingMaxCombo()->getWidth() * scale * 0.5f +
                         osu->getUIScale(4.0f) * uiScale,
                     this->vPos.y + (osu->getUIScale(row4 - 5 - row4ImageOffset) + globalYOffset) * uiScale);
        g->drawImage(osu->getSkin()->getRankingMaxCombo());
    }
    g->popTransform();

    // draw accuracy
    scale = osu->getImageScale(osu->getSkin()->getScore0(), 17.0f) * globalScoreScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(this->vPos.x + osu->getUIScale(195.0f) * uiScale,
                     this->vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale +
                         (osu->getUIScale(row4 + 10) + globalYOffset) * uiScale);
        osu->getHUD()->drawAccuracySimple(this->fAccuracy * 100.0f, scale);
    }
    g->popTransform();

    // draw accuracy label
    Vector2 hardcodedOsuRankingAccuracyImageSize =
        Vector2(192, 58) * (osu->getSkin()->isRankingAccuracy2x() ? 2.0f : 1.0f);
    scale = osu->getImageScale(hardcodedOsuRankingAccuracyImageSize, 36.0f) * uiScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(this->vPos.x + osu->getSkin()->getRankingAccuracy()->getWidth() * scale * 0.5f +
                         osu->getUIScale(183.0f) * uiScale,
                     this->vPos.y + (osu->getUIScale(row4 - 3 - row4ImageOffset) + globalYOffset) * uiScale);
        g->drawImage(osu->getSkin()->getRankingAccuracy());
    }
    g->popTransform();

    // draw perfect
    if(this->bPerfect) {
        scale = osu->getImageScale(osu->getSkin()->getRankingPerfect()->getSizeBaseRaw(), 94.0f) * uiScale;
        osu->getSkin()->getRankingPerfect()->drawRaw(
            this->vPos +
                Vector2(osu->getUIScale(osu->getSkin()->getVersion() > 1.0f ? 260 : 200),
                        osu->getUIScale(430.0f) + globalYOffset) *
                    Vector2(1.0f, 0.97f) * uiScale -
                Vector2(0, osu->getSkin()->getRankingPerfect()->getSizeBaseRaw().y) * scale * 0.5f,
            scale);
    }
}

void UIRankingScreenRankingPanel::drawHitImage(SkinImage *img, float  /*scale*/, Vector2 pos) {
    const float uiScale = /*cv::ui_scale.getFloat()*/ 1.0f;  // NOTE: commented for now, doesn't really work due to
                                                            // legacy layout expectations

    /// img->setAnimationFrameForce(0);
    img->draw(
        Vector2(this->vPos.x + osu->getUIScale(pos.x) * uiScale, this->vPos.y + osu->getUIScale(pos.y) * uiScale),
        uiScale);
}

void UIRankingScreenRankingPanel::drawNumHits(int numHits, float scale, Vector2 pos) {
    const float uiScale = /*cv::ui_scale.getFloat()*/ 1.0f;  // NOTE: commented for now, doesn't really work due to
                                                            // legacy layout expectations

    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(
            this->vPos.x + osu->getUIScale(pos.x) * uiScale,
            this->vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale + osu->getUIScale(pos.y) * uiScale);
        osu->getHUD()->drawComboSimple(numHits, scale);
    }
    g->popTransform();
}

void UIRankingScreenRankingPanel::setScore(LiveScore *score) {
    this->iScore = score->getScore();
    this->iNum300s = score->getNum300s();
    this->iNum300gs = score->getNum300gs();
    this->iNum100s = score->getNum100s();
    this->iNum100ks = score->getNum100ks();
    this->iNum50s = score->getNum50s();
    this->iNumMisses = score->getNumMisses();
    this->iCombo = score->getComboMax();
    this->fAccuracy = score->getAccuracy();
    this->bPerfect = (score->getComboFull() > 0 && this->iCombo >= score->getComboFull());
}

void UIRankingScreenRankingPanel::setScore(const FinishedScore& score) {
    this->iScore = score.score;
    this->iNum300s = score.num300s;
    this->iNum300gs = score.numGekis;
    this->iNum100s = score.num100s;
    this->iNum100ks = score.numKatus;
    this->iNum50s = score.num50s;
    this->iNumMisses = score.numMisses;
    this->iCombo = score.comboMax;
    this->fAccuracy = LiveScore::calculateAccuracy(score.num300s, score.num100s, score.num50s, score.numMisses);
    this->bPerfect = score.perfect;

    // round acc up from two decimal places
    if(this->fAccuracy > 0.0f) this->fAccuracy = std::round(this->fAccuracy * 10000.0f) / 10000.0f;
}
