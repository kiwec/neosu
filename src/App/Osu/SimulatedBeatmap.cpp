#include "SimulatedBeatmap.h"

#include <algorithm>

#include "DatabaseBeatmap.h"
#include "DifficultyCalculator.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "HitObjects.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "LegacyReplay.h"
#include "ModFPoSu.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "ResourceManager.h"

SimulatedBeatmap::SimulatedBeatmap(DatabaseBeatmap *diff2, Replay::Mods mods_) {
    this->selectedDifficulty2 = diff2;
    this->mods = mods_;
    this->live_score.mods = mods_;
    this->mod_halfwindow = !!(ModMasks::eq(this->mods.flags, Replay::ModFlags::HalfWindow));
    this->mod_halfwindow_allow_300s = !!(ModMasks::eq(this->mods.flags, Replay::ModFlags::HalfWindowAllow300s));
    this->mod_ming3012 = !!(ModMasks::eq(this->mods.flags, Replay::ModFlags::Ming3012));
    this->mod_no100s = !!(ModMasks::eq(this->mods.flags, Replay::ModFlags::No100s));
    this->mod_no50s = !!(ModMasks::eq(this->mods.flags, Replay::ModFlags::No50s));

    this->nb_hitobjects = diff2->getNumObjects();

    this->iNPS = 0;
    this->iND = 0;
    this->iCurrentHitObjectIndex = 0;
    this->iCurrentNumCircles = 0;
    this->iCurrentNumSliders = 0;
    this->iCurrentNumSpinners = 0;
    this->bIsSpinnerActive = false;
    this->fPlayfieldRotation = 0.0f;
    this->fXMultiplier = 1.0f;
    this->fRawHitcircleDiameter = 27.35f * 2.0f;
    this->fSliderFollowCircleDiameter = 0.0f;

    this->start();
}

SimulatedBeatmap::~SimulatedBeatmap() {
    for(auto &hitobject : this->hitobjects) {
        delete hitobject;
    }
}

void SimulatedBeatmap::simulate_to(i32 music_pos) {
    if(this->spectated_replay.size() < 2) return;

    LegacyReplay::Frame current_frame = this->spectated_replay[this->current_frame_idx];
    LegacyReplay::Frame next_frame = this->spectated_replay[this->current_frame_idx + 1];

    while(next_frame.cur_music_pos <= music_pos) {
        if(this->current_frame_idx + 2 >= this->spectated_replay.size()) break;

        this->last_keys = this->current_keys;
        f64 frame_time = (f64)(next_frame.cur_music_pos - current_frame.cur_music_pos) / 1000.0;

        this->current_frame_idx++;
        current_frame = this->spectated_replay[this->current_frame_idx];
        next_frame = this->spectated_replay[this->current_frame_idx + 1];
        this->current_keys = current_frame.key_flags;

        Click click;
        click.click_time = current_frame.cur_music_pos;
        click.pos.x = current_frame.x;
        click.pos.y = current_frame.y;

        // Flag fix to simplify logic (stable sets both K1 and M1 when K1 is pressed)
        if(this->current_keys & LegacyReplay::K1) this->current_keys &= ~LegacyReplay::M1;
        if(this->current_keys & LegacyReplay::K2) this->current_keys &= ~LegacyReplay::M2;

        // Pressed key 1
        if(!(this->last_keys & LegacyReplay::K1) && this->current_keys & LegacyReplay::K1) {
            this->bPrevKeyWasKey1 = true;
            this->clicks.push_back(click);
            if(!this->bInBreak) this->live_score.addKeyCount(1);
        }
        if(!(this->last_keys & LegacyReplay::M1) && this->current_keys & LegacyReplay::M1) {
            this->bPrevKeyWasKey1 = true;
            this->clicks.push_back(click);
            if(!this->bInBreak) this->live_score.addKeyCount(3);
        }

        // Pressed key 2
        if(!(this->last_keys & LegacyReplay::K2) && this->current_keys & LegacyReplay::K2) {
            this->bPrevKeyWasKey1 = false;
            this->clicks.push_back(click);
            if(!this->bInBreak) this->live_score.addKeyCount(2);
        }
        if(!(this->last_keys & LegacyReplay::M2) && this->current_keys & LegacyReplay::M2) {
            this->bPrevKeyWasKey1 = false;
            this->clicks.push_back(click);
            if(!this->bInBreak) this->live_score.addKeyCount(4);
        }

        this->interpolatedMousePos = Vector2{current_frame.x, current_frame.y};
        this->iCurMusicPos = current_frame.cur_music_pos;

        this->update(frame_time);
    }
}

bool SimulatedBeatmap::start() {
    // reset everything, including deleting any previously loaded hitobjects from another diff which we might just have
    // played
    this->resetScore();

    // some hitobjects already need this information to be up-to-date before their constructor is called
    this->updatePlayfieldMetrics();
    this->updateHitobjectMetrics();

    // actually load the difficulty (and the hitobjects)
    DatabaseBeatmap::LOAD_GAMEPLAY_RESULT result = DatabaseBeatmap::loadGameplay(this->selectedDifficulty2, this);
    if(result.errorCode != 0) {
        return false;
    }
    this->hitobjects = std::move(result.hitobjects);
    this->breaks = std::move(result.breaks);

    // sort hitobjects by endtime
    this->hitobjectsSortedByEndTime = this->hitobjects;
    std::ranges::sort(this->hitobjectsSortedByEndTime, Beatmap::sortHitObjectByEndTimeComp);

    // after the hitobjects have been loaded we can calculate the stacks
    this->calculateStacks();
    this->computeDrainRate();

    this->bInBreak = false;

    // NOTE: loading failures are handled dynamically in update(), so temporarily assume everything has worked in here
    return true;
}

void SimulatedBeatmap::fail(bool force_death) {
    (void)force_death;

    debugLog("SimulatedBeatmap::fail called!\n");
    this->bFailed = true;
}

void SimulatedBeatmap::cancelFailing() { this->bFailed = false; }

u32 SimulatedBeatmap::getScoreV1DifficultyMultiplier_full() const {
    // NOTE: We intentionally get CS/HP/OD from beatmap data, not "real" CS/HP/OD
    //       Since this multiplier is only used for ScoreV1
    u32 breakTimeMS = this->getBreakDurationTotal();
    f32 drainLength =
        std::max(this->getLengthPlayable() - std::min(breakTimeMS, this->getLengthPlayable()), (u32)1000) / 1000;
    return std::round(
        (this->selectedDifficulty2->getCS() + this->selectedDifficulty2->getHP() + this->selectedDifficulty2->getOD() +
         std::clamp<f32>((f32)this->selectedDifficulty2->getNumObjects() / drainLength * 8.0f, 0.0f, 16.0f)) /
        38.0f * 5.0f);
}

f32 SimulatedBeatmap::getCS_full() const {
    float CSdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::HardRock))) CSdifficultyMultiplier = 1.3f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Easy))) CSdifficultyMultiplier = 0.5f;

    f32 CS = std::clamp<f32>(this->selectedDifficulty2->getCS() * CSdifficultyMultiplier, 0.0f, 10.0f);

    if(this->mods.cs_override >= 0.0f) CS = this->mods.cs_override;
    if(this->mods.cs_overridenegative < 0.0f) CS = this->mods.cs_overridenegative;

    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Minimize)) {
        const f32 percent =
            1.0f + ((f64)(this->iCurMusicPos - this->hitobjects[0]->click_time) /
                    (f64)(this->hitobjects[this->hitobjects.size() - 1]->click_time +
                          this->hitobjects[this->hitobjects.size() - 1]->duration - this->hitobjects[0]->click_time)) *
                       this->mods.minimize_multiplier;
        CS *= percent;
    }

    CS = std::min(CS, 12.1429f);

    return CS;
}

f32 SimulatedBeatmap::getHP_full() const {
    float HPdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::HardRock))) HPdifficultyMultiplier = 1.4f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Easy))) HPdifficultyMultiplier = 0.5f;

    f32 HP = std::clamp<f32>(this->selectedDifficulty2->getHP() * HPdifficultyMultiplier, 0.0f, 10.0f);
    if(this->mods.hp_override >= 0.0f) HP = this->mods.hp_override;

    return HP;
}

f32 SimulatedBeatmap::getRawAR_full() const {
    float ARdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::HardRock))) ARdifficultyMultiplier = 1.4f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Easy))) ARdifficultyMultiplier = 0.5f;

    return std::clamp<f32>(this->selectedDifficulty2->getAR() * ARdifficultyMultiplier, 0.0f, 10.0f);
}

f32 SimulatedBeatmap::getAR_full() const {
    f32 AR = this->getRawAR();
    if(this->mods.ar_override >= 0.0f) AR = this->mods.ar_override;
    if(this->mods.ar_overridenegative < 0.0f) AR = this->mods.ar_overridenegative;

    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::AROverrideLock)) {
        AR = GameRules::getRawConstantApproachRateForSpeedMultiplier(GameRules::getRawApproachTime(AR),
                                                                     this->mods.speed);
    }

    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::ARTimewarp)) && this->hitobjects.size() > 0) {
        const f32 percent =
            1.0f - ((f64)(this->iCurMusicPos - this->hitobjects[0]->click_time) /
                    (f64)(this->hitobjects[this->hitobjects.size() - 1]->click_time +
                          this->hitobjects[this->hitobjects.size() - 1]->duration - this->hitobjects[0]->click_time)) *
                       (1.0f - this->mods.artimewarp_multiplier);
        AR *= percent;
    }

    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::ARWobble)) {
        AR += std::sin((this->iCurMusicPos / 1000.0f) * this->mods.arwobble_interval) * this->mods.arwobble_strength;
    }

    return AR;
}

f32 SimulatedBeatmap::getRawOD_full() const {
    float ODdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::HardRock))) ODdifficultyMultiplier = 1.4f;
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Easy))) ODdifficultyMultiplier = 0.5f;

    return std::clamp<f32>(this->selectedDifficulty2->getOD() * ODdifficultyMultiplier, 0.0f, 10.0f);
}

f32 SimulatedBeatmap::getOD_full() const {
    f32 OD = this->getRawOD();

    if(this->mods.od_override >= 0.0f) OD = this->mods.od_override;

    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::ODOverrideLock))
        OD = GameRules::getRawConstantOverallDifficultyForSpeedMultiplier(GameRules::getRawHitWindow300(OD),
                                                                          this->mods.speed);

    return OD;
}

bool SimulatedBeatmap::isKey1Down() const { return this->current_keys & (LegacyReplay::M1 | LegacyReplay::K1); }
bool SimulatedBeatmap::isKey2Down() const { return this->current_keys & (LegacyReplay::M2 | LegacyReplay::K2); }
bool SimulatedBeatmap::isClickHeld() const { return this->isKey1Down() || this->isKey2Down(); }

u32 SimulatedBeatmap::getLength() const { return this->selectedDifficulty2->getLengthMS(); }

u32 SimulatedBeatmap::getLengthPlayable() const {
    if(this->hitobjects.size() > 0)
        return (u32)((this->hitobjects[this->hitobjects.size() - 1]->click_time +
                      this->hitobjects[this->hitobjects.size() - 1]->duration) -
                     this->hitobjects[0]->click_time);
    else
        return this->getLength();
}

u32 SimulatedBeatmap::getBreakDurationTotal() const {
    u32 breakDurationTotal = 0;
    for(auto i : this->breaks) {
        breakDurationTotal += (u32)(i.endTime - i.startTime);
    }

    return breakDurationTotal;
}

DatabaseBeatmap::BREAK SimulatedBeatmap::getBreakForTimeRange(long startMS, long positionMS, long endMS) const {
    DatabaseBeatmap::BREAK curBreak;

    curBreak.startTime = -1;
    curBreak.endTime = -1;

    for(auto i : this->breaks) {
        if(i.startTime >= (int)startMS && i.endTime <= (int)endMS) {
            if((int)positionMS >= curBreak.startTime) curBreak = i;
        }
    }

    return curBreak;
}

LiveScore::HIT SimulatedBeatmap::addHitResult(HitObject *hitObject, LiveScore::HIT hit, i32 delta, bool isEndOfCombo,
                                              bool ignoreOnHitErrorBar, bool hitErrorBarOnly, bool ignoreCombo,
                                              bool ignoreScore, bool ignoreHealth) {
    // handle perfect & sudden death
    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Perfect)) {
        if(!hitErrorBarOnly && hit != LiveScore::HIT::HIT_300 && hit != LiveScore::HIT::HIT_300G &&
           hit != LiveScore::HIT::HIT_SLIDER10 && hit != LiveScore::HIT::HIT_SLIDER30 &&
           hit != LiveScore::HIT::HIT_SPINNERSPIN && hit != LiveScore::HIT::HIT_SPINNERBONUS) {
            this->fail();
            return LiveScore::HIT::HIT_MISS;
        }
    } else if(ModMasks::eq(this->mods.flags, Replay::ModFlags::SuddenDeath)) {
        if(hit == LiveScore::HIT::HIT_MISS) {
            this->fail();
            return LiveScore::HIT::HIT_MISS;
        }
    }

    // score
    this->live_score.addHitResult(this, hitObject, hit, delta, ignoreOnHitErrorBar, hitErrorBarOnly, ignoreCombo,
                                  ignoreScore);

    // health
    LiveScore::HIT returnedHit = LiveScore::HIT::HIT_MISS;
    if(!ignoreHealth) {
        this->addHealth(this->live_score.getHealthIncrease((BeatmapInterface *)this, hit), true);

        // geki/katu handling
        if(isEndOfCombo) {
            const int comboEndBitmask = this->live_score.getComboEndBitmask();

            if(comboEndBitmask == 0) {
                returnedHit = LiveScore::HIT::HIT_300G;
                this->addHealth(this->live_score.getHealthIncrease(this, returnedHit), true);
                this->live_score.addHitResultComboEnd(returnedHit);
            } else if((comboEndBitmask & 2) == 0) {
                if(hit == LiveScore::HIT::HIT_100) {
                    returnedHit = LiveScore::HIT::HIT_100K;
                    this->addHealth(this->live_score.getHealthIncrease(this, returnedHit), true);
                    this->live_score.addHitResultComboEnd(returnedHit);
                } else if(hit == LiveScore::HIT::HIT_300) {
                    returnedHit = LiveScore::HIT::HIT_300K;
                    this->addHealth(this->live_score.getHealthIncrease(this, returnedHit), true);
                    this->live_score.addHitResultComboEnd(returnedHit);
                }
            } else if(hit != LiveScore::HIT::HIT_MISS)
                this->addHealth(this->live_score.getHealthIncrease(this, LiveScore::HIT::HIT_MU), true);

            this->live_score.setComboEndBitmask(0);
        }
    }

    return returnedHit;
}

void SimulatedBeatmap::addSliderBreak() {
    // handle perfect & sudden death
    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Perfect)) {
        this->fail();
        return;
    } else if(ModMasks::eq(this->mods.flags, Replay::ModFlags::SuddenDeath)) {
        this->fail();
        return;
    }

    // score
    this->live_score.addSliderBreak();
}

void SimulatedBeatmap::addScorePoints(int points, bool isSpinner) { this->live_score.addPoints(points, isSpinner); }

void SimulatedBeatmap::addHealth(f64 percent, bool isFromHitResult) {
    // never drain before first hitobject
    if(this->iCurMusicPos < this->hitobjects[0]->click_time) return;

    // never drain after last hitobject
    if(this->iCurMusicPos > (this->hitobjectsSortedByEndTime[this->hitobjectsSortedByEndTime.size() - 1]->click_time +
                             this->hitobjectsSortedByEndTime[this->hitobjectsSortedByEndTime.size() - 1]->duration))
        return;

    if(this->bFailed) {
        this->fHealth = 0.0;
        return;
    }

    this->fHealth = std::clamp<f64>(this->fHealth + percent, 0.0, 1.0);

    // handle generic fail state (2)
    const bool isDead = this->fHealth < 0.001;
    if(isDead && !(ModMasks::eq(this->mods.flags, Replay::ModFlags::NoFail))) {
        if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Easy)) &&
           this->live_score.getNumEZRetries() > 0)  // retries with ez
        {
            this->live_score.setNumEZRetries(this->live_score.getNumEZRetries() - 1);

            // special case: set health to 160/200 (osu!stable behavior, seems fine for all drains)
            this->fHealth = 160.0f / 200.f;
        } else if(isFromHitResult && percent < 0.0) {
            // judgement fail
            this->fail();
        }
    }
}

long SimulatedBeatmap::getPVS() {
    // this is an approximation with generous boundaries, it doesn't need to be exact (just good enough to filter 10000
    // hitobjects down to a few hundred or so) it will be used in both positive and negative directions (previous and
    // future hitobjects) to speed up loops which iterate over all hitobjects
    return this->getApproachTime() + GameRules::getFadeInTime() + (long)GameRules::getHitWindowMiss() + 1500;  // sanity
}

void SimulatedBeatmap::resetScore() {
    this->current_keys = 0;
    this->last_keys = 0;
    this->current_frame_idx = 0;

    this->fHealth = 1.0;
    this->bFailed = false;

    this->live_score.reset();
}

void SimulatedBeatmap::update(f64 frame_time) {
    if(this->hitobjects.empty()) return;

    auto last_hitobject = this->hitobjectsSortedByEndTime[this->hitobjectsSortedByEndTime.size() - 1];
    const bool isAfterLastHitObject = (this->iCurMusicPos > (last_hitobject->click_time + last_hitobject->duration));
    if(isAfterLastHitObject) {
        return;
    }

    // live update hitobject and playfield metrics
    this->updateHitobjectMetrics();
    this->updatePlayfieldMetrics();

    // wobble mod
    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Wobble1)) {
        const f32 speedMultiplierCompensation = 1.0f / this->mods.speed;
        this->fPlayfieldRotation =
            (this->iCurMusicPos / 1000.0f) * 30.0f * speedMultiplierCompensation * this->mods.wobble_rotation_speed;
        this->fPlayfieldRotation = std::fmod(this->fPlayfieldRotation, 360.0f);
    } else {
        this->fPlayfieldRotation = 0.0f;
    }

    // for performance reasons, a lot of operations are crammed into 1 loop over all hitobjects:
    // update all hitobjects,
    // handle click events,
    // also get the time of the next/previous hitobject and their indices for later,
    // and get the current hitobject,
    // also handle miss hiterrorbar slots,
    // also calculate nps and nd,
    // also handle note blocking
    this->currentHitObject = NULL;
    this->iNextHitObjectTime = 0;
    this->iPreviousHitObjectTime = 0;
    this->iNPS = 0;
    this->iND = 0;
    this->iCurrentNumCircles = 0;
    this->iCurrentNumSliders = 0;
    this->iCurrentNumSpinners = 0;
    {
        bool blockNextNotes = false;
        bool spinner_active = false;

        const long pvs = this->getPVS();
        const int notelockType = this->mods.notelock_type;
        const long tolerance2B = 3;

        this->iCurrentHitObjectIndex = 0;  // reset below here, since it's needed for mafham pvs

        for(int i = 0; i < this->hitobjects.size(); i++) {
            // the order must be like this:
            // 0) miscellaneous stuff (minimal performance impact)
            // 1) prev + next time vars
            // 2) PVS optimization
            // 3) main hitobject update
            // 4) note blocking
            // 5) click events
            //
            // (because the hitobjects need to know about note blocking before handling the click events)

            // ************ live pp block start ************ //
            const bool isCircle = this->hitobjects[i]->type == HitObjectType::CIRCLE;
            const bool isSlider = this->hitobjects[i]->type == HitObjectType::SLIDER;
            const bool isSpinner = this->hitobjects[i]->type == HitObjectType::SPINNER;
            // ************ live pp block end ************** //

            // determine previous & next object time, used for auto + followpoints + warning arrows + empty section
            // skipping
            if(this->iNextHitObjectTime == 0) {
                if(this->hitobjects[i]->click_time > this->iCurMusicPos) {
                    this->iNextHitObjectTime = this->hitobjects[i]->click_time;
                } else {
                    this->currentHitObject = this->hitobjects[i];
                    const long actualPrevHitObjectTime =
                        this->hitobjects[i]->click_time + this->hitobjects[i]->duration;
                    this->iPreviousHitObjectTime = actualPrevHitObjectTime;
                }
            }

            // PVS optimization
            if(this->hitobjects[i]->isFinished() &&
               (this->iCurMusicPos - pvs >
                this->hitobjects[i]->click_time + this->hitobjects[i]->duration))  // past objects
            {
                // ************ live pp block start ************ //
                if(isCircle) this->iCurrentNumCircles++;
                if(isSlider) this->iCurrentNumSliders++;
                if(isSpinner) this->iCurrentNumSpinners++;

                this->iCurrentHitObjectIndex = i;
                // ************ live pp block end ************** //

                continue;
            }
            if(this->hitobjects[i]->click_time > this->iCurMusicPos + pvs)  // future objects
                break;

            // ************ live pp block start ************ //
            if(this->iCurMusicPos >= this->hitobjects[i]->click_time + this->hitobjects[i]->duration) {
                this->iCurrentHitObjectIndex = i;
            }
            // ************ live pp block end ************** //

            // main hitobject update
            this->hitobjects[i]->update(this->iCurMusicPos, frame_time);

            // spinner visibility detection
            // XXX: there might be a "better" way to do it?
            if(isSpinner) {
                bool spinner_started = this->iCurMusicPos >= this->hitobjects[i]->click_time;
                bool spinner_ended =
                    this->iCurMusicPos > this->hitobjects[i]->click_time + this->hitobjects[i]->duration;
                spinner_active |= (spinner_started && !spinner_ended);
            }

            // note blocking / notelock (1)
            const Slider *currentSliderPointer = dynamic_cast<Slider *>(this->hitobjects[i]);
            if(notelockType > 0) {
                this->hitobjects[i]->setBlocked(blockNextNotes);

                if(notelockType == 1)  // neosu
                {
                    // (nothing, handled in (2) block)
                } else if(notelockType == 2)  // osu!stable
                {
                    if(!this->hitobjects[i]->isFinished()) {
                        blockNextNotes = true;

                        // Sliders are "finished" after they end
                        // Extra handling for simultaneous/2b hitobjects, as these would otherwise get blocked
                        // NOTE: this will still unlock some simultaneous/2b patterns too early
                        //       (slider slider circle [circle]), but nobody from that niche has complained so far
                        if(isSlider || isSpinner) {
                            if((i + 1) < this->hitobjects.size()) {
                                if((isSpinner || currentSliderPointer->isStartCircleFinished()) &&
                                   (this->hitobjects[i + 1]->click_time <=
                                    (this->hitobjects[i]->click_time + this->hitobjects[i]->duration + tolerance2B)))
                                    blockNextNotes = false;
                            }
                        }
                    }
                } else if(notelockType == 3)  // osu!lazer 2020
                {
                    if(!this->hitobjects[i]->isFinished()) {
                        // spinners are completely ignored (transparent)
                        if(!isSpinner) {
                            blockNextNotes = (this->iCurMusicPos <= this->hitobjects[i]->click_time);

                            // sliders are "finished" after their startcircle
                            if(isSlider && currentSliderPointer->isStartCircleFinished()) {
                                // sliders with finished startcircles do not block
                                blockNextNotes = false;
                            }
                        }
                    }
                }
            } else {
                this->hitobjects[i]->setBlocked(false);
            }

            // click events (this also handles hitsounds!)
            const bool isCurrentHitObjectASliderAndHasItsStartCircleFinishedBeforeClickEvents =
                (currentSliderPointer != NULL && currentSliderPointer->isStartCircleFinished());
            const bool isCurrentHitObjectFinishedBeforeClickEvents = this->hitobjects[i]->isFinished();
            {
                if(this->clicks.size() > 0) this->hitobjects[i]->onClickEvent(this->clicks);
            }
            const bool isCurrentHitObjectFinishedAfterClickEvents = this->hitobjects[i]->isFinished();
            const bool isCurrentHitObjectASliderAndHasItsStartCircleFinishedAfterClickEvents =
                (currentSliderPointer != NULL && currentSliderPointer->isStartCircleFinished());

            // note blocking / notelock (2.1)
            if(!isCurrentHitObjectASliderAndHasItsStartCircleFinishedBeforeClickEvents &&
               isCurrentHitObjectASliderAndHasItsStartCircleFinishedAfterClickEvents) {
                // in here if a slider had its startcircle clicked successfully in this update iteration

                if(notelockType == 2)  // osu!stable
                {
                    // edge case: frame perfect double tapping on overlapping sliders would incorrectly eat the second
                    // input, because the isStartCircleFinished() 2b edge case check handling happens before
                    // m_hitobjects[i]->onClickEvent(this->clicks); so, we check if the currentSliderPointer got its
                    // isStartCircleFinished() within this m_hitobjects[i]->onClickEvent(this->clicks); and unlock
                    // blockNextNotes if that is the case note that we still only unlock within duration + tolerance2B
                    // (same as in (1))
                    if((i + 1) < this->hitobjects.size()) {
                        if((this->hitobjects[i + 1]->click_time <=
                            (this->hitobjects[i]->click_time + this->hitobjects[i]->duration + tolerance2B)))
                            blockNextNotes = false;
                    }
                }
            }

            // note blocking / notelock (2.2)
            if(!isCurrentHitObjectFinishedBeforeClickEvents && isCurrentHitObjectFinishedAfterClickEvents) {
                // in here if a hitobject has been clicked (and finished completely) successfully in this update
                // iteration

                blockNextNotes = false;

                if(notelockType == 1)  // neosu
                {
                    // auto miss all previous unfinished hitobjects, always
                    // (can stop reverse iteration once we get to the first finished hitobject)

                    for(int m = i - 1; m >= 0; m--) {
                        if(this->hitobjects[m]->isFinished()) break;

                        // spinners are completely ignored (transparent)
                        if(this->hitobjects[m]->type == HitObjectType::SPINNER) continue;

                        // NOTE: 2b exception. only force miss if objects are not overlapping.
                        if(this->hitobjects[i]->click_time >
                           (this->hitobjects[m]->click_time + this->hitobjects[m]->duration)) {
                            this->hitobjects[m]->miss(this->iCurMusicPos);
                        }
                    }
                } else if(notelockType == 2)  // osu!stable
                {
                    // (nothing, handled in (1) and (2.1) blocks)
                } else if(notelockType == 3)  // osu!lazer 2020
                {
                    // auto miss all previous unfinished hitobjects if the current music time is > their time (center)
                    // (can stop reverse iteration once we get to the first finished hitobject)

                    for(int m = i - 1; m >= 0; m--) {
                        if(this->hitobjects[m]->isFinished()) break;

                        // spinners are completely ignored (transparent)
                        if(this->hitobjects[m]->type == HitObjectType::SPINNER) continue;

                        if(this->iCurMusicPos > this->hitobjects[m]->click_time) {
                            // NOTE: 2b exception. only force miss if objects are not overlapping.
                            if(this->hitobjects[i]->click_time >
                               (this->hitobjects[m]->click_time + this->hitobjects[m]->duration)) {
                                this->hitobjects[m]->miss(this->iCurMusicPos);
                            }
                        }
                    }
                }
            }

            // ************ live pp block start ************ //
            if(isCircle && this->hitobjects[i]->isFinished()) this->iCurrentNumCircles++;
            if(isSlider && this->hitobjects[i]->isFinished()) this->iCurrentNumSliders++;
            if(isSpinner && this->hitobjects[i]->isFinished()) this->iCurrentNumSpinners++;

            if(this->hitobjects[i]->isFinished()) this->iCurrentHitObjectIndex = i;
            // ************ live pp block end ************** //

            // notes per second
            const long npsHalfGateSizeMS = (long)(500.0f * this->mods.speed);
            if(this->hitobjects[i]->click_time > this->iCurMusicPos - npsHalfGateSizeMS &&
               this->hitobjects[i]->click_time < this->iCurMusicPos + npsHalfGateSizeMS)
                this->iNPS++;

            // note density
            if(this->hitobjects[i]->isVisible()) this->iND++;
        }

        // all remaining clicks which have not been consumed by any hitobjects can safely be deleted
        if(this->clicks.size() > 0) {
            // nightmare mod: extra clicks = sliderbreak
            bool break_on_extra_click = (ModMasks::eq(this->mods.flags, Replay::ModFlags::Nightmare) ||
                                         (ModMasks::eq(this->mods.flags, Replay::ModFlags::Jigsaw1)));
            break_on_extra_click &= !this->bInBreak && !spinner_active;
            break_on_extra_click &= this->iCurrentHitObjectIndex > 0;
            if(break_on_extra_click) {
                this->addSliderBreak();
                this->addHitResult(NULL, LiveScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true, true,
                                   false);  // only decrease health
            }

            this->clicks.clear();
        }
    }

    // break time detection
    const DatabaseBeatmap::BREAK breakEvent =
        this->getBreakForTimeRange(this->iPreviousHitObjectTime, this->iCurMusicPos, this->iNextHitObjectTime);
    const bool isInBreak =
        ((int)this->iCurMusicPos >= breakEvent.startTime && (int)this->iCurMusicPos <= breakEvent.endTime);
    if(isInBreak != this->bInBreak) {
        this->bInBreak = !this->bInBreak;
    }

    // hp drain & failing
    // handle constant drain
    {
        if(this->fDrainRate > 0.0) {
            if(!this->bInBreak) {
                // special case: break drain edge cases
                bool drainAfterLastHitobjectBeforeBreakStart = (this->selectedDifficulty2->getVersion() < 8);

                const bool isBetweenHitobjectsAndBreak = (int)this->iPreviousHitObjectTime <= breakEvent.startTime &&
                                                         (int)this->iNextHitObjectTime >= breakEvent.endTime &&
                                                         this->iCurMusicPos > this->iPreviousHitObjectTime;
                const bool isLastHitobjectBeforeBreakStart =
                    isBetweenHitobjectsAndBreak && (int)this->iCurMusicPos <= breakEvent.startTime;

                if(!isBetweenHitobjectsAndBreak ||
                   (drainAfterLastHitobjectBeforeBreakStart && isLastHitobjectBeforeBreakStart)) {
                    // special case: spinner nerf
                    f64 spinnerDrainNerf = this->bIsSpinnerActive ? 0.25 : 1.0;
                    this->addHealth(-this->fDrainRate * frame_time * (f64)this->mods.speed * spinnerDrainNerf, false);
                }
            }
        }
    }

    // revive in mp
    if(this->fHealth > 0.999 && this->live_score.isDead()) this->live_score.setDead(false);

    // update auto (after having updated the hitobjects)
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Autopilot))) this->updateAutoCursorPos();

    // spinner detection (used by osu!stable drain, and by HUD for not drawing the hiterrorbar)
    if(this->currentHitObject != NULL) {
        Spinner *spinnerPointer = dynamic_cast<Spinner *>(this->currentHitObject);
        if(spinnerPointer != NULL && this->iCurMusicPos > this->currentHitObject->click_time &&
           this->iCurMusicPos < this->currentHitObject->click_time + this->currentHitObject->duration)
            this->bIsSpinnerActive = true;
        else
            this->bIsSpinnerActive = false;
    }

    // full alternate mod lenience
    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::FullAlternate)) {
        if(this->bInBreak || this->bIsSpinnerActive || this->iCurrentHitObjectIndex < 1)
            this->iAllowAnyNextKeyForFullAlternateUntilHitObjectIndex = this->iCurrentHitObjectIndex + 1;
    }
}

Vector2 SimulatedBeatmap::pixels2OsuCoords(Vector2 pixelCoords) const { return pixelCoords; }

Vector2 SimulatedBeatmap::osuCoords2Pixels(Vector2 coords) const {
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::HardRock))) coords.y = GameRules::OSU_COORD_HEIGHT - coords.y;

    // wobble
    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Wobble1)) {
        const f32 speedMultiplierCompensation = 1.0f / this->mods.speed;
        coords.x +=
            std::sin((this->iCurMusicPos / 1000.0f) * 5 * speedMultiplierCompensation * this->mods.wobble_frequency) *
            this->mods.wobble_strength;
        coords.y +=
            std::sin((this->iCurMusicPos / 1000.0f) * 4 * speedMultiplierCompensation * this->mods.wobble_frequency) *
            this->mods.wobble_strength;
    }

    // wobble2
    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Wobble2)) {
        const f32 speedMultiplierCompensation = 1.0f / this->mods.speed;
        Vector2 centerDelta = coords - Vector2(GameRules::OSU_COORD_WIDTH, GameRules::OSU_COORD_HEIGHT) / 2;
        coords.x +=
            centerDelta.x * 0.25f *
            std::sin((this->iCurMusicPos / 1000.0f) * 5 * speedMultiplierCompensation * this->mods.wobble_frequency) *
            this->mods.wobble_strength;
        coords.y +=
            centerDelta.y * 0.25f *
            std::sin((this->iCurMusicPos / 1000.0f) * 3 * speedMultiplierCompensation * this->mods.wobble_frequency) *
            this->mods.wobble_strength;
    }

    // if wobble, clamp coordinates

    if(ModMasks::eq(this->mods.flags, Replay::ModFlags::Wobble1) ||
       ModMasks::eq(this->mods.flags, Replay::ModFlags::Wobble2)) {
        coords.x = std::clamp<f32>(coords.x, 0.0f, GameRules::OSU_COORD_WIDTH);
        coords.y = std::clamp<f32>(coords.y, 0.0f, GameRules::OSU_COORD_HEIGHT);
    }

    return coords;
}

Vector2 SimulatedBeatmap::osuCoords2RawPixels(Vector2 coords) const { return coords; }

Vector2 SimulatedBeatmap::osuCoords2LegacyPixels(Vector2 coords) const {
    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::HardRock))) coords.y = GameRules::OSU_COORD_HEIGHT - coords.y;

    // VR center
    coords.x -= GameRules::OSU_COORD_WIDTH / 2;
    coords.y -= GameRules::OSU_COORD_HEIGHT / 2;

    return coords;
}

Vector2 SimulatedBeatmap::getCursorPos() const { return this->interpolatedMousePos; }

Vector2 SimulatedBeatmap::getFirstPersonCursorDelta() const {
    return this->vPlayfieldCenter - ((ModMasks::eq(this->mods.flags, Replay::ModFlags::Autopilot))
                                         ? this->vAutoCursorPos
                                         : this->getCursorPos());
}

void SimulatedBeatmap::updateAutoCursorPos() {
    this->vAutoCursorPos = this->vPlayfieldCenter;
    this->vAutoCursorPos.y *= 2.5f;  // start moving in offscreen from bottom

    if(this->hitobjects.size() < 1) {
        this->vAutoCursorPos = this->vPlayfieldCenter;
        return;
    }

    // general
    long prevTime = 0;
    long nextTime = this->hitobjects[0]->click_time;
    Vector2 prevPos = this->vAutoCursorPos;
    Vector2 curPos = this->vAutoCursorPos;
    Vector2 nextPos = this->vAutoCursorPos;
    bool haveCurPos = false;

    if((ModMasks::eq(this->mods.flags, Replay::ModFlags::Autopilot))) {
        for(auto o : this->hitobjects) {
            // get previous object
            if(o->isFinished() ||
               (this->iCurMusicPos >
                o->click_time + o->duration + (long)(this->getHitWindow50() * this->mods.autopilot_lenience))) {
                prevTime = o->click_time + o->duration + o->getAutopilotDelta();
                prevPos = o->getAutoCursorPos(this->iCurMusicPos);
            } else if(!o->isFinished())  // get next object
            {
                nextPos = o->getAutoCursorPos(this->iCurMusicPos);
                nextTime = o->click_time;

                // wait for the user to click
                if(this->iCurMusicPos >= nextTime + o->duration) {
                    haveCurPos = true;
                    curPos = nextPos;

                    // long delta = m_iCurMusicPos - (nextTime + o->duration);
                    o->setAutopilotDelta(this->iCurMusicPos - (nextTime + o->duration));
                } else if(o->duration > 0 && this->iCurMusicPos >= nextTime)  // handle objects with duration
                {
                    haveCurPos = true;
                    curPos = nextPos;
                    o->setAutopilotDelta(0);
                }

                break;
            }
        }
    }

    if(haveCurPos)  // in active hitObject
        this->vAutoCursorPos = curPos;
    else {
        // interpolation
        f32 percent = 1.0f;
        if((nextTime == 0 && prevTime == 0) || (nextTime - prevTime) == 0)
            percent = 1.0f;
        else
            percent = (f32)((long)this->iCurMusicPos - prevTime) / (f32)(nextTime - prevTime);

        percent = std::clamp<f32>(percent, 0.0f, 1.0f);

        // scaled distance (not osucoords)
        f32 distance = (nextPos - prevPos).length();
        if(distance > this->fHitcircleDiameter * 1.05f)  // snap only if not in a stream (heuristic)
        {
            percent = 1.f;
        }

        this->vAutoCursorPos = prevPos + (nextPos - prevPos) * percent;
    }
}

void SimulatedBeatmap::updatePlayfieldMetrics() {
    this->vPlayfieldSize = GameRules::getPlayfieldSize();
    this->vPlayfieldCenter = GameRules::getPlayfieldCenter();
}

void SimulatedBeatmap::updateHitobjectMetrics() {
    this->fRawHitcircleDiameter = GameRules::getRawHitCircleDiameter(this->getCS());
    this->fXMultiplier = GameRules::getHitCircleXMultiplier();
    this->fHitcircleDiameter = GameRules::getRawHitCircleDiameter(this->getCS()) * GameRules::getHitCircleXMultiplier();

    const f32 followcircle_size_multiplier = 2.4f;
    const f32 sliderFollowCircleDiameterMultiplier =
        ((ModMasks::eq(this->mods.flags, Replay::ModFlags::Nightmare) ||
          (ModMasks::eq(this->mods.flags, Replay::ModFlags::Jigsaw2)))
             ? (1.0f * (1.0f - this->mods.jigsaw_followcircle_radius_factor) +
                this->mods.jigsaw_followcircle_radius_factor * followcircle_size_multiplier)
             : followcircle_size_multiplier);
    this->fSliderFollowCircleDiameter = this->fHitcircleDiameter * sliderFollowCircleDiameterMultiplier;
}

void SimulatedBeatmap::calculateStacks() {
    this->updateHitobjectMetrics();

    debugLog("Beatmap: Calculating stacks ...\n");

    // reset
    for(auto &hitobject : this->hitobjects) {
        hitobject->setStack(0);
    }

    const f32 STACK_LENIENCE = 3.0f;
    const f32 STACK_OFFSET = 0.05f;

    const f32 approachTime =
        GameRules::mapDifficultyRange(this->getAR(), GameRules::getMinApproachTime(), GameRules::getMidApproachTime(),
                                      GameRules::getMaxApproachTime());
    const f32 stackLeniency = this->selectedDifficulty2->getStackLeniency();

    if(this->selectedDifficulty2->getVersion() > 5) {
        // peppy's algorithm
        // https://gist.github.com/peppy/1167470

        for(int i = this->hitobjects.size() - 1; i >= 0; i--) {
            int n = i;

            HitObject *objectI = this->hitobjects[i];

            bool isSpinner = dynamic_cast<Spinner *>(objectI) != NULL;

            if(objectI->getStack() != 0 || isSpinner) continue;

            bool isHitCircle = dynamic_cast<Circle *>(objectI) != NULL;
            bool isSlider = dynamic_cast<Slider *>(objectI) != NULL;

            if(isHitCircle) {
                while(--n >= 0) {
                    HitObject *objectN = this->hitobjects[n];

                    bool isSpinnerN = dynamic_cast<Spinner *>(objectN);

                    if(isSpinnerN) continue;

                    if(objectI->click_time - (approachTime * stackLeniency) > (objectN->click_time + objectN->duration))
                        break;

                    Vector2 objectNEndPosition = objectN->getOriginalRawPosAt(objectN->click_time + objectN->duration);
                    if(objectN->duration != 0 &&
                       (objectNEndPosition - objectI->getOriginalRawPosAt(objectI->click_time)).length() <
                           STACK_LENIENCE) {
                        int offset = objectI->getStack() - objectN->getStack() + 1;
                        for(int j = n + 1; j <= i; j++) {
                            if((objectNEndPosition -
                                this->hitobjects[j]->getOriginalRawPosAt(this->hitobjects[j]->click_time))
                                   .length() < STACK_LENIENCE)
                                this->hitobjects[j]->setStack(this->hitobjects[j]->getStack() - offset);
                        }

                        break;
                    }

                    if((objectN->getOriginalRawPosAt(objectN->click_time) -
                        objectI->getOriginalRawPosAt(objectI->click_time))
                           .length() < STACK_LENIENCE) {
                        objectN->setStack(objectI->getStack() + 1);
                        objectI = objectN;
                    }
                }
            } else if(isSlider) {
                while(--n >= 0) {
                    HitObject *objectN = this->hitobjects[n];

                    bool isSpinner = dynamic_cast<Spinner *>(objectN) != NULL;

                    if(isSpinner) continue;

                    if(objectI->click_time - (approachTime * stackLeniency) > objectN->click_time) break;

                    if(((objectN->duration != 0 ? objectN->getOriginalRawPosAt(objectN->click_time + objectN->duration)
                                                : objectN->getOriginalRawPosAt(objectN->click_time)) -
                        objectI->getOriginalRawPosAt(objectI->click_time))
                           .length() < STACK_LENIENCE) {
                        objectN->setStack(objectI->getStack() + 1);
                        objectI = objectN;
                    }
                }
            }
        }
    } else  // getSelectedDifficulty()->version < 6
    {
        // old stacking algorithm for old beatmaps
        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Beatmaps/BeatmapProcessor.cs

        for(int i = 0; i < this->hitobjects.size(); i++) {
            HitObject *currHitObject = this->hitobjects[i];
            Slider *sliderPointer = dynamic_cast<Slider *>(currHitObject);

            const bool isSlider = (sliderPointer != NULL);

            if(currHitObject->getStack() != 0 && !isSlider) continue;

            long startTime = currHitObject->click_time + currHitObject->duration;
            int sliderStack = 0;

            for(int j = i + 1; j < this->hitobjects.size(); j++) {
                HitObject *objectJ = this->hitobjects[j];

                if(objectJ->click_time - (approachTime * stackLeniency) > startTime) break;

                // "The start position of the hitobject, or the position at the end of the path if the hitobject is a
                // slider"
                Vector2 position2 =
                    isSlider ? sliderPointer->getOriginalRawPosAt(sliderPointer->click_time + sliderPointer->duration)
                             : currHitObject->getOriginalRawPosAt(currHitObject->click_time);

                if((objectJ->getOriginalRawPosAt(objectJ->click_time) -
                    currHitObject->getOriginalRawPosAt(currHitObject->click_time))
                       .length() < 3) {
                    currHitObject->setStack(currHitObject->getStack() + 1);
                    startTime = objectJ->click_time + objectJ->duration;
                } else if((objectJ->getOriginalRawPosAt(objectJ->click_time) - position2).length() < 3) {
                    // "Case for sliders - bump notes down and right, rather than up and left."
                    sliderStack++;
                    objectJ->setStack(objectJ->getStack() - sliderStack);
                    startTime = objectJ->click_time + objectJ->duration;
                }
            }
        }
    }

    // update hitobject positions
    f32 stackOffset = this->fRawHitcircleDiameter * STACK_OFFSET;
    for(auto &hitobject : this->hitobjects) {
        if(hitobject->getStack() != 0) hitobject->updateStackPosition(stackOffset);
    }
}

void SimulatedBeatmap::computeDrainRate() {
    this->fDrainRate = 0.0;
    this->fHpMultiplierNormal = 1.0;
    this->fHpMultiplierComboEnd = 1.0;

    if(this->hitobjects.size() < 1) return;

    debugLog("Beatmap: Calculating drain ...\n");

    {
        // see https://github.com/ppy/osu-iPhone/blob/master/Classes/OsuPlayer.m
        // see calcHPDropRate() @ https://github.com/ppy/osu-iPhone/blob/master/Classes/OsuFiletype.m#L661

        // NOTE: all drain changes between 2014 and today have been fixed here (the link points to an old version of the
        // algorithm!) these changes include: passive spinner nerf (drain * 0.25 while spinner is active), and clamping
        // the object length drain to 0 + an extra check for that (see maxLongObjectDrop) see
        // https://osu.ppy.sh/home/changelog/stable40/20190513.2

        struct TestPlayer {
            TestPlayer(f64 hpBarMaximum) {
                this->hpBarMaximum = hpBarMaximum;

                this->hpMultiplierNormal = 1.0;
                this->hpMultiplierComboEnd = 1.0;

                this->resetHealth();
            }

            void resetHealth() {
                this->health = this->hpBarMaximum;
                this->healthUncapped = this->hpBarMaximum;
            }

            void increaseHealth(f64 amount) {
                this->healthUncapped += amount;
                this->health += amount;

                if(this->health > this->hpBarMaximum) this->health = this->hpBarMaximum;

                if(this->health < 0.0) this->health = 0.0;

                if(this->healthUncapped < 0.0) this->healthUncapped = 0.0;
            }

            void decreaseHealth(f64 amount) {
                this->health -= amount;

                if(this->health < 0.0) this->health = 0.0;

                if(this->health > this->hpBarMaximum) this->health = this->hpBarMaximum;

                this->healthUncapped -= amount;

                if(this->healthUncapped < 0.0) this->healthUncapped = 0.0;
            }

            f64 hpBarMaximum;

            f64 health;
            f64 healthUncapped;

            f64 hpMultiplierNormal;
            f64 hpMultiplierComboEnd;
        };
        TestPlayer testPlayer(200.f);

        const f64 HP = this->getHP();
        const int version = this->selectedDifficulty2->getVersion();

        f64 testDrop = 0.05;

        const f64 lowestHpEver = GameRules::mapDifficultyRangeDouble(HP, 195.0, 160.0, 60.0);
        const f64 lowestHpComboEnd = GameRules::mapDifficultyRangeDouble(HP, 198.0, 170.0, 80.0);
        const f64 lowestHpEnd = GameRules::mapDifficultyRangeDouble(HP, 198.0, 180.0, 80.0);
        const f64 HpRecoveryAvailable = GameRules::mapDifficultyRangeDouble(HP, 8.0, 4.0, 0.0);

        bool fail = false;

        do {
            testPlayer.resetHealth();

            f64 lowestHp = testPlayer.health;
            int lastTime = (int)(this->hitobjects[0]->click_time - (long)this->getApproachTime());
            fail = false;

            const int breakCount = this->breaks.size();
            int breakNumber = 0;

            int comboTooLowCount = 0;

            for(int i = 0; i < this->hitobjects.size(); i++) {
                const HitObject *h = this->hitobjects[i];
                const Slider *sliderPointer = dynamic_cast<const Slider *>(h);
                const Spinner *spinnerPointer = dynamic_cast<const Spinner *>(h);

                const int localLastTime = lastTime;

                int breakTime = 0;
                if(breakCount > 0 && breakNumber < breakCount) {
                    const DatabaseBeatmap::BREAK &e = this->breaks[breakNumber];
                    if(e.startTime >= localLastTime && e.endTime <= h->click_time) {
                        // consider break start equal to object end time for version 8+ since drain stops during this
                        // time
                        breakTime = (version < 8) ? (e.endTime - e.startTime) : (e.endTime - localLastTime);
                        breakNumber++;
                    }
                }

                testPlayer.decreaseHealth(testDrop * (h->click_time - lastTime - breakTime));

                lastTime = (int)(h->click_time + h->duration);

                if(testPlayer.health < lowestHp) lowestHp = testPlayer.health;

                if(testPlayer.health > lowestHpEver) {
                    const f64 longObjectDrop = testDrop * (f64)h->duration;
                    const f64 maxLongObjectDrop = std::max(0.0, longObjectDrop - testPlayer.health);

                    testPlayer.decreaseHealth(longObjectDrop);

                    // nested hitobjects
                    if(sliderPointer != NULL) {
                        // startcircle
                        testPlayer.increaseHealth(LiveScore::getHealthIncrease(
                            LiveScore::HIT::HIT_SLIDER30, HP, testPlayer.hpMultiplierNormal,
                            testPlayer.hpMultiplierComboEnd, 1.0));  // slider30

                        // ticks + repeats + repeat ticks
                        const std::vector<Slider::SLIDERCLICK> &clicks = sliderPointer->getClicks();
                        for(const auto &click : clicks) {
                            switch(click.type) {
                                case 0:  // repeat
                                    testPlayer.increaseHealth(LiveScore::getHealthIncrease(
                                        LiveScore::HIT::HIT_SLIDER30, HP, testPlayer.hpMultiplierNormal,
                                        testPlayer.hpMultiplierComboEnd, 1.0));  // slider30
                                    break;
                                case 1:  // tick
                                    testPlayer.increaseHealth(LiveScore::getHealthIncrease(
                                        LiveScore::HIT::HIT_SLIDER10, HP, testPlayer.hpMultiplierNormal,
                                        testPlayer.hpMultiplierComboEnd, 1.0));  // slider10
                                    break;
                            }
                        }

                        // endcircle
                        testPlayer.increaseHealth(LiveScore::getHealthIncrease(
                            LiveScore::HIT::HIT_SLIDER30, HP, testPlayer.hpMultiplierNormal,
                            testPlayer.hpMultiplierComboEnd, 1.0));  // slider30
                    } else if(spinnerPointer != NULL) {
                        const int rotationsNeeded =
                            (int)((f32)spinnerPointer->duration / 1000.0f * GameRules::getSpinnerSpinsPerSecond(this));
                        for(int r = 0; r < rotationsNeeded; r++) {
                            testPlayer.increaseHealth(LiveScore::getHealthIncrease(
                                LiveScore::HIT::HIT_SPINNERSPIN, HP, testPlayer.hpMultiplierNormal,
                                testPlayer.hpMultiplierComboEnd, 1.0));  // spinnerspin
                        }
                    }

                    if(!(maxLongObjectDrop > 0.0) || (testPlayer.health - maxLongObjectDrop) > lowestHpEver) {
                        // regular hit (for every hitobject)
                        testPlayer.increaseHealth(
                            LiveScore::getHealthIncrease(LiveScore::HIT::HIT_300, HP, testPlayer.hpMultiplierNormal,
                                                         testPlayer.hpMultiplierComboEnd, 1.0));  // 300

                        // end of combo (new combo starts at next hitobject)
                        if((i == this->hitobjects.size() - 1) || this->hitobjects[i]->is_end_of_combo) {
                            testPlayer.increaseHealth(LiveScore::getHealthIncrease(
                                LiveScore::HIT::HIT_300G, HP, testPlayer.hpMultiplierNormal,
                                testPlayer.hpMultiplierComboEnd, 1.0));  // geki

                            if(testPlayer.health < lowestHpComboEnd) {
                                if(++comboTooLowCount > 2) {
                                    testPlayer.hpMultiplierComboEnd *= 1.07;
                                    testPlayer.hpMultiplierNormal *= 1.03;
                                    fail = true;
                                    break;
                                }
                            }
                        }

                        continue;
                    }

                    fail = true;
                    testDrop *= 0.96;
                    break;
                }

                fail = true;
                testDrop *= 0.96;
                break;
            }

            if(!fail && testPlayer.health < lowestHpEnd) {
                fail = true;
                testDrop *= 0.94;
                testPlayer.hpMultiplierComboEnd *= 1.01;
                testPlayer.hpMultiplierNormal *= 1.01;
            }

            const f64 recovery = (testPlayer.healthUncapped - testPlayer.hpBarMaximum) / (f64)this->hitobjects.size();
            if(!fail && recovery < HpRecoveryAvailable) {
                fail = true;
                testDrop *= 0.96;
                testPlayer.hpMultiplierComboEnd *= 1.02;
                testPlayer.hpMultiplierNormal *= 1.01;
            }
        } while(fail);

        this->fDrainRate =
            (testDrop / testPlayer.hpBarMaximum) * 1000.0;  // from [0, 200] to [0, 1], and from ms to seconds
        this->fHpMultiplierComboEnd = testPlayer.hpMultiplierComboEnd;
        this->fHpMultiplierNormal = testPlayer.hpMultiplierNormal;
    }
}

f32 SimulatedBeatmap::getApproachTime_full() const {
    return (ModMasks::eq(this->mods.flags, Replay::ModFlags::Mafham))
               ? this->getLength() * 2
               : GameRules::mapDifficultyRange(this->getAR(), GameRules::getMinApproachTime(),
                                               GameRules::getMidApproachTime(), GameRules::getMaxApproachTime());
}

f32 SimulatedBeatmap::getRawApproachTime_full() const {
    return (ModMasks::eq(this->mods.flags, Replay::ModFlags::Mafham))
               ? this->getLength() * 2
               : GameRules::mapDifficultyRange(this->getRawAR(), GameRules::getMinApproachTime(),
                                               GameRules::getMidApproachTime(), GameRules::getMaxApproachTime());
}