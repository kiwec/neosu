// Copyright (c) 2020, PG, All rights reserved.
#include "DatabaseBeatmap.h"

#include "SString.h"
#include "Bancho.h"  // md5
#include "BeatmapInterface.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"
#include "File.h"
#include "GameRules.h"
#include "HitObjects.h"
#include "Osu.h"
#include "Parsing.h"
#include "Skin.h"
#include "Logging.h"
#include "SongBrowser.h"
#include "AsyncIOHandler.h"

#include <cassert>
#include <algorithm>
#include <utility>
#include <source_location>

namespace {  // static namespace

bool timingPointSortComparator(DatabaseBeatmap::TIMINGPOINT const &a, DatabaseBeatmap::TIMINGPOINT const &b) {
    if(a.offset != b.offset) return a.offset < b.offset;

    // non-inherited timingpoints go before inherited timingpoints
    bool a_inherited = a.msPerBeat >= 0;
    bool b_inherited = b.msPerBeat >= 0;
    if(a_inherited != b_inherited) return a_inherited;

    if(a.sampleSet != b.sampleSet) return a.sampleSet < b.sampleSet;
    if(a.sampleIndex != b.sampleIndex) return a.sampleIndex < b.sampleIndex;
    if(a.kiai != b.kiai) return a.kiai;

    return false;  // equivalent
}
}  // namespace

DatabaseBeatmap::DatabaseBeatmap(std::string filePath, std::string folder, BeatmapType type) {
    this->sFilePath = std::move(filePath);
    this->sFolder = std::move(folder);
    this->type = type;

    // raw metadata (note the special default values)

    this->iVersion = cv::beatmap_version.getInt();
    this->iGameMode = 0;
    this->iID = 0;
    this->iSetID = -1;

    this->iLengthMS = 0;
    this->iPreviewTime = -1;

    this->fAR = 5.0f;
    this->fCS = 5.0f;
    this->fHP = 5.0f;
    this->fOD = 5.0f;

    this->fStackLeniency = 0.7f;
    this->fSliderTickRate = 1.0f;
    this->fSliderMultiplier = 1.0f;

    // precomputed data

    this->fStarsNomod = 0.0f;

    this->iMinBPM = 0;
    this->iMaxBPM = 0;
    this->iMostCommonBPM = 0;

    this->iNumObjects = 0;
    this->iNumCircles = 0;
    this->iNumSliders = 0;
    this->iNumSpinners = 0;

    // custom data
    this->iLocalOffset = 0;
    this->iOnlineOffset = 0;
}

DatabaseBeatmap::DatabaseBeatmap(std::vector<DatabaseBeatmap *> *difficulties, BeatmapType type)
    : DatabaseBeatmap("", "", type) {
    this->difficulties = difficulties;
    if(this->difficulties->empty()) return;

    // set representative values for this container (i.e. use values from first difficulty)
    this->sTitle = (*this->difficulties)[0]->sTitle;
    this->sTitleUnicode = (*this->difficulties)[0]->sTitleUnicode;
    this->bEmptyTitleUnicode = (*this->difficulties)[0]->bEmptyTitleUnicode;
    this->sArtist = (*this->difficulties)[0]->sArtist;
    this->sArtistUnicode = (*this->difficulties)[0]->sArtistUnicode;
    this->bEmptyArtistUnicode = (*this->difficulties)[0]->bEmptyArtistUnicode;
    this->sCreator = (*this->difficulties)[0]->sCreator;
    this->sBackgroundImageFileName = (*this->difficulties)[0]->sBackgroundImageFileName;
    this->iSetID = (*this->difficulties)[0]->iSetID;

    // also calculate largest representative values
    this->iLengthMS = 0;
    this->fCS = 99.f;
    this->fAR = 0.0f;
    this->fOD = 0.0f;
    this->fHP = 0.0f;
    this->fStarsNomod = 0.0f;
    this->iMinBPM = 9001;
    this->iMaxBPM = 0;
    this->iMostCommonBPM = 0;
    this->last_modification_time = 0;
    for(auto diff : (*this->difficulties)) {
        if(diff->getLengthMS() > this->iLengthMS) this->iLengthMS = diff->getLengthMS();
        if(diff->getCS() < this->fCS) this->fCS = diff->getCS();
        if(diff->getAR() > this->fAR) this->fAR = diff->getAR();
        if(diff->getHP() > this->fHP) this->fHP = diff->getHP();
        if(diff->getOD() > this->fOD) this->fOD = diff->getOD();
        if(diff->getStarsNomod() > this->fStarsNomod) this->fStarsNomod = diff->getStarsNomod();
        if(diff->getMinBPM() < this->iMinBPM) this->iMinBPM = diff->getMinBPM();
        if(diff->getMaxBPM() > this->iMaxBPM) this->iMaxBPM = diff->getMaxBPM();
        if(diff->getMostCommonBPM() > this->iMostCommonBPM) this->iMostCommonBPM = diff->getMostCommonBPM();
        if(diff->last_modification_time > this->last_modification_time)
            this->last_modification_time = diff->last_modification_time;
    }
}

DatabaseBeatmap::~DatabaseBeatmap() {
    if(this->difficulties != nullptr) {
        for(auto &diff : *this->difficulties) {
            assert(diff->difficulties == nullptr);
            SAFE_DELETE(diff);
        }
        SAFE_DELETE(this->difficulties);
    }
}

bool DatabaseBeatmap::parse_timing_point(const std::string &curLine, DatabaseBeatmap::TIMINGPOINT *out) {
    // old beatmaps: Offset, Milliseconds per Beat
    // old new beatmaps: Offset, Milliseconds per Beat, Meter, sampleSet, sampleIndex, Volume,
    // !Inherited new new beatmaps: Offset, Milliseconds per Beat, Meter, sampleSet, sampleIndex,
    // Volume, !Inherited, Kiai Mode

    f64 tpOffset;
    f32 tpMSPerBeat;
    i32 tpMeter;
    i32 tpSampleSet, tpSampleIndex;
    u8 tpVolume;
    i32 tpTimingChange;
    i32 tpKiai = 0;  // optional

    if(Parsing::parse(curLine, &tpOffset, ',', &tpMSPerBeat, ',', &tpMeter, ',', &tpSampleSet, ',', &tpSampleIndex, ',',
                      &tpVolume, ',', &tpTimingChange, ',', &tpKiai) ||
       Parsing::parse(curLine, &tpOffset, ',', &tpMSPerBeat, ',', &tpMeter, ',', &tpSampleSet, ',', &tpSampleIndex, ',',
                      &tpVolume, ',', &tpTimingChange)) {
        out->offset = std::round(tpOffset);
        out->msPerBeat = tpMSPerBeat;
        out->sampleSet = tpSampleSet;
        out->sampleIndex = tpSampleIndex;
        out->volume = std::clamp<u8>(tpVolume, 0, 100);
        out->timingChange = tpTimingChange == 1;
        out->kiai = tpKiai > 0;
        return true;
    }

    if(Parsing::parse(curLine, &tpOffset, ',', &tpMSPerBeat)) {
        out->offset = std::round(tpOffset);
        out->msPerBeat = tpMSPerBeat;
        out->sampleSet = 0;
        out->sampleIndex = 0;
        out->volume = 100;
        out->timingChange = true;
        out->kiai = false;
        return true;
    }

    return false;
}

DatabaseBeatmap::PRIMITIVE_CONTAINER DatabaseBeatmap::loadPrimitiveObjects(const std::string &osuFilePath) {
    std::atomic<bool> dead;
    dead = false;
    return loadPrimitiveObjects(osuFilePath, dead);
}

DatabaseBeatmap::PRIMITIVE_CONTAINER DatabaseBeatmap::loadPrimitiveObjects(const std::string &osuFilePath,
                                                                           const std::atomic<bool> &dead) {
    PRIMITIVE_CONTAINER c;
    {
        c.errorCode = 0;

        c.stackLeniency = 0.7f;

        c.sliderMultiplier = 1.0f;
        c.sliderTickRate = 1.0f;

        c.numCircles = 0;
        c.numSliders = 0;
        c.numSpinners = 0;
        c.numHitobjects = 0;

        c.version = 14;
    }

    const float sliderSanityRange = cv::slider_curve_max_length.getFloat();  // infinity sanity check, same as before
    const int sliderMaxRepeatRange =
        cv::slider_max_repeats.getInt();  // NOTE: osu! will refuse to play any beatmap which has sliders with more than
                                          // 9000 repeats, here we just clamp it instead

    // open osu file for parsing
    {
        File file(osuFilePath);
        if(!file.canRead()) {
            c.errorCode = 2;
            return c;
        }

        // std::istringstream ss("");

        // load the actual beatmap
        int hitobjectsWithoutSpinnerCounter = 0;
        int colorCounter = 1;
        int colorOffset = 0;
        int comboNumber = 1;
        int curBlock = -1;
        std::string curLine;
        while(file.canRead()) {
            if(dead.load()) {
                c.errorCode = 6;
                return c;
            }

            curLine = file.readLine();
            const char *curLineChar = curLine.c_str();

            // ignore comments, but only if at the beginning of a line (e.g. allow Artist:DJ'TEKINA//SOMETHING)
            if(curLine.starts_with("//")) continue;

            if(curLine.find("[General]") != std::string::npos)
                curBlock = 1;
            else if(curLine.find("[Difficulty]") != std::string::npos)
                curBlock = 2;
            else if(curLine.find("[Events]") != std::string::npos)
                curBlock = 3;
            else if(curLine.find("[TimingPoints]") != std::string::npos)
                curBlock = 4;
            else if(curLine.find("[Colours]") != std::string::npos)
                curBlock = 5;
            else if(curLine.find("[HitObjects]") != std::string::npos)
                curBlock = 6;

            switch(curBlock) {
                // header (e.g. "osu file format v12")
                case -1: {
                    Parsing::parse(curLineChar, "osu file format v", &c.version);
                    break;
                }

                // General
                case 1: {
                    std::string sampleSet;
                    if(Parsing::parse(curLineChar, "SampleSet", ':', &sampleSet)) {
                        SString::lower_inplace(sampleSet);
                        if(sampleSet == "normal") {
                            c.defaultSampleSet = SampleSetType::NORMAL;
                        } else if(sampleSet == "soft") {
                            c.defaultSampleSet = SampleSetType::SOFT;
                        } else if(sampleSet == "drum") {
                            c.defaultSampleSet = SampleSetType::DRUM;
                        }
                    }

                    Parsing::parse(curLineChar, "StackLeniency", ':', &c.stackLeniency);
                    break;
                }

                // Difficulty
                case 2: {
                    Parsing::parse(curLineChar, "SliderMultiplier", ':', &c.sliderMultiplier);
                    Parsing::parse(curLineChar, "SliderTickRate", ':', &c.sliderTickRate);
                    break;
                }

                // Events
                case 3: {
                    i64 type, startTime, endTime;
                    if(Parsing::parse(curLineChar, &type, ',', &startTime, ',', &endTime)) {
                        if(type == 2) {
                            BREAK b{.startTime = startTime, .endTime = endTime};
                            c.breaks.push_back(b);
                        }
                    }
                    break;
                }

                // TimingPoints
                case 4: {
                    DatabaseBeatmap::TIMINGPOINT t{};
                    if(parse_timing_point(curLine, &t)) {
                        c.timingpoints.push_back(t);
                    }
                    break;
                }

                // Colours
                case 5: {
                    u8 comboNum;
                    u8 r, g, b;

                    // XXX: this assumes combo colors are defined in the proper order
                    // FIXME: actually use comboNum for ordering
                    if(Parsing::parse(curLineChar, "Combo", &comboNum, ':', &r, ',', &g, ',', &b)) {
                        if(comboNum >= 1 && comboNum <= 8) {  // bare minimum validation effort
                            c.combocolors.push_back(rgb(r, g, b));
                        }
                    }

                    break;
                }

                // HitObjects
                case 6: {
                    size_t err_line = 0;

                    auto upd_last_error = [&err_line](bool parse_result_bad,
                                                      size_t line = std::source_location::current().line()) -> void {
                        if(err_line) {  // already got error
                            return;
                        } else {
                            if(parse_result_bad) {
                                err_line = line;
                            }
                        }
                    };

                    // circles:
                    // x,y,time,type,hitSounds,hitSamples
                    // sliders:
                    // x,y,time,type,hitSounds,sliderType|curveX:curveY|...,repeat,pixelLength,edgeHitsound,edgeSets,hitSamples
                    // spinners:
                    // x,y,time,type,hitSounds,endTime,hitSamples

                    // hitSamples are colon-separated optional components (up to 5), and not all 5 have to be specified
                    static auto parse_hitsamples = [](const std::string &hitSampleStr, HitSamples &samples) -> bool {
                        samples.normalSet = 0;
                        samples.additionSet = 0;
                        samples.index = 0;
                        samples.volume = 0;
                        samples.filename = "";

                        if(hitSampleStr.empty()) return true;

                        auto parts = SString::split(hitSampleStr, ":");

                        // Parse available components, using defaults for missing ones
                        if(parts.size() >= 1 && !parts[0].empty()) {
                            if(!Parsing::parse(parts[0], &samples.normalSet)) return false;
                        }
                        if(parts.size() >= 2 && !parts[1].empty()) {
                            if(!Parsing::parse(parts[1], &samples.additionSet)) return false;
                        }
                        if(parts.size() >= 3 && !parts[2].empty()) {
                            if(!Parsing::parse(parts[2], &samples.index)) return false;
                        }
                        if(parts.size() >= 4 && !parts[3].empty()) {
                            if(!Parsing::parse(parts[3], &samples.volume)) return false;
                        }
                        if(parts.size() >= 5) {
                            samples.filename = parts[4];  // filename can be empty
                        }

                        samples.volume = std::clamp<u8>(samples.volume, 0, 100);
                        return true;
                    };

                    // NOTE: calculating combo numbers and color offsets based on the parsing order is dangerous.
                    // maybe the hitobjects are not sorted by time in the file; these values should be calculated
                    // after sorting just to be sure?

                    f32 x, y;
                    u32 time;
                    u8 type;
                    i32 hitSounds;

                    auto csvs = SString::split(curLine, ",");
                    if(csvs.size() < 5) break;
                    upd_last_error(!Parsing::parse(csvs[0], &x) || !std::isfinite(x) || std::isnan(x));
                    upd_last_error(!Parsing::parse(csvs[1], &y) || !std::isfinite(y) || std::isnan(y));
                    upd_last_error(!Parsing::parse(csvs[2], &time));
                    upd_last_error(!Parsing::parse(csvs[3], &type));
                    upd_last_error(!Parsing::parse(csvs[4], &hitSounds));
                    upd_last_error((type & PpyHitObjectType::SLIDER) && (csvs.size() < 8));
                    upd_last_error((type & PpyHitObjectType::SPINNER) && (csvs.size() < 6));
                    upd_last_error(
                        !(type & (PpyHitObjectType::CIRCLE | PpyHitObjectType::SLIDER | PpyHitObjectType::SPINNER)));
                    if(err_line) {
                        debugLog("Invalid hit object (error on line {}): {}", err_line, curLine);
                        break;
                    }

                    if(!(type & PpyHitObjectType::SPINNER)) hitobjectsWithoutSpinnerCounter++;

                    if(type & PpyHitObjectType::NEW_COMBO) {
                        comboNumber = 1;

                        // special case 1: if the current object is a spinner, then the raw color counter is not
                        // increased (but the offset still is!)
                        // special case 2: the first (non-spinner) hitobject in a beatmap is always a new combo,
                        // therefore the raw color counter is not increased for it (but the offset still is!)
                        if(!(type & PpyHitObjectType::SPINNER) && hitobjectsWithoutSpinnerCounter > 1) colorCounter++;

                        // special case 3: "Bits 4-6 (16, 32, 64) form a 3-bit number (0-7) that chooses how many combo colours to skip."
                        colorOffset += (type >> 4) & 0b111;
                    }

                    if(type & PpyHitObjectType::CIRCLE) {
                        HITCIRCLE h{};
                        h.x = x;
                        h.y = y;
                        h.time = time;
                        h.number = comboNumber++;
                        h.colorCounter = colorCounter;
                        h.colorOffset = colorOffset;
                        h.clicked = false;
                        h.samples.hitSounds = (hitSounds & HitSoundType::VALID_HITSOUNDS);

                        if(csvs.size() > 5) {
                            if(!parse_hitsamples(csvs[5], h.samples)) {
                                debugLog("Invalid circle hitSamples in line: {}", curLine);
                                break;
                            }
                        }

                        c.hitcircles.push_back(h);
                    } else if(type & PpyHitObjectType::SLIDER) {
                        SLIDER slider{};
                        slider.colorCounter = colorCounter;
                        slider.colorOffset = colorOffset;
                        slider.time = time;
                        slider.hoverSamples.hitSounds = (hitSounds & HitSoundType::VALID_SLIDER_HITSOUNDS);

                        auto curves = SString::split(csvs[5], "|");
                        slider.type = curves[0].c_str()[0];
                        curves.erase(curves.begin());
                        for(auto &curvePoints : curves) {
                            f32 cpX{}, cpY{};
                            upd_last_error(!Parsing::parse(curvePoints, &cpX, ':', &cpY));
                            upd_last_error(!std::isfinite(cpX) || std::isnan(cpX));
                            upd_last_error(!std::isfinite(cpY) || std::isnan(cpY));
                            if(err_line) break;

                            slider.points.emplace_back(std::clamp(cpX, -sliderSanityRange, sliderSanityRange),
                                                       std::clamp(cpY, -sliderSanityRange, sliderSanityRange));
                        }

                        if(err_line) {
                            debugLog("Invalid slider (error on line {}): {}", err_line, curLine);
                            break;
                        }

                        upd_last_error(!Parsing::parse(csvs[6], &slider.repeat));
                        upd_last_error(!Parsing::parse(csvs[7], &slider.pixelLength));

                        // special case: osu! logic for handling the hitobject point vs the controlpoints (since
                        // sliders have both, and older beatmaps store the start point inside the control
                        // points)
                        vec2 xy = vec2(std::clamp(x, -sliderSanityRange, sliderSanityRange),
                                       std::clamp(y, -sliderSanityRange, sliderSanityRange));
                        if(slider.points.size() > 0) {
                            if(slider.points[0] != xy) slider.points.insert(slider.points.begin(), xy);
                        } else {
                            slider.points.push_back(xy);
                        }

                        // partially allow bullshit sliders (add second point to make valid)
                        // e.g. https://osu.ppy.sh/beatmapsets/791900#osu/1676490
                        if(slider.points.size() == 1) slider.points.push_back(xy);

                        std::vector<std::string> edgeSounds;
                        if(csvs.size() > 8) edgeSounds = SString::split(csvs[8], "|");

                        std::vector<std::string> edgeSets;
                        if(csvs.size() > 9) edgeSets = SString::split(csvs[9], "|");
                        upd_last_error((!edgeSets.empty() && (edgeSounds.size() != edgeSets.size())));

                        for(i32 i = 0; !err_line && i < edgeSounds.size(); i++) {
                            HitSamples samples;
                            upd_last_error(!Parsing::parse(edgeSounds[i], &samples.hitSounds));
                            samples.hitSounds &= HitSoundType::VALID_HITSOUNDS;

                            if(!edgeSets.empty()) {
                                upd_last_error(
                                    !Parsing::parse(edgeSets[i], &samples.normalSet, ':', &samples.additionSet));
                            }

                            slider.edgeSamples.push_back(samples);
                        }

                        if(err_line) {
                            debugLog("Invalid slider edgeSamples (error on line {}): {}", err_line, curLine);
                            break;
                        }

                        // No start sample specified, use default
                        if(slider.edgeSamples.empty()) slider.edgeSamples.emplace_back();

                        // No end sample specified, use the same as start
                        if(slider.edgeSamples.size() == 1) slider.edgeSamples.push_back(slider.edgeSamples.front());

                        if(csvs.size() > 10) {
                            if(!parse_hitsamples(csvs[10], slider.hoverSamples)) {
                                debugLog("Invalid slider hitSamples in line: {}", curLine);
                                break;
                            }
                        }

                        slider.x = xy.x;
                        slider.y = xy.y;
                        slider.repeat = std::clamp(slider.repeat, 0, sliderMaxRepeatRange);
                        slider.pixelLength = std::clamp(slider.pixelLength, -sliderSanityRange, sliderSanityRange);
                        slider.number = comboNumber++;
                        c.sliders.push_back(slider);
                    } else if(type & PpyHitObjectType::SPINNER) {
                        SPINNER s{.x = (i32)x,
                                  .y = (i32)y,
                                  .time = time,
                                  .endTime = 0,
                                  .samples = {.hitSounds = (u8)(hitSounds & HitSoundType::VALID_HITSOUNDS)}};

                        upd_last_error(!Parsing::parse(csvs[5], &s.endTime));

                        if(err_line) {
                            debugLog("Invalid spinner (error on line {}): {}", err_line, curLine);
                            break;
                        }

                        if(csvs.size() > 6) {
                            if(!parse_hitsamples(csvs[6], s.samples)) {
                                debugLog("Invalid spinner hitSamples in line: {}", curLine);
                                break;
                            }
                        }

                        c.spinners.push_back(s);
                    }

                    break;
                }
            }
        }
    }

    // late bail if too many hitobjects would run out of memory and crash
    c.numCircles = c.hitcircles.size();
    c.numSliders = c.sliders.size();
    c.numSpinners = c.spinners.size();
    c.numHitobjects = c.numCircles + c.numSliders + c.numSpinners;
    if(c.numHitobjects > (size_t)cv::beatmap_max_num_hitobjects.getInt()) {
        c.errorCode = 5;
        return c;
    }

    // sort timingpoints by time
    if(c.timingpoints.size() > 1) std::ranges::sort(c.timingpoints, timingPointSortComparator);

    return c;
}

DatabaseBeatmap::CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT DatabaseBeatmap::calculateSliderTimesClicksTicks(
    int beatmapVersion, std::vector<SLIDER> &sliders, zarray<DatabaseBeatmap::TIMINGPOINT> &timingpoints,
    float sliderMultiplier, float sliderTickRate) {
    std::atomic<bool> dead;
    dead = false;
    return calculateSliderTimesClicksTicks(beatmapVersion, sliders, timingpoints, sliderMultiplier, sliderTickRate,
                                           false);
}

DatabaseBeatmap::CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT DatabaseBeatmap::calculateSliderTimesClicksTicks(
    int beatmapVersion, std::vector<SLIDER> &sliders, zarray<DatabaseBeatmap::TIMINGPOINT> &timingpoints,
    float sliderMultiplier, float sliderTickRate, const std::atomic<bool> &dead) {
    CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT r;
    {
        r.errorCode = 0;
    }

    if(timingpoints.size() < 1) {
        r.errorCode = 3;
        return r;
    }

    struct SliderHelper {
        static float getSliderTickDistance(float sliderMultiplier, float sliderTickRate) {
            return ((100.0f * sliderMultiplier) / sliderTickRate);
        }

        static float getSliderTimeForSlider(const SLIDER &slider, const TIMING_INFO &timingInfo,
                                            float sliderMultiplier) {
            const float duration = timingInfo.beatLength * (slider.pixelLength / sliderMultiplier) / 100.0f;
            return duration >= 1.0f ? duration : 1.0f;  // sanity check
        }

        static float getSliderVelocity(const TIMING_INFO &timingInfo, float sliderMultiplier, float sliderTickRate) {
            const float beatLength = timingInfo.beatLength;
            if(beatLength > 0.0f)
                return (getSliderTickDistance(sliderMultiplier, sliderTickRate) * sliderTickRate *
                        (1000.0f / beatLength));
            else
                return getSliderTickDistance(sliderMultiplier, sliderTickRate) * sliderTickRate;
        }

        static float getTimingPointMultiplierForSlider(const TIMING_INFO &timingInfo)  // needed for slider ticks
        {
            float beatLengthBase = timingInfo.beatLengthBase;
            if(beatLengthBase == 0.0f)  // sanity check
                beatLengthBase = 1.0f;

            return timingInfo.beatLength / beatLengthBase;
        }
    };

    for(auto &s : sliders) {
        if(dead.load()) {
            r.errorCode = 6;
            return r;
        }

        // sanity reset
        s.ticks.clear();
        s.scoringTimesForStarCalc.clear();

        // calculate duration
        const TIMING_INFO timingInfo = getTimingInfoForTimeAndTimingPoints(s.time, timingpoints);
        s.sliderTimeWithoutRepeats = SliderHelper::getSliderTimeForSlider(s, timingInfo, sliderMultiplier);
        s.sliderTime = s.sliderTimeWithoutRepeats * s.repeat;

        // calculate ticks
        {
            const float minTickPixelDistanceFromEnd =
                0.01f * SliderHelper::getSliderVelocity(timingInfo, sliderMultiplier, sliderTickRate);
            const float tickPixelLength =
                (beatmapVersion < 8 ? SliderHelper::getSliderTickDistance(sliderMultiplier, sliderTickRate)
                                    : SliderHelper::getSliderTickDistance(sliderMultiplier, sliderTickRate) /
                                          SliderHelper::getTimingPointMultiplierForSlider(timingInfo));
            const float tickDurationPercentOfSliderLength =
                tickPixelLength / (s.pixelLength == 0.0f ? 1.0f : s.pixelLength);
            const int max_ticks = cv::slider_max_ticks.getInt();
            const int tickCount = std::min((int)std::ceil(s.pixelLength / tickPixelLength) - 1,
                                           max_ticks);  // NOTE: hard sanity limit number of ticks per slider

            if(tickCount > 0 && !timingInfo.isNaN && !std::isnan(s.pixelLength) &&
               !std::isnan(tickPixelLength))  // don't generate ticks for NaN timingpoints and infinite values
            {
                const float tickTOffset = tickDurationPercentOfSliderLength;
                float pixelDistanceToEnd = s.pixelLength;
                float t = tickTOffset;
                for(int i = 0; i < tickCount; i++, t += tickTOffset) {
                    // skip ticks which are too close to the end of the slider
                    pixelDistanceToEnd -= tickPixelLength;
                    if(pixelDistanceToEnd <= minTickPixelDistanceFromEnd) break;

                    s.ticks.push_back(t);
                }
            }
        }

        // bail if too many predicted heuristic scoringTimes would run out of memory and crash
        if((size_t)std::abs(s.repeat) * s.ticks.size() > (size_t)cv::beatmap_max_num_slider_scoringtimes.getInt()) {
            r.errorCode = 5;
            return r;
        }

        // calculate s.scoringTimesForStarCalc, which should include every point in time where the cursor must be within
        // the followcircle radius and at least one key must be pressed: see
        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs
        const i32 osuSliderEndInsideCheckOffset = (i32)cv::slider_end_inside_check_offset.getInt();

        // 1) "skip the head circle"

        // 2) add repeat times (either at slider begin or end)
        for(int i = 0; i < (s.repeat - 1); i++) {
            const f32 time = s.time + (s.sliderTimeWithoutRepeats * (i + 1));  // see Slider.cpp
            s.scoringTimesForStarCalc.push_back(OsuDifficultyHitObject::SLIDER_SCORING_TIME{
                .type = OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::REPEAT,
                .time = time,
            });
        }

        // 3) add tick times (somewhere within slider, repeated for every repeat)
        for(int i = 0; i < s.repeat; i++) {
            for(int t = 0; t < s.ticks.size(); t++) {
                const float tickPercentRelativeToRepeatFromStartAbs =
                    (((i + 1) % 2) != 0 ? s.ticks[t] : 1.0f - s.ticks[t]);  // see Slider.cpp
                const f32 time =
                    s.time + (s.sliderTimeWithoutRepeats * i) +
                    (tickPercentRelativeToRepeatFromStartAbs * s.sliderTimeWithoutRepeats);  // see Slider.cpp
                s.scoringTimesForStarCalc.push_back(OsuDifficultyHitObject::SLIDER_SCORING_TIME{
                    .type = OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::TICK,
                    .time = time,
                });
            }
        }

        // 4) add slider end (potentially before last tick for bullshit sliders, but sorting takes care of that)
        // see https://github.com/ppy/osu/pull/4193#issuecomment-460127543
        const f32 time =
            std::max(static_cast<f32>(s.time) + s.sliderTime / 2.0f,
                     (static_cast<f32>(s.time) + s.sliderTime) - static_cast<f32>(osuSliderEndInsideCheckOffset));
        s.scoringTimesForStarCalc.push_back(OsuDifficultyHitObject::SLIDER_SCORING_TIME{
            .type = OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::END,
            .time = time,
        });

        if(dead.load()) {
            r.errorCode = 6;
            return r;
        }

        constexpr auto sliderScoringTimeComparator = [](const OsuDifficultyHitObject::SLIDER_SCORING_TIME &a,
                                                        const OsuDifficultyHitObject::SLIDER_SCORING_TIME &b) -> bool {
            if(a.time != b.time) return a.time < b.time;
            if(a.type != b.type) return static_cast<int>(a.type) < static_cast<int>(b.type);
            return false;  // equivalent
        };

        // 5) sort scoringTimes from earliest to latest
        if(s.scoringTimesForStarCalc.size() > 1) {
            std::ranges::sort(s.scoringTimesForStarCalc, sliderScoringTimeComparator);
        }
    }

    return r;
}

DatabaseBeatmap::LOAD_DIFFOBJ_RESULT DatabaseBeatmap::loadDifficultyHitObjects(const std::string &osuFilePath, float AR,
                                                                               float CS, float speedMultiplier,
                                                                               bool calculateStarsInaccurately) {
    std::atomic<bool> dead;
    dead = false;
    return loadDifficultyHitObjects(osuFilePath, AR, CS, speedMultiplier, calculateStarsInaccurately, dead);
}

DatabaseBeatmap::LOAD_DIFFOBJ_RESULT DatabaseBeatmap::loadDifficultyHitObjects(const std::string &osuFilePath, float AR,
                                                                               float CS, float speedMultiplier,
                                                                               bool calculateStarsInaccurately,
                                                                               const std::atomic<bool> &dead) {
    // load primitive arrays
    PRIMITIVE_CONTAINER c = loadPrimitiveObjects(osuFilePath, dead);
    return loadDifficultyHitObjects(c, AR, CS, speedMultiplier, calculateStarsInaccurately, dead);
}

DatabaseBeatmap::LOAD_DIFFOBJ_RESULT DatabaseBeatmap::loadDifficultyHitObjects(PRIMITIVE_CONTAINER &c, float AR,
                                                                               float CS, float speedMultiplier,
                                                                               bool calculateStarsInaccurately,
                                                                               const std::atomic<bool> &dead) {
    LOAD_DIFFOBJ_RESULT result{};

    // build generalized OsuDifficultyHitObjects from the vectors (hitcircles, sliders, spinners)
    // the OsuDifficultyHitObject class is the one getting used in all pp/star calculations, it encompasses every object
    // type for simplicity

    if(c.errorCode != 0) {
        result.errorCode = c.errorCode;
        return result;
    }

    // calculate sliderTimes, and build slider clicks and ticks
    CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT sliderTimeCalcResult = calculateSliderTimesClicksTicks(
        c.version, c.sliders, c.timingpoints, c.sliderMultiplier, c.sliderTickRate, dead);
    if(sliderTimeCalcResult.errorCode != 0) {
        result.errorCode = sliderTimeCalcResult.errorCode;
        return result;
    }

    // now we can calculate the max possible combo (because that needs ticks/clicks to be filled, mostly convenience)
    {
        result.maxPossibleCombo += c.hitcircles.size();
        for(const auto &s : c.sliders) {
            const int repeats = std::max((s.repeat - 1), 0);
            result.maxPossibleCombo +=
                2 + repeats + (repeats + 1) * s.ticks.size();  // start/end + repeat arrow + ticks
        }
        result.maxPossibleCombo += c.spinners.size();
    }

    // and generate the difficultyhitobjects
    result.diffobjects.reserve(c.hitcircles.size() + c.sliders.size() + c.spinners.size());

    for(auto &hitcircle : c.hitcircles) {
        result.diffobjects.emplace_back(OsuDifficultyHitObject::TYPE::CIRCLE, vec2(hitcircle.x, hitcircle.y),
                                        (i32)hitcircle.time);
    }

    const bool calculateSliderCurveInConstructor =
        (c.sliders.size() < 5000);  // NOTE: for explanation see OsuDifficultyHitObject constructor
    for(auto &slider : c.sliders) {
        if(dead.load()) {
            result.errorCode = 6;
            return result;
        }

        if(!calculateStarsInaccurately) {
            result.diffobjects.emplace_back(
                OsuDifficultyHitObject::TYPE::SLIDER, vec2(slider.x, slider.y), slider.time,
                slider.time + (i32)slider.sliderTime, slider.sliderTimeWithoutRepeats, slider.type, slider.points,
                slider.pixelLength, slider.scoringTimesForStarCalc, slider.repeat, calculateSliderCurveInConstructor);
        } else {
            result.diffobjects.emplace_back(
                OsuDifficultyHitObject::TYPE::SLIDER, vec2(slider.x, slider.y), slider.time,
                slider.time + (i32)slider.sliderTime, slider.sliderTimeWithoutRepeats, slider.type,
                std::vector<vec2>(),  // NOTE: ignore curve when calculating inaccurately
                slider.pixelLength,
                std::vector<OsuDifficultyHitObject::SLIDER_SCORING_TIME>(),  // NOTE: ignore curve when calculating
                                                                             // inaccurately
                slider.repeat,
                false);  // NOTE: ignore curve when calculating inaccurately
        }
    }

    for(auto &spinner : c.spinners) {
        result.diffobjects.emplace_back(OsuDifficultyHitObject::TYPE::SPINNER, vec2(spinner.x, spinner.y),
                                        (i32)spinner.time, (i32)spinner.endTime);
    }

    if(dead.load()) {
        result.errorCode = 6;
        return result;
    }

    // sort hitobjects by time
    constexpr auto diffHitObjectSortComparator = [](const OsuDifficultyHitObject &a,
                                                    const OsuDifficultyHitObject &b) -> bool {
        if(a.time != b.time) return a.time < b.time;
        if(a.type != b.type) return static_cast<int>(a.type) < static_cast<int>(b.type);
        if(a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
        if(a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
        return false;  // equivalent
    };

    if(result.diffobjects.size() > 1) {
        std::ranges::sort(result.diffobjects, diffHitObjectSortComparator);
    }

    if(dead.load()) {
        result.errorCode = 6;
        return result;
    }

    // calculate stacks
    // see Beatmap.cpp
    // NOTE: this must be done before the speed multiplier is applied!
    // HACKHACK: code duplication ffs
    if(cv::stars_stacking.getBool() &&
       !calculateStarsInaccurately)  // NOTE: ignore stacking when calculating inaccurately
    {
        const float finalAR = AR;
        const float finalCS = CS;
        const float rawHitCircleDiameter = GameRules::getRawHitCircleDiameter(finalCS);

        const float STACK_LENIENCE = 3.0f;

        const float approachTime = GameRules::getApproachTimeForStacking(finalAR);

        if(c.version > 5) {
            // peppy's algorithm
            // https://gist.github.com/peppy/1167470

            for(int i = result.diffobjects.size() - 1; i >= 0; i--) {
                int n = i;

                OsuDifficultyHitObject *objectI = &result.diffobjects[i];

                const bool isSpinner = (objectI->type == OsuDifficultyHitObject::TYPE::SPINNER);

                if(objectI->stack != 0 || isSpinner) continue;

                const bool isHitCircle = (objectI->type == OsuDifficultyHitObject::TYPE::CIRCLE);
                const bool isSlider = (objectI->type == OsuDifficultyHitObject::TYPE::SLIDER);

                if(isHitCircle) {
                    while(--n >= 0) {
                        OsuDifficultyHitObject *objectN = &result.diffobjects[n];

                        const bool isSpinnerN = (objectN->type == OsuDifficultyHitObject::TYPE::SPINNER);

                        if(isSpinnerN) continue;

                        if(objectI->time - (approachTime * c.stackLeniency) > (objectN->endTime)) break;

                        vec2 objectNEndPosition = objectN->getOriginalRawPosAt(objectN->time + objectN->getDuration());
                        if(objectN->getDuration() != 0 &&
                           vec::length(objectNEndPosition - objectI->getOriginalRawPosAt(objectI->time)) <
                               STACK_LENIENCE) {
                            int offset = objectI->stack - objectN->stack + 1;
                            for(int j = n + 1; j <= i; j++) {
                                if(vec::length(objectNEndPosition - result.diffobjects[j].getOriginalRawPosAt(
                                                                        result.diffobjects[j].time)) < STACK_LENIENCE)
                                    result.diffobjects[j].stack = (result.diffobjects[j].stack - offset);
                            }

                            break;
                        }

                        if(vec::length(objectN->getOriginalRawPosAt(objectN->time) -
                                       objectI->getOriginalRawPosAt(objectI->time)) < STACK_LENIENCE) {
                            objectN->stack = (objectI->stack + 1);
                            objectI = objectN;
                        }
                    }
                } else if(isSlider) {
                    while(--n >= 0) {
                        OsuDifficultyHitObject *objectN = &result.diffobjects[n];

                        const bool isSpinner = (objectN->type == OsuDifficultyHitObject::TYPE::SPINNER);

                        if(isSpinner) continue;

                        if(objectI->time - (approachTime * c.stackLeniency) > objectN->time) break;

                        if(vec::length((objectN->getDuration() != 0
                                            ? objectN->getOriginalRawPosAt(objectN->time + objectN->getDuration())
                                            : objectN->getOriginalRawPosAt(objectN->time)) -
                                       objectI->getOriginalRawPosAt(objectI->time)) < STACK_LENIENCE) {
                            objectN->stack = (objectI->stack + 1);
                            objectI = objectN;
                        }
                    }
                }
            }
        } else  // version < 6
        {
            // old stacking algorithm for old beatmaps
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Beatmaps/OsuBeatmapProcessor.cs

            for(int i = 0; i < result.diffobjects.size(); i++) {
                OsuDifficultyHitObject *currHitObject = &result.diffobjects[i];

                const bool isSlider = (currHitObject->type == OsuDifficultyHitObject::TYPE::SLIDER);

                if(currHitObject->stack != 0 && !isSlider) continue;

                i32 startTime = currHitObject->time + currHitObject->getDuration();
                int sliderStack = 0;

                for(int j = i + 1; j < result.diffobjects.size(); j++) {
                    OsuDifficultyHitObject *objectJ = &result.diffobjects[j];

                    if(objectJ->time - (approachTime * c.stackLeniency) > startTime) break;

                    // "The start position of the hitobject, or the position at the end of the path if the hitobject is
                    // a slider"
                    vec2 position2 =
                        isSlider
                            ? currHitObject->getOriginalRawPosAt(currHitObject->time + currHitObject->getDuration())
                            : currHitObject->getOriginalRawPosAt(currHitObject->time);

                    if(vec::length(objectJ->getOriginalRawPosAt(objectJ->time) -
                                   currHitObject->getOriginalRawPosAt(currHitObject->time)) < 3) {
                        currHitObject->stack++;
                        startTime = objectJ->time + objectJ->getDuration();
                    } else if(vec::length(objectJ->getOriginalRawPosAt(objectJ->time) - position2) < 3) {
                        // "Case for sliders - bump notes down and right, rather than up and left."
                        sliderStack++;
                        objectJ->stack -= sliderStack;
                        startTime = objectJ->time + objectJ->getDuration();
                    }
                }
            }
        }

        // update hitobject positions
        float stackOffset = rawHitCircleDiameter / 128.0f / GameRules::broken_gamefield_rounding_allowance * 6.4f;
        for(int i = 0; i < result.diffobjects.size(); i++) {
            if(dead.load()) {
                result.errorCode = 6;
                return result;
            }

            if(result.diffobjects[i].curve && result.diffobjects[i].stack != 0)
                result.diffobjects[i].updateStackPosition(stackOffset);
        }
    }

    // apply speed multiplier (if present)
    if(speedMultiplier != 1.0f && speedMultiplier > 0.0f) {
        const double invSpeedMultiplier = 1.0 / (double)speedMultiplier;
        for(int i = 0; i < result.diffobjects.size(); i++) {
            if(dead.load()) {
                result.errorCode = 6;
                return result;
            }

            result.diffobjects[i].time = (i32)((double)result.diffobjects[i].time * invSpeedMultiplier);
            result.diffobjects[i].endTime = (i32)((double)result.diffobjects[i].endTime * invSpeedMultiplier);

            if(!calculateStarsInaccurately)  // NOTE: ignore slider curves when calculating inaccurately
            {
                result.diffobjects[i].spanDuration = (double)result.diffobjects[i].spanDuration * invSpeedMultiplier;
                for(auto &scoringTime : result.diffobjects[i].scoringTimes) {
                    scoringTime.time = ((f64)scoringTime.time * invSpeedMultiplier);
                }
            }
        }
    }

    return result;
}

bool DatabaseBeatmap::getMapFileAsync(MapFileReadDoneCallback data_callback) {
    // don't want to include AsyncIOHandler.h in DatabaseBeatmap.h
    static_assert(std::is_same_v<MapFileReadDoneCallback, AsyncIOHandler::ReadCallback>);
    if(!Environment::fileExists(this->sFilePath)) return false;
    return io->read(this->sFilePath, std::move(data_callback));
}

// XXX: code duplication (see loadPrimitiveObjects)
bool DatabaseBeatmap::loadMetadata(bool compute_md5) {
    if(this->difficulties != nullptr) return false;  // we are a beatmapset, not a difficulty

    // reset
    this->timingpoints.clear();

    if(cv::debug_osu.getBool()) debugLog("DatabaseBeatmap::loadMetadata() : {:s}", this->sFilePath.c_str());

    std::vector<u8> fileBuffer;
    u8 *beatmapFile{nullptr};
    size_t beatmapFileSize{0};

    {
        File file(this->sFilePath);
        if(file.canRead()) {
            beatmapFileSize = file.getFileSize();
            fileBuffer = file.takeFileBuffer();
            beatmapFile = fileBuffer.data();
        }
        // close the file here
    }

    if(fileBuffer.empty()) {
        debugLog("Osu Error: Couldn't read file {:s}", this->sFilePath.c_str());
        return false;
    }

    // compute MD5 hash (very slow)
    if(compute_md5) {
        this->sMD5Hash = {BanchoState::md5(beatmapFile, beatmapFileSize)};
    }

    // load metadata
    bool foundAR = false;
    int curBlock = -1;
    std::string curLine;

    const u8 *start = beatmapFile;
    const u8 *end = beatmapFile + beatmapFileSize;
    while(start < end) {
        const u8 *lineEnd = (u8 *)memchr(start, '\n', end - start);
        if(!lineEnd) lineEnd = end;

        std::string curLine((const char *)start, lineEnd - start);
        start = lineEnd + 1;

        // ignore comments, but only if at the beginning of a line (e.g. allow Artist:DJ'TEKINA//SOMETHING)
        if(curLine.starts_with("//")) continue;

        const char *curLineChar = curLine.c_str();
        if(curLine.find("[General]") != std::string::npos)
            curBlock = 0;
        else if(curLine.find("[Metadata]") != std::string::npos)
            curBlock = 1;
        else if(curLine.find("[Difficulty]") != std::string::npos)
            curBlock = 2;
        else if(curLine.find("[Events]") != std::string::npos)
            curBlock = 3;
        else if(curLine.find("[TimingPoints]") != std::string::npos)
            curBlock = 4;
        else if(curLine.find("[HitObjects]") != std::string::npos)
            break;  // NOTE: stop early

        switch(curBlock) {
            // header (e.g. "osu file format v12")
            case -1: {
                if(Parsing::parse(curLineChar, "osu file format v", &this->iVersion)) {
                    if(this->iVersion > cv::beatmap_version.getInt()) {
                        debugLog("Ignoring unknown/invalid beatmap version {:d}", this->iVersion);
                        return false;
                    }
                }
                break;
            }

            // General
            case 0: {
                Parsing::parse(curLineChar, "AudioFilename", ':', &this->sAudioFileName);
                Parsing::parse(curLineChar, "StackLeniency", ':', &this->fStackLeniency);
                Parsing::parse(curLineChar, "PreviewTime", ':', &this->iPreviewTime);
                Parsing::parse(curLineChar, "Mode", ':', &this->iGameMode);
                break;
            }

            // Metadata
            case 1: {
                Parsing::parse(curLineChar, "Title", ':', &this->sTitle);
                Parsing::parse(curLineChar, "TitleUnicode", ':', &this->sTitleUnicode);
                Parsing::parse(curLineChar, "Artist", ':', &this->sArtist);
                Parsing::parse(curLineChar, "ArtistUnicode", ':', &this->sArtistUnicode);
                Parsing::parse(curLineChar, "Creator", ':', &this->sCreator);
                Parsing::parse(curLineChar, "Version", ':', &this->sDifficultyName);
                Parsing::parse(curLineChar, "Source", ':', &this->sSource);
                Parsing::parse(curLineChar, "Tags", ':', &this->sTags);
                Parsing::parse(curLineChar, "BeatmapID", ':', &this->iID);
                Parsing::parse(curLineChar, "BeatmapSetID", ':', &this->iSetID);
                break;
            }

            // Difficulty
            case 2: {
                Parsing::parse(curLineChar, "CircleSize", ':', &this->fCS);
                if(Parsing::parse(curLineChar, "ApproachRate", ':', &this->fAR)) foundAR = true;
                Parsing::parse(curLineChar, "HPDrainRate", ':', &this->fHP);
                Parsing::parse(curLineChar, "OverallDifficulty", ':', &this->fOD);
                Parsing::parse(curLineChar, "SliderMultiplier", ':', &this->fSliderMultiplier);
                Parsing::parse(curLineChar, "SliderTickRate", ':', &this->fSliderTickRate);
                break;
            }

            // Events
            case 3: {
                std::string str;
                i32 type, startTime;
                if(Parsing::parse(curLineChar, &type, ',', &startTime, ',', &str)) {
                    if(type == 0) {
                        this->sBackgroundImageFileName = str;
                        this->sFullBackgroundImageFilePath = this->sFolder;
                        this->sFullBackgroundImageFilePath.append(this->sBackgroundImageFileName);
                    }
                }

                break;
            }

            // TimingPoints
            case 4: {
                DatabaseBeatmap::TIMINGPOINT t{};
                if(parse_timing_point(curLine, &t)) {
                    this->timingpoints.push_back(t);
                }
                break;
            }
        }
    }

    // im not actually sure if we need to do this again here
    if(SString::is_wspace_only(this->sTitleUnicode)) {
        this->bEmptyTitleUnicode = true;
    }
    if(SString::is_wspace_only(this->sArtistUnicode)) {
        this->bEmptyArtistUnicode = true;
    }

    // gamemode filter
    if(this->iGameMode != 0) return false;  // nothing more to do here

    // general sanity checks
    if((this->timingpoints.size() < 1)) {
        if(cv::debug_osu.getBool()) debugLog("DatabaseBeatmap::loadMetadata() : no timingpoints in beatmap!");
        return false;  // nothing more to do here
    }

    // build sound file path
    this->sFullSoundFilePath = this->sFolder;
    this->sFullSoundFilePath.append(this->sAudioFileName);

    // sort timingpoints and calculate BPM range
    if(this->timingpoints.size() > 0) {
        // sort timingpoints by time
        if(this->timingpoints.size() > 1) {
            std::ranges::sort(this->timingpoints, timingPointSortComparator);
        }

        if(this->iMostCommonBPM == 0) {
            if(cv::debug_osu.getBool()) debugLog("DatabaseBeatmap::loadMetadata() : calculating BPM range ...");
            BPMInfo bpm{};
            if(this->timingpoints.size() > 0) {
                zarray<BPMTuple> bpm_calculation_buffer(this->timingpoints.size());
                bpm = getBPM(this->timingpoints, bpm_calculation_buffer);
            }
            this->iMinBPM = bpm.min;
            this->iMaxBPM = bpm.max;
            this->iMostCommonBPM = bpm.most_common;
        }
    }

    // special case: old beatmaps have AR = OD, there is no ApproachRate stored
    if(!foundAR) this->fAR = this->fOD;

    return true;
}

DatabaseBeatmap::LOAD_GAMEPLAY_RESULT DatabaseBeatmap::loadGameplay(DatabaseBeatmap *databaseBeatmap,
                                                                    AbstractBeatmapInterface *beatmap) {
    LOAD_GAMEPLAY_RESULT result = LOAD_GAMEPLAY_RESULT();

    // NOTE: reload metadata (force ensures that all necessary data is ready for creating hitobjects and playing etc.,
    // also if beatmap file is changed manually in the meantime)
    // XXX: file io, md5 calc, all on main thread!!
    if(!databaseBeatmap->loadMetadata()) {
        result.errorCode = 1;
        return result;
    }

    // load primitives, put in temporary container
    PRIMITIVE_CONTAINER c = loadPrimitiveObjects(databaseBeatmap->sFilePath);
    if(c.errorCode != 0) {
        result.errorCode = c.errorCode;
        return result;
    }
    result.breaks = std::move(c.breaks);
    result.combocolors = std::move(c.combocolors);
    result.defaultSampleSet = c.defaultSampleSet;

    // override some values with data from primitive load, even though they should already be loaded from metadata
    // (sanity)
    databaseBeatmap->timingpoints.swap(c.timingpoints);
    databaseBeatmap->fSliderMultiplier = c.sliderMultiplier;
    databaseBeatmap->fSliderTickRate = c.sliderTickRate;
    databaseBeatmap->fStackLeniency = c.stackLeniency;
    databaseBeatmap->iVersion = c.version;

    // check if we have any timingpoints at all
    if(databaseBeatmap->timingpoints.size() == 0) {
        result.errorCode = 3;
        return result;
    }

    // update numObjects
    databaseBeatmap->iNumObjects = c.numHitobjects;
    databaseBeatmap->iNumCircles = c.numCircles;
    databaseBeatmap->iNumSliders = c.numSliders;
    databaseBeatmap->iNumSpinners = c.numSpinners;

    // check if we have any hitobjects at all
    if(databaseBeatmap->iNumObjects < 1) {
        result.errorCode = 4;
        return result;
    }

    // calculate sliderTimes, and build slider clicks and ticks
    CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT sliderTimeCalcResult =
        calculateSliderTimesClicksTicks(c.version, c.sliders, databaseBeatmap->timingpoints,
                                        databaseBeatmap->fSliderMultiplier, databaseBeatmap->fSliderTickRate);
    if(sliderTimeCalcResult.errorCode != 0) {
        result.errorCode = sliderTimeCalcResult.errorCode;
        return result;
    }

    // build hitobjects from the primitive data we loaded from the osu file
    {
        // also calculate max possible combo
        int maxPossibleCombo = 0;

        for(auto &h : c.hitcircles) {
            result.hitobjects.push_back(
                new Circle(h.x, h.y, h.time, h.samples, h.number, false, h.colorCounter, h.colorOffset, beatmap));
        }
        maxPossibleCombo += c.hitcircles.size();

        for(auto &s : c.sliders) {
            if(cv::mod_strict_tracking.getBool() && cv::mod_strict_tracking_remove_slider_ticks.getBool())
                s.ticks.clear();

            if(cv::mod_reverse_sliders.getBool()) std::ranges::reverse(s.points);

            result.hitobjects.push_back(new Slider(s.type, s.repeat, s.pixelLength, s.points, s.ticks, s.sliderTime,
                                                   s.sliderTimeWithoutRepeats, s.time, s.hoverSamples, s.edgeSamples,
                                                   s.number, false, s.colorCounter, s.colorOffset, beatmap));

            const int repeats = std::max((s.repeat - 1), 0);
            maxPossibleCombo += 2 + repeats + (repeats + 1) * s.ticks.size();  // start/end + repeat arrow + ticks
        }

        for(auto &s : c.spinners) {
            result.hitobjects.push_back(new Spinner(s.x, s.y, s.time, s.samples, false, s.endTime, beatmap));
        }
        maxPossibleCombo += c.spinners.size();

        beatmap->iMaxPossibleCombo = maxPossibleCombo;
    }

    // sort hitobjects by starttime
    if(result.hitobjects.size() > 1) {
        std::ranges::sort(result.hitobjects, BeatmapInterface::sortHitObjectByStartTimeComp);
    }

    // update beatmap length stat
    if(databaseBeatmap->iLengthMS == 0 && result.hitobjects.size() > 0)
        databaseBeatmap->iLengthMS = result.hitobjects[result.hitobjects.size() - 1]->click_time +
                                     result.hitobjects[result.hitobjects.size() - 1]->duration;

    // set isEndOfCombo + precalculate Score v2 combo portion maximum
    if(beatmap != nullptr) {
        u64 scoreV2ComboPortionMaximum = 1;

        if(result.hitobjects.size() > 0) scoreV2ComboPortionMaximum = 0;

        int combo = 0;
        for(size_t i = 0; i < result.hitobjects.size(); i++) {
            HitObject *currentHitObject = result.hitobjects[i];
            const HitObject *nextHitObject = (i + 1 < result.hitobjects.size() ? result.hitobjects[i + 1] : nullptr);

            const Circle *circlePointer = dynamic_cast<Circle *>(currentHitObject);
            const Slider *sliderPointer = dynamic_cast<Slider *>(currentHitObject);
            const Spinner *spinnerPointer = dynamic_cast<Spinner *>(currentHitObject);

            int scoreComboMultiplier = std::max(combo - 1, 0);

            if(circlePointer != nullptr || spinnerPointer != nullptr) {
                scoreV2ComboPortionMaximum += (u64)(300.0 * (1.0 + (double)scoreComboMultiplier / 10.0));
                combo++;
            } else if(sliderPointer != nullptr) {
                combo += 1 + sliderPointer->getClicks().size();
                scoreComboMultiplier = std::max(combo - 1, 0);
                scoreV2ComboPortionMaximum += (u64)(300.0 * (1.0 + (double)scoreComboMultiplier / 10.0));
                combo++;
            }

            if(nextHitObject == nullptr || nextHitObject->combo_number == 1) {
                currentHitObject->is_end_of_combo = true;
            }
        }

        beatmap->iScoreV2ComboPortionMaximum = scoreV2ComboPortionMaximum;
    }

    // special rule for first hitobject (for 1 approach circle with HD)
    if(cv::show_approach_circle_on_first_hidden_object.getBool()) {
        if(result.hitobjects.size() > 0) result.hitobjects[0]->setForceDrawApproachCircle(true);
    }

    // custom override for forcing a hard number cap and/or sequence (visually only)
    // NOTE: this is done after we have already calculated/set isEndOfCombos
    {
        if(cv::ignore_beatmap_combo_numbers.getBool()) {
            // NOTE: spinners don't increment the combo number
            int comboNumber = 1;
            for(auto currentHitObject : result.hitobjects) {
                const Spinner *spinnerPointer = dynamic_cast<Spinner *>(currentHitObject);

                if(spinnerPointer == nullptr) {
                    currentHitObject->combo_number = comboNumber;
                    comboNumber++;
                }
            }
        }

        const int numberMax = cv::number_max.getInt();
        if(numberMax > 0) {
            for(auto currentHitObject : result.hitobjects) {
                const int currentComboNumber = currentHitObject->combo_number;
                const int newComboNumber = (currentComboNumber % numberMax);

                currentHitObject->combo_number = (newComboNumber == 0) ? numberMax : newComboNumber;
            }
        }
    }

    debugLog("DatabaseBeatmap::loadGameplay() loaded {:d} hitobjects", result.hitobjects.size());

    return result;
}

MapOverrides DatabaseBeatmap::get_overrides() {
    MapOverrides overrides;
    overrides.local_offset = this->iLocalOffset;
    overrides.online_offset = this->iOnlineOffset;
    overrides.nb_circles = this->iNumCircles;
    overrides.nb_sliders = this->iNumSliders;
    overrides.nb_spinners = this->iNumSpinners;
    overrides.star_rating = this->fStarsNomod;
    overrides.loudness = this->loudness.load();
    overrides.min_bpm = this->iMinBPM;
    overrides.max_bpm = this->iMaxBPM;
    overrides.avg_bpm = this->iMostCommonBPM;
    overrides.draw_background = this->draw_background;
    return overrides;
}

void DatabaseBeatmap::update_overrides() {
    if(this->type != BeatmapType::PEPPY_DIFFICULTY) return;

    // XXX: not actually thread safe, if m_sMD5Hash gets updated by loadGameplay()
    //      or other values in get_overrides()
    db->peppy_overrides_mtx.lock();
    db->peppy_overrides[this->sMD5Hash] = this->get_overrides();
    db->peppy_overrides_mtx.unlock();
}

DatabaseBeatmap::TIMING_INFO DatabaseBeatmap::getTimingInfoForTime(u32 positionMS) {
    return getTimingInfoForTimeAndTimingPoints(positionMS, this->timingpoints);
}

DatabaseBeatmap::TIMING_INFO DatabaseBeatmap::getTimingInfoForTimeAndTimingPoints(
    u32 positionMS, const zarray<DatabaseBeatmap::TIMINGPOINT> &timingpoints) {
    TIMING_INFO ti{};
    ti.offset = 0;
    ti.beatLengthBase = 1;
    ti.beatLength = 1;
    ti.volume = 100;
    ti.isNaN = false;

    if(timingpoints.size() < 1) return ti;

    // initial values
    ti.offset = timingpoints[0].offset;
    ti.volume = timingpoints[0].volume;
    ti.sampleSet = timingpoints[0].sampleSet;
    ti.sampleIndex = timingpoints[0].sampleIndex;

    // new (peppy's algorithm)
    // (correctly handles aspire & NaNs)
    {
        const bool allowMultiplier = true;

        int point = 0;
        int samplePoint = 0;
        int audioPoint = 0;

        for(int i = 0; i < timingpoints.size(); i++) {
            if(timingpoints[i].offset <= positionMS) {
                audioPoint = i;

                if(timingpoints[i].timingChange)
                    point = i;
                else
                    samplePoint = i;
            }
        }

        double mult = 1;

        if(allowMultiplier && samplePoint > point && timingpoints[samplePoint].msPerBeat < 0) {
            if(timingpoints[samplePoint].msPerBeat >= 0)
                mult = 1;
            else
                mult = std::clamp<float>((float)-timingpoints[samplePoint].msPerBeat, 10.0f, 1000.0f) / 100.0f;
        }

        ti.beatLengthBase = timingpoints[point].msPerBeat;
        ti.offset = timingpoints[point].offset;

        ti.isNaN = std::isnan(timingpoints[samplePoint].msPerBeat) || std::isnan(timingpoints[point].msPerBeat);
        ti.beatLength = ti.beatLengthBase * mult;

        ti.volume = timingpoints[audioPoint].volume;
        ti.sampleSet = timingpoints[audioPoint].sampleSet;
        ti.sampleIndex = timingpoints[audioPoint].sampleIndex;
    }

    return ti;
}

void DatabaseBeatmapBackgroundImagePathLoader::init() {
    // (nothing)
    this->bReady = true;
}

void DatabaseBeatmapBackgroundImagePathLoader::initAsync() {
    if(this->bInterrupted) return;

    File file(this->sFilePath);
    if(this->bInterrupted || !file.canRead()) return;

    bool is_events_block = false;
    while(file.canRead()) {
        if(this->bInterrupted) {
            return;
        }
        std::string curLine = file.readLine();

        // ignore comments, but only if at the beginning of a line (e.g. allow Artist:DJ'TEKINA//SOMETHING)
        if(curLine.starts_with("//")) continue;

        if(curLine.find("[Events]") != std::string::npos) {
            is_events_block = true;
            continue;
        } else if(curLine.find("[TimingPoints]") != std::string::npos)
            break;  // NOTE: stop early
        else if(curLine.find("[Colours]") != std::string::npos)
            break;  // NOTE: stop early
        else if(curLine.find("[HitObjects]") != std::string::npos)
            break;  // NOTE: stop early

        if(!is_events_block) continue;

        std::string str;
        i32 type, startTime;
        if(Parsing::parse(curLine.c_str(), &type, ',', &startTime, ',', &str) && type == 0) {
            this->sLoadedBackgroundImageFileName = str;
            break;
        }
    }

    if(this->bInterrupted) {
        return;
    }

    this->bAsyncReady = true;
    this->bReady = true;  // NOTE: on purpose. there is nothing to do in init(), so finish 1 frame early
}

std::string DatabaseBeatmap::getFullSoundFilePath() {
    // this modifies this->sFullSoundFilePath inplace
    if(File::existsCaseInsensitive(this->sFullSoundFilePath) == File::FILETYPE::FILE) {
        return this->sFullSoundFilePath;
    }

    // wasn't found but return what we have anyways
    return this->sFullSoundFilePath;
}
