// Copyright (c) 2015, PG, All rights reserved.
#include "score.h"

#include "Bancho.h"
#include "BanchoProtocol.h"
#include "BeatmapInterface.h"
#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "Engine.h"
#include "GameRules.h"
#include "HUD.h"
#include "HitObjects.h"
#include "LegacyReplay.h"
#include "Osu.h"
#include "RoomScreen.h"
#include "SongBrowser/LeaderboardPPCalcThread.h"
#include "Logging.h"

LiveScore::LiveScore(bool simulating) {
    this->simulating = simulating;
    this->reset();
}

void LiveScore::reset() {
    this->hitresults = std::vector<HIT>();
    this->hitdeltas = std::vector<int>();

    this->grade = FinishedScore::Grade::N;
    if(!this->simulating) {
        this->mods = Replay::Mods::from_cvars();
    }

    this->fStarsTomTotal = 0.0f;
    this->fStarsTomAim = 0.0f;
    this->fStarsTomSpeed = 0.0f;
    this->fPPv2 = 0.0f;
    this->iIndex = -1;

    this->iScoreV1 = 0;
    this->iScoreV2 = 0;
    this->iScoreV2ComboPortion = 0;
    this->iBonusPoints = 0;
    this->iCombo = 0;
    this->iComboMax = 0;
    this->iComboFull = 0;
    this->iComboEndBitmask = 0;
    this->fAccuracy = 1.0f;
    this->fHitErrorAvgMin = 0.0f;
    this->fHitErrorAvgMax = 0.0f;
    this->fHitErrorAvgCustomMin = 0.0f;
    this->fHitErrorAvgCustomMax = 0.0f;
    this->fUnstableRate = 0.0f;

    this->iNumMisses = 0;
    this->iNumSliderBreaks = 0;
    this->iNum50s = 0;
    this->iNum100s = 0;
    this->iNum100ks = 0;
    this->iNum300s = 0;
    this->iNum300gs = 0;

    this->bDead = false;
    this->bDied = false;

    this->iNumK1 = 0;
    this->iNumK2 = 0;
    this->iNumM1 = 0;
    this->iNumM2 = 0;

    this->iNumEZRetries = 2;
    this->bIsUnranked = false;

    this->onScoreChange();
}

float LiveScore::getScoreMultiplier() {
    bool ez = this->mods.has(ModFlags::Easy);
    bool nf = this->mods.has(ModFlags::NoFail);
    bool sv2 = this->mods.has(ModFlags::ScoreV2);
    bool hr = this->mods.has(ModFlags::HardRock);
    bool fl = this->mods.has(ModFlags::Flashlight);
    bool hd = this->mods.has(ModFlags::Hidden);
    bool so = this->mods.has(ModFlags::SpunOut);
    bool rx = this->mods.has(ModFlags::Relax);
    bool ap = this->mods.has(ModFlags::Autopilot);

    float multiplier = 1.0f;

    // Dumb formula, but the values for HT/DT were dumb to begin with
    if(this->mods.speed > 1.f) {
        multiplier *= (0.24 * this->mods.speed) + 0.76;
    } else if(this->mods.speed < 1.f) {
        multiplier *= 0.008 * std::exp(4.81588 * this->mods.speed);
    }

    if(ez || (nf && !sv2)) multiplier *= 0.5f;
    if(hr) multiplier *= sv2 ? 1.1f : 1.06f;
    if(fl) multiplier *= 1.12f;
    if(hd) multiplier *= 1.06f;
    if(so) multiplier *= 0.90f;
    if(rx || ap) multiplier *= 0.f;

    return multiplier;
}

void LiveScore::addHitResult(AbstractBeatmapInterface *beatmap, HitObject * /*hitObject*/, HIT hit, i32 delta,
                             bool ignoreOnHitErrorBar, bool hitErrorBarOnly, bool ignoreCombo, bool ignoreScore) {
    // current combo, excluding the current hitobject which caused the addHitResult() call
    const int scoreComboMultiplier = std::max(this->iCombo - 1, 0);

    if(hit == LiveScore::HIT::HIT_MISS) {
        this->iCombo = 0;

        if(!this->simulating && !ignoreOnHitErrorBar && cv::hiterrorbar_misses.getBool() &&
           delta <= (i32)beatmap->getHitWindow50()) {
            osu->getHUD()->addHitError(delta, true);
        }
    } else {
        if(!ignoreOnHitErrorBar) {
            this->hitdeltas.push_back((int)delta);
            if(!this->simulating) osu->getHUD()->addHitError(delta);
        }

        if(!ignoreCombo) {
            this->iCombo++;
            if(!this->simulating) osu->getHUD()->animateCombo();
        }
    }

    // keep track of the maximum possible combo at this time, for live pp calculation
    if(!ignoreCombo) this->iComboFull++;

    // store the result, get hit value
    u64 hitValue = 0;
    if(!hitErrorBarOnly) {
        this->hitresults.push_back(hit);

        switch(hit) {
            case LiveScore::HIT::HIT_MISS:
                this->iNumMisses++;
                this->iComboEndBitmask |= 2;
                break;

            case LiveScore::HIT::HIT_50:
                this->iNum50s++;
                hitValue = 50;
                this->iComboEndBitmask |= 2;
                break;

            case LiveScore::HIT::HIT_100:
                this->iNum100s++;
                hitValue = 100;
                this->iComboEndBitmask |= 1;
                break;

            case LiveScore::HIT::HIT_300:
                this->iNum300s++;
                hitValue = 300;
                break;

            default:
                debugLog("Unexpected hitresult {:d}", static_cast<unsigned int>(hit));
                break;
        }
    }

    // add hitValue to score, recalculate score v1
    if(!ignoreScore) {
        const int difficultyMultiplier = beatmap->getScoreV1DifficultyMultiplier();
        this->iScoreV1 += hitValue;
        this->iScoreV1 += ((hitValue * (u32)((f64)scoreComboMultiplier * (f64)difficultyMultiplier *
                                             (f64)this->getScoreMultiplier())) /
                           (u32)25);
    }

    const float totalHitPoints = this->iNum50s * (1.0f / 6.0f) + this->iNum100s * (2.0f / 6.0f) + this->iNum300s;
    const float totalNumHits = this->iNumMisses + this->iNum50s + this->iNum100s + this->iNum300s;

    const float percent300s = this->iNum300s / totalNumHits;
    const float percent50s = this->iNum50s / totalNumHits;

    // recalculate accuracy
    if(((totalHitPoints == 0.0f || totalNumHits == 0.0f) && this->hitresults.size() < 1) || totalNumHits <= 0.0f)
        this->fAccuracy = 1.0f;
    else
        this->fAccuracy = totalHitPoints / totalNumHits;

    // recalculate score v2
    this->iScoreV2ComboPortion += (u64)((double)hitValue * (1.0 + (double)scoreComboMultiplier / 10.0));
    if(this->mods.has(ModFlags::ScoreV2)) {
        const double maximumAccurateHits = beatmap->nb_hitobjects;

        if(totalNumHits > 0) {
            this->iScoreV2 =
                (u64)(((f64)this->iScoreV2ComboPortion / (f64)beatmap->iScoreV2ComboPortionMaximum * 700000.0 +
                       pow((f64)this->fAccuracy, 10.0) * ((f64)totalNumHits / maximumAccurateHits) * 300000.0 +
                       (f64)this->iBonusPoints) *
                      (f64)this->getScoreMultiplier());
        }
    }

    // recalculate grade
    this->grade = FinishedScore::Grade::D;

    bool hd = this->mods.has(ModFlags::Hidden);
    bool fl = this->mods.has(ModFlags::Flashlight);

    if(percent300s > 0.6f) this->grade = FinishedScore::Grade::C;
    if((percent300s > 0.7f && this->iNumMisses == 0) || (percent300s > 0.8f)) this->grade = FinishedScore::Grade::B;
    if((percent300s > 0.8f && this->iNumMisses == 0) || (percent300s > 0.9f)) this->grade = FinishedScore::Grade::A;
    if(percent300s > 0.9f && percent50s <= 0.01f && this->iNumMisses == 0) {
        this->grade = (hd || fl) ? FinishedScore::Grade::SH : FinishedScore::Grade::S;
    }
    if(this->iNumMisses == 0 && this->iNum50s == 0 && this->iNum100s == 0) {
        this->grade = (hd || fl) ? FinishedScore::Grade::XH : FinishedScore::Grade::X;
    }

    // recalculate unstable rate
    float averageDelta = 0.0f;
    this->fUnstableRate = 0.0f;
    this->fHitErrorAvgMin = 0.0f;
    this->fHitErrorAvgMax = 0.0f;
    this->fHitErrorAvgCustomMin = 0.0f;
    this->fHitErrorAvgCustomMax = 0.0f;
    if(this->hitdeltas.size() > 0) {
        int numPositives = 0;
        int numNegatives = 0;
        int numCustomPositives = 0;
        int numCustomNegatives = 0;

        int customStartIndex = -1;
        if(cv::hud_statistics_hitdelta_chunksize.getInt() >= 0) {
            customStartIndex +=
                std::max(0, (int)this->hitdeltas.size() - cv::hud_statistics_hitdelta_chunksize.getInt());
        }

        for(int i = 0; i < this->hitdeltas.size(); i++) {
            averageDelta += (float)this->hitdeltas[i];

            if(this->hitdeltas[i] > 0) {
                // positive
                this->fHitErrorAvgMax += (float)this->hitdeltas[i];
                numPositives++;

                if(i > customStartIndex) {
                    this->fHitErrorAvgCustomMax += (float)this->hitdeltas[i];
                    numCustomPositives++;
                }
            } else if(this->hitdeltas[i] < 0) {
                // negative
                this->fHitErrorAvgMin += (float)this->hitdeltas[i];
                numNegatives++;

                if(i > customStartIndex) {
                    this->fHitErrorAvgCustomMin += (float)this->hitdeltas[i];
                    numCustomNegatives++;
                }
            } else {
                // perfect
                numPositives++;
                numNegatives++;

                if(i > customStartIndex) {
                    numCustomPositives++;
                    numCustomNegatives++;
                }
            }
        }
        averageDelta /= (float)this->hitdeltas.size();
        this->fHitErrorAvgMin = (numNegatives > 0 ? this->fHitErrorAvgMin / (float)numNegatives : 0.0f);
        this->fHitErrorAvgMax = (numPositives > 0 ? this->fHitErrorAvgMax / (float)numPositives : 0.0f);
        this->fHitErrorAvgCustomMin =
            (numCustomNegatives > 0 ? this->fHitErrorAvgCustomMin / (float)numCustomNegatives : 0.0f);
        this->fHitErrorAvgCustomMax =
            (numCustomPositives > 0 ? this->fHitErrorAvgCustomMax / (float)numCustomPositives : 0.0f);

        for(int hitdelta : this->hitdeltas) {
            this->fUnstableRate += ((float)hitdelta - averageDelta) * ((float)hitdelta - averageDelta);
        }
        this->fUnstableRate /= (float)this->hitdeltas.size();
        this->fUnstableRate = std::sqrt(this->fUnstableRate) * 10;

        // compensate for speed
        this->fUnstableRate /= beatmap->getSpeedMultiplier();
    }

    // recalculate max combo
    if(this->iCombo > this->iComboMax) this->iComboMax = this->iCombo;

    if(!this->simulating) {
        this->onScoreChange();
    }
}

void LiveScore::addHitResultComboEnd(LiveScore::HIT hit) {
    switch(hit) {
        case LiveScore::HIT::HIT_100K:
        case LiveScore::HIT::HIT_300K:
            this->iNum100ks++;
            break;

        case LiveScore::HIT::HIT_300G:
            this->iNum300gs++;
            break;

        default:
            break;
    }
}

void LiveScore::addSliderBreak() {
    this->iCombo = 0;
    this->iNumSliderBreaks++;

    this->onScoreChange();
}

void LiveScore::addPoints(int points, bool isSpinner) {
    this->iScoreV1 += (u64)points;

    if(isSpinner) this->iBonusPoints += points;  // only used for scorev2 calculation currently

    this->onScoreChange();
}

void LiveScore::setDead(bool dead) {
    if(this->bDead == dead) return;

    this->bDead = dead;

    if(this->bDead) this->bDied = true;

    this->onScoreChange();
}

void LiveScore::addKeyCount(GameplayKeys key_flag) {
    for(GameplayKeys flag = GameplayKeys::K2 /* 8 */; flag >= 1; flag = static_cast<GameplayKeys>(flag >> 1)) {
        if(!(flag & key_flag)) continue;

        switch(flag) {
            case GameplayKeys::Smoke:
                std::unreachable();
                return;
            case GameplayKeys::K1:
                this->iNumK1++;
                break;
            case GameplayKeys::K2:
                this->iNumK2++;
                break;
            case GameplayKeys::M1:
                this->iNumM1++;
                break;
            case GameplayKeys::M2:
                this->iNumM2++;
                break;
        }
    }
}

double LiveScore::getHealthIncrease(AbstractBeatmapInterface *beatmap, HIT hit) {
    return getHealthIncrease(hit, beatmap->getHP(), beatmap->fHpMultiplierNormal, beatmap->fHpMultiplierComboEnd,
                             200.0);
}

double LiveScore::getHealthIncrease(LiveScore::HIT hit, double HP, double hpMultiplierNormal,
                                    double hpMultiplierComboEnd, double hpBarMaximumForNormalization) {
    switch(hit) {
        case LiveScore::HIT::HIT_MISS:
            return (GameRules::mapDifficultyRange(HP, -6.0, -25.0, -40.0) / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_50:
            return (hpMultiplierNormal * GameRules::mapDifficultyRange(HP, 0.4 * 8.0, 0.4, 0.4) /
                    hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_100:
            return (hpMultiplierNormal * GameRules::mapDifficultyRange(HP, 2.2 * 8.0, 2.2, 2.2) /
                    hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_300:
            return (hpMultiplierNormal * 6.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_MISS_SLIDERBREAK:
            return (GameRules::mapDifficultyRange(HP, -4.0, -15.0, -28.0) / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_MU:
            return (hpMultiplierComboEnd * 6.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_100K:
            return (hpMultiplierComboEnd * 10.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_300K:
            return (hpMultiplierComboEnd * 10.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_300G:
            return (hpMultiplierComboEnd * 14.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_SLIDER10:
            return (hpMultiplierNormal * 3.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_SLIDER30:
            return (hpMultiplierNormal * 4.0 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_SPINNERSPIN:
            return (hpMultiplierNormal * 1.7 / hpBarMaximumForNormalization);

        case LiveScore::HIT::HIT_SPINNERBONUS:
            return (hpMultiplierNormal * 2.0 / hpBarMaximumForNormalization);

        default:
            return 0.f;
    }
}

int LiveScore::getKeyCount(int key) {
    switch(key) {
        case 1:
            return this->iNumK1;
        case 2:
            return this->iNumK2;
        case 3:
            return this->iNumM1;
        case 4:
            return this->iNumM2;
    }

    return 0;
}

u32 LiveScore::getModsLegacy() { return this->mods.to_legacy(); }

UString LiveScore::getModsStringForRichPresence() {
    UString modsString;

    if(osu->getModNF()) modsString.append("NF");
    if(osu->getModEZ()) modsString.append("EZ");
    if(osu->getModHD()) modsString.append("HD");
    if(osu->getModHR()) modsString.append("HR");
    if(osu->getModSD()) modsString.append("SD");
    if(osu->getModRelax()) modsString.append("RX");
    if(osu->getModAuto()) modsString.append("AT");
    if(osu->getModSpunout()) modsString.append("SO");
    if(osu->getModAutopilot()) modsString.append("AP");
    if(osu->getModSS()) modsString.append("PF");
    if(osu->getModScorev2()) modsString.append("v2");
    if(osu->getModTarget()) modsString.append("TP");
    if(osu->getModNightmare()) modsString.append("NM");
    if(osu->getModTD()) modsString.append("TD");

    return modsString;
}

u64 LiveScore::getScore() { return this->mods.has(ModFlags::ScoreV2) ? this->iScoreV2 : this->iScoreV1; }

void LiveScore::onScoreChange() {
    if(this->simulating || !osu->getRoom()) return;

    osu->getRoom()->onClientScoreChange();

    // only used to block local scores for people who think they are very clever by quickly disabling auto just before
    // the end of a beatmap
    this->bIsUnranked |= (osu->getModAuto() || (osu->getModAutopilot() && osu->getModRelax()));

    if(osu->isInPlayMode()) {
        osu->getHUD()->updateScoreboard(true);
    }
}

float LiveScore::calculateAccuracy(int num300s, int num100s, int num50s, int numMisses) {
    const float totalHitPoints = num50s * (1.0f / 6.0f) + num100s * (2.0f / 6.0f) + num300s;
    const float totalNumHits = numMisses + num50s + num100s + num300s;

    if(totalNumHits > 0.0f) return (totalHitPoints / totalNumHits);

    return 0.0f;
}

f64 FinishedScore::get_or_calc_pp() {
    assert(this->map != nullptr);

    f64 pp = this->get_pp();
    if(pp != -1.0) return pp;

    pp_calc_request request;
    request.mods = this->mods;
    request.AR = this->mods.get_naive_ar(this->map);
    request.CS = this->mods.get_naive_cs(this->map);
    request.OD = this->mods.get_naive_od(this->map);
    request.rx = this->mods.has(ModFlags::Relax);
    request.td = this->mods.has(ModFlags::TouchDevice);
    request.comboMax = this->comboMax;
    request.numMisses = this->numMisses;
    request.num300s = this->num300s;
    request.num100s = this->num100s;
    request.num50s = this->num50s;

    auto info = lct_get_pp(request);
    if(info.pp != -1.0) {
        pp = info.pp;
        this->ppv2_score = info.pp;
        this->ppv2_version = DifficultyCalculator::PP_ALGORITHM_VERSION;
        this->ppv2_total_stars = info.total_stars;
        this->ppv2_aim_stars = info.aim_stars;
        this->ppv2_speed_stars = info.speed_stars;
    }

    return pp;
};

f64 FinishedScore::get_pp() const {
    if(cv::use_ppv3.getBool() && this->ppv3_algorithm.size() > 0) {
        return this->ppv3_score;
    }

    if(this->ppv2_version < DifficultyCalculator::PP_ALGORITHM_VERSION) {
        return -1.0;
    } else {
        return this->ppv2_score;
    }
}

FinishedScore::Grade FinishedScore::calculate_grade() const {
    float totalNumHits = this->numMisses + this->num50s + this->num100s + this->num300s;
    bool modHidden = (this->mods.has(ModFlags::Hidden));
    bool modFlashlight = (this->mods.has(ModFlags::Flashlight));

    float percent300s = 0.0f;
    float percent50s = 0.0f;
    if(totalNumHits > 0.0f) {
        percent300s = this->num300s / totalNumHits;
        percent50s = this->num50s / totalNumHits;
    }

    FinishedScore::Grade grade = FinishedScore::Grade::D;
    if(percent300s > 0.6f) grade = FinishedScore::Grade::C;
    if((percent300s > 0.7f && this->numMisses == 0) || (percent300s > 0.8f)) grade = FinishedScore::Grade::B;
    if((percent300s > 0.8f && this->numMisses == 0) || (percent300s > 0.9f)) grade = FinishedScore::Grade::A;
    if(percent300s > 0.9f && percent50s <= 0.01f && this->numMisses == 0)
        grade = ((modHidden || modFlashlight) ? FinishedScore::Grade::SH : FinishedScore::Grade::S);
    if(this->numMisses == 0 && this->num50s == 0 && this->num100s == 0)
        grade = ((modHidden || modFlashlight) ? FinishedScore::Grade::XH : FinishedScore::Grade::X);

    return grade;
}
