// Copyright (c) 2024, kiwec, 2025, WH, All rights reserved.
#include "LivePPCalc.h"

#include "Logging.h"

#include "Osu.h"
#include "BeatmapInterface.h"
#include "DifficultyCalculator.h"
#include "uwu.h"
#include "ConVar.h"

namespace cv {
extern ConVar debug_pp;
}

struct LivePPCalc::LivePPCalcImpl {
    NOCOPY_NOMOVE(LivePPCalcImpl);

   public:
    struct LazyPPRes {  // to know what we are actually up-to-date with
        AsyncPPC::pp_res res{};
        i32 calc_index{-1};
    };
    uwu::lazy_promise<std::function<LazyPPRes()>> m_calc_inst{{}};

    BeatmapInterface *m_bmi;  // parent instance

    f32 m_live_stars{0.f};
    f32 m_live_pp{0.f};

    i32 m_last_calculated{-1};
    i32 m_last_queued{-1};

    bool m_calculated_valid{false};
    bool m_queued_valid{false};

    inline void update_calc_res(i32 curIdx, const LazyPPRes &resPair) {
        m_calculated_valid = (resPair.calc_index == curIdx);
        m_last_calculated = resPair.calc_index;

        m_live_pp = resPair.res.pp;
        m_live_stars = resPair.res.total_stars;
    }
    inline void update_queued_idx(i32 queuedIdx) { m_last_queued = queuedIdx; }

    [[nodiscard]] inline bool needs_update(i32 curIdx) const {
        return !m_calculated_valid || (curIdx != m_last_calculated && curIdx >= 0);
    }

    inline bool needs_queue(i32 curIdx) {
        const bool was_invalid = !m_queued_valid;
        if(was_invalid) m_queued_valid = true;  // only force queue once
        return was_invalid || (curIdx != m_last_queued && curIdx >= 0);
    }

    struct lazyCalcParams {
       private:
        struct storage {
            std::string lastCalcedPath;
            f32 lastCalcedAR, lastCalcedCS;
            f32 lastCalcedSpeedMultiplier;
            DatabaseBeatmap::LOAD_DIFFOBJ_RESULT diffres;
            std::unique_ptr<std::vector<DifficultyCalculator::DiffObject>> diffobjCache;
        };

       public:
        std::string osufile_path;
        u64 legacyTotalScore;
        f32 CS, AR, HP, OD;
        f32 speedMultiplier;
        int current_hitobject;
        int nb_circles, nb_sliders, nb_spinners;
        int highestCombo, numMisses;
        int num300s, num100s, num50s;
        Replay::Mods mods;

        // to avoid rebuilding diffres unless something changes
        [[nodiscard]] storage &get_cache() const {
            static storage c{.lastCalcedPath = this->osufile_path,
                             .lastCalcedAR = this->AR,
                             .lastCalcedCS = this->CS,
                             .lastCalcedSpeedMultiplier = this->speedMultiplier,
                             .diffres = DatabaseBeatmap::loadDifficultyHitObjects(this->osufile_path, this->AR,
                                                                                  this->CS, this->speedMultiplier),
                             .diffobjCache = std::make_unique<std::vector<DifficultyCalculator::DiffObject>>()};

            // rebuild as necessary
            if(c.lastCalcedPath != this->osufile_path || c.lastCalcedAR != this->AR || c.lastCalcedCS != this->CS ||
               c.lastCalcedSpeedMultiplier != this->speedMultiplier) {
                c.lastCalcedPath = this->osufile_path;
                c.lastCalcedAR = this->AR;
                c.lastCalcedCS = this->CS;
                c.lastCalcedSpeedMultiplier = this->speedMultiplier;
                // get new diffres
                c.diffobjCache->clear();
                c.diffres = DatabaseBeatmap::loadDifficultyHitObjects(this->osufile_path, this->AR, this->CS,
                                                                      this->speedMultiplier);
            }
            return c;
        }
    };

    LivePPCalcImpl() = delete;
    LivePPCalcImpl(BeatmapInterface *parent) : m_bmi(parent) {}
    ~LivePPCalcImpl() = default;

    // designed in a way that most of the heavy lifting is done off-thread (inside the calc_inst invocation)
    void update(LiveScore &score) {
        const i32 cur_hobj = m_bmi->iCurrentHitObjectIndex;

        if(!needs_update(cur_hobj)) {
            return;
        }

        // get most recent results and update hud
        if(auto maybe_res = m_calc_inst.try_get(); maybe_res != std::nullopt) {
            update_calc_res(cur_hobj, *maybe_res);
        }

        if(!needs_queue(cur_hobj)) {
            return;
        }

        update_queued_idx(cur_hobj);

        m_calc_inst.enqueue([p = lazyCalcParams{
                                 .osufile_path = m_bmi->beatmap ? m_bmi->beatmap->getFilePath() : "",  //
                                 .legacyTotalScore = score.getScore(),                                 //
                                 .CS = m_bmi->getCS(),                                                 //
                                 .AR = m_bmi->getAR(),                                                 //
                                 .HP = m_bmi->getHP(),                                                 //
                                 .OD = m_bmi->getOD(),                                                 //
                                 .speedMultiplier = m_bmi->getSpeedMultiplier(),                       //
                                 .current_hitobject = m_bmi->iCurrentHitObjectIndex,                   //
                                 .nb_circles = m_bmi->iCurrentNumCircles,                              //
                                 .nb_sliders = m_bmi->iCurrentNumSliders,                              //
                                 .nb_spinners = m_bmi->iCurrentNumSpinners,                            //
                                 .highestCombo = score.getComboMax(),                                  //
                                 .numMisses = score.getNumMisses(),                                    //
                                 .num300s = score.getNum300s(),                                        //
                                 .num100s = score.getNum100s(),                                        //
                                 .num50s = score.getNum50s(),                                          //
                                 .mods = score.mods,                                                   //
                             }](void) -> LazyPPRes {
            LazyPPRes result;

            if(p.osufile_path.empty()) return result;

            AsyncPPC::pp_res &retInfo = result.res;

            auto &cache = p.get_cache();
            DatabaseBeatmap::LOAD_DIFFOBJ_RESULT &diffres = cache.diffres;

            if(diffres.error.errc) return result;  // uh-oh

            const bool relax = flags::has<ModFlags::Relax>(p.mods.flags);
            const bool td = flags::has<ModFlags::TouchDevice>(p.mods.flags);
            const bool hidden = flags::has<ModFlags::Hidden>(p.mods.flags);
            const bool autopilot = flags::has<ModFlags::Autopilot>(p.mods.flags);
            const bool modAuto = flags::has<ModFlags::Autoplay>(p.mods.flags);

            // this is assumed to always be valid
            std::unique_ptr<std::vector<DifficultyCalculator::DiffObject>> &diffobjCache = cache.diffobjCache;

            DifficultyCalculator::BeatmapDiffcalcData diffcalcData{.sortedHitObjects = diffres.diffobjects,
                                                                   .CS = p.CS,
                                                                   .HP = p.HP,
                                                                   .AR = p.AR,
                                                                   .OD = p.OD,
                                                                   .hidden = hidden,
                                                                   .relax = relax,
                                                                   .autopilot = autopilot,
                                                                   .touchDevice = td,
                                                                   .speedMultiplier = p.speedMultiplier,
                                                                   .breakDuration = diffres.totalBreakDuration,
                                                                   .playableLength = diffres.playableLength};

            DifficultyCalculator::DifficultyAttributes diffattrsOut{};

            DifficultyCalculator::StarCalcParams params{
                .cachedDiffObjects = std::move(diffobjCache),
                .outAttributes = diffattrsOut,
                .beatmapData = diffcalcData,
                .outAimStrains = &retInfo.aimStrains,
                .outSpeedStrains = &retInfo.speedStrains,
                .incremental = nullptr,  // TODO: use incremental instead of this bs
                .upToObjectIndex = p.current_hitobject,
                .cancelCheck = {},
                .forceFillDiffobjCache = true};

            retInfo.total_stars = DifficultyCalculator::calculateStarDiffForHitObjects(params);

            // move unique_ptr ownership back
            diffobjCache = std::move(params.cachedDiffObjects);

            retInfo.aim_stars = diffattrsOut.AimDifficulty;
            retInfo.aim_slider_factor = diffattrsOut.SliderFactor;
            retInfo.difficult_aim_sliders = diffattrsOut.AimDifficultSliderCount;
            retInfo.difficult_aim_strains = diffattrsOut.AimDifficultStrainCount;
            retInfo.speed_stars = diffattrsOut.SpeedDifficulty;
            retInfo.speed_notes = diffattrsOut.SpeedNoteCount;
            retInfo.difficult_speed_strains = diffattrsOut.SpeedDifficultStrainCount;

            DifficultyCalculator::PPv2CalcParams ppv2pars{
                .attributes = diffattrsOut,
                .modFlags = p.mods.flags,
                .timescale = p.mods.speed,
                .ar = p.AR,
                .od = p.OD,
                .numHitObjects = p.current_hitobject,
                .numCircles = p.nb_circles,
                .numSliders = p.nb_sliders,
                .numSpinners = p.nb_spinners,
                .maxPossibleCombo = (i32)diffres.getMaxComboAtIndex(p.current_hitobject < 0 ? 0 : p.current_hitobject),
                .combo = p.highestCombo,
                .misses = p.numMisses,
                .c300 = p.num300s,
                .c100 = p.num100s,
                .c50 = p.num50s,
                .legacyTotalScore = (u32)p.legacyTotalScore};

            // HACKHACK: for auto, just ignore reality and calculate maximum pp of a perfect play up until this point
            // this allows calculation after seeking and dropping combo
            // need to re-simulate the play up until that point to be accurate
            if(modAuto) {
                ppv2pars.combo = ppv2pars.maxPossibleCombo;
                ppv2pars.c300 = ppv2pars.numHitObjects;
                ppv2pars.c100 = ppv2pars.c50 = ppv2pars.misses = 0;
                ppv2pars.legacyTotalScore = 0;  // no score-based misscount
            }

            retInfo.pp = DifficultyCalculator::calculatePPv2(ppv2pars);

            if(cv::debug_pp.getBool()) {
                logRaw("[LivePPCalc] PP: {} params (post):\n{}", retInfo.pp,
                               DifficultyCalculator::PPv2CalcParamsToString(ppv2pars));
            }

            result.calc_index = p.current_hitobject;
            return result;
        });
    }

    void invalidate() {
        m_last_calculated = -1;
        m_last_queued = -1;
        m_calculated_valid = false;
        m_queued_valid = false;

        m_live_pp = 0.f;
        m_live_stars = 0.f;
    }
};

LivePPCalc::LivePPCalc(BeatmapInterface *parent) : pImpl(parent) {}
LivePPCalc::~LivePPCalc() = default;

void LivePPCalc::update(LiveScore &score) { return pImpl->update(score); }
void LivePPCalc::invalidate() { return pImpl->invalidate(); }

float LivePPCalc::get_stars() const { return pImpl->m_live_stars; }
float LivePPCalc::get_pp() const { return pImpl->m_live_pp; }
