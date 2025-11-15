// Copyright (c) 2016, PG, All rights reserved.
#include "Replay.h"

#include "BeatmapInterface.h"
#include "CBaseUICheckbox.h"
#include "CBaseUISlider.h"
#include "DatabaseBeatmap.h"
#include "GameRules.h"
#include "ConVar.h"
#include "Osu.h"

namespace Replay {

LegacyFlags Mods::to_legacy() const {
    LegacyFlags legacy_flags{};
    if(this->speed > 1.f) {
        legacy_flags |= LegacyFlags::DoubleTime;
        if(flags::has<ModFlags::NoPitchCorrection>(this->flags)) legacy_flags |= LegacyFlags::Nightcore;
    } else if(this->speed < 1.f) {
        legacy_flags |= LegacyFlags::HalfTime;
    }

    if(flags::has<ModFlags::NoHP>(this->flags)) legacy_flags |= LegacyFlags::NoFail;
    if(flags::has<ModFlags::NoFail>(this->flags)) legacy_flags |= LegacyFlags::NoFail;
    if(flags::has<ModFlags::Easy>(this->flags)) legacy_flags |= LegacyFlags::Easy;
    if(flags::has<ModFlags::TouchDevice>(this->flags)) legacy_flags |= LegacyFlags::TouchDevice;
    if(flags::has<ModFlags::Hidden>(this->flags)) legacy_flags |= LegacyFlags::Hidden;
    if(flags::has<ModFlags::HardRock>(this->flags)) legacy_flags |= LegacyFlags::HardRock;
    if(flags::has<ModFlags::SuddenDeath>(this->flags)) legacy_flags |= LegacyFlags::SuddenDeath;
    if(flags::has<ModFlags::Relax>(this->flags)) legacy_flags |= LegacyFlags::Relax;
    if(flags::has<ModFlags::Flashlight>(this->flags)) legacy_flags |= LegacyFlags::Flashlight;
    if(flags::has<ModFlags::SpunOut>(this->flags)) legacy_flags |= LegacyFlags::SpunOut;
    if(flags::has<ModFlags::Autopilot>(this->flags)) legacy_flags |= LegacyFlags::Autopilot;
    if(flags::has<ModFlags::Perfect>(this->flags)) legacy_flags |= LegacyFlags::Perfect;
    if(flags::has<ModFlags::Target>(this->flags)) legacy_flags |= LegacyFlags::Target;
    if(flags::has<ModFlags::ScoreV2>(this->flags)) legacy_flags |= LegacyFlags::ScoreV2;
    if(flags::has<ModFlags::Autoplay>(this->flags)) {
        legacy_flags &= ~(LegacyFlags::Relax | LegacyFlags::Autopilot);
        legacy_flags |= LegacyFlags::Autoplay;
    }

    // NOTE: Ignoring nightmare, fposu

    return legacy_flags;
}

f32 Mods::get_naive_ar(const DatabaseBeatmap *map) const {
    float ARdifficultyMultiplier = 1.0f;
    if((this->has(ModFlags::HardRock))) ARdifficultyMultiplier = 1.4f;
    if((this->has(ModFlags::Easy))) ARdifficultyMultiplier = 0.5f;

    f32 AR = std::clamp<f32>(map->getAR() * ARdifficultyMultiplier, 0.0f, 10.0f);
    if(this->ar_override >= 0.0f) AR = this->ar_override;
    if(this->ar_overridenegative < 0.0f) AR = this->ar_overridenegative;

    if(this->has(ModFlags::AROverrideLock)) {
        AR = GameRules::arWithSpeed(AR, 1.f / this->speed);
    }

    return AR;
}

f32 Mods::get_naive_cs(const DatabaseBeatmap *map) const {
    float CSdifficultyMultiplier = 1.0f;
    if((this->has(ModFlags::HardRock))) CSdifficultyMultiplier = 1.3f;  // different!
    if((this->has(ModFlags::Easy))) CSdifficultyMultiplier = 0.5f;

    f32 CS = std::clamp<f32>(map->getCS() * CSdifficultyMultiplier, 0.0f, 10.0f);
    if(this->cs_override >= 0.0f) CS = this->cs_override;
    if(this->cs_overridenegative < 0.0f) CS = this->cs_overridenegative;

    return CS;
}

f32 Mods::get_naive_od(const DatabaseBeatmap *map) const {
    float ODdifficultyMultiplier = 1.0f;
    if((this->has(ModFlags::HardRock))) ODdifficultyMultiplier = 1.4f;
    if((this->has(ModFlags::Easy))) ODdifficultyMultiplier = 0.5f;

    f32 OD = std::clamp<f32>(map->getOD() * ODdifficultyMultiplier, 0.0f, 10.0f);
    if(this->od_override >= 0.0f) OD = this->od_override;

    if(this->has(ModFlags::ODOverrideLock)) {
        OD = GameRules::odWithSpeed(OD, 1.f / this->speed);
    }

    return OD;
}

Mods Mods::from_cvars() {
    using enum ModFlags;
    Mods mods;

    if(cv::mod_nofail.getBool()) mods.flags |= NoFail;
    if(cv::drain_disabled.getBool()) mods.flags |= NoHP;  // Not an actual "mod", it's in the options menu
    if(cv::mod_easy.getBool()) mods.flags |= Easy;
    if(cv::mod_autopilot.getBool()) mods.flags |= Autopilot;
    if(cv::mod_relax.getBool()) mods.flags |= Relax;
    if(cv::mod_hidden.getBool()) mods.flags |= Hidden;
    if(cv::mod_hardrock.getBool()) mods.flags |= HardRock;
    if(cv::mod_flashlight.getBool()) mods.flags |= Flashlight;
    if(cv::mod_suddendeath.getBool()) mods.flags |= SuddenDeath;
    if(cv::mod_perfect.getBool()) mods.flags |= Perfect;
    if(cv::mod_nightmare.getBool()) mods.flags |= Nightmare;
    if(cv::nightcore_enjoyer.getBool()) mods.flags |= NoPitchCorrection;
    if(cv::mod_touchdevice.getBool()) mods.flags |= TouchDevice;
    if(cv::mod_spunout.getBool()) mods.flags |= SpunOut;
    if(cv::mod_scorev2.getBool()) mods.flags |= ScoreV2;
    if(cv::mod_fposu.getBool()) mods.flags |= FPoSu;
    if(cv::mod_target.getBool()) mods.flags |= Target;
    if(cv::ar_override_lock.getBool()) mods.flags |= AROverrideLock;
    if(cv::od_override_lock.getBool()) mods.flags |= ODOverrideLock;
    if(cv::mod_timewarp.getBool()) mods.flags |= Timewarp;
    if(cv::mod_artimewarp.getBool()) mods.flags |= ARTimewarp;
    if(cv::mod_minimize.getBool()) mods.flags |= Minimize;
    if(cv::mod_jigsaw1.getBool()) mods.flags |= Jigsaw1;
    if(cv::mod_jigsaw2.getBool()) mods.flags |= Jigsaw2;
    if(cv::mod_wobble.getBool()) mods.flags |= Wobble1;
    if(cv::mod_wobble2.getBool()) mods.flags |= Wobble2;
    if(cv::mod_arwobble.getBool()) mods.flags |= ARWobble;
    if(cv::mod_fullalternate.getBool()) mods.flags |= FullAlternate;
    if(cv::mod_shirone.getBool()) mods.flags |= Shirone;
    if(cv::mod_mafham.getBool()) mods.flags |= Mafham;
    if(cv::mod_halfwindow.getBool()) mods.flags |= HalfWindow;
    if(cv::mod_halfwindow_allow_300s.getBool()) mods.flags |= HalfWindowAllow300s;
    if(cv::mod_ming3012.getBool()) mods.flags |= Ming3012;
    if(cv::mod_no100s.getBool()) mods.flags |= No100s;
    if(cv::mod_no50s.getBool()) mods.flags |= No50s;
    if(cv::mod_singletap.getBool()) mods.flags |= Singletap;
    if(cv::mod_no_keylock.getBool()) mods.flags |= NoKeylock;
    if(cv::mod_no_pausing.getBool()) mods.flags |= NoPausing;
    if(cv::mod_autoplay.getBool()) {
        mods.flags &= ~(Relax | Autopilot);
        mods.flags |= Autoplay;
    }

    mods.speed = osu->getMapInterface()->getSpeedMultiplier();

    mods.notelock_type = cv::notelock_type.getInt();
    mods.autopilot_lenience = cv::autopilot_lenience.getFloat();
    mods.ar_override = cv::ar_override.getFloat();
    mods.ar_overridenegative = cv::ar_overridenegative.getFloat();
    mods.cs_override = cv::cs_override.getFloat();
    mods.cs_overridenegative = cv::cs_overridenegative.getFloat();
    mods.hp_override = cv::hp_override.getFloat();
    mods.od_override = cv::od_override.getFloat();
    mods.timewarp_multiplier = cv::mod_timewarp_multiplier.getFloat();
    mods.minimize_multiplier = cv::mod_minimize_multiplier.getFloat();
    mods.artimewarp_multiplier = cv::mod_artimewarp_multiplier.getFloat();
    mods.arwobble_strength = cv::mod_arwobble_strength.getFloat();
    mods.arwobble_interval = cv::mod_arwobble_interval.getFloat();
    mods.wobble_strength = cv::mod_wobble_strength.getFloat();
    mods.wobble_rotation_speed = cv::mod_wobble_rotation_speed.getFloat();
    mods.jigsaw_followcircle_radius_factor = cv::mod_jigsaw_followcircle_radius_factor.getFloat();
    mods.shirone_combo = cv::mod_shirone_combo.getFloat();

    return mods;
}

Mods Mods::from_legacy(LegacyFlags legacy_flags) {
    ModFlags neoflags{};
    if(flags::has<LegacyFlags::NoFail>(legacy_flags)) neoflags |= ModFlags::NoFail;
    if(flags::has<LegacyFlags::Easy>(legacy_flags)) neoflags |= ModFlags::Easy;
    if(flags::has<LegacyFlags::TouchDevice>(legacy_flags)) neoflags |= ModFlags::TouchDevice;
    if(flags::has<LegacyFlags::Hidden>(legacy_flags)) neoflags |= ModFlags::Hidden;
    if(flags::has<LegacyFlags::HardRock>(legacy_flags)) neoflags |= ModFlags::HardRock;
    if(flags::has<LegacyFlags::SuddenDeath>(legacy_flags)) neoflags |= ModFlags::SuddenDeath;
    if(flags::has<LegacyFlags::Relax>(legacy_flags)) neoflags |= ModFlags::Relax;
    if(flags::has<LegacyFlags::Nightcore>(legacy_flags)) neoflags |= ModFlags::NoPitchCorrection;
    if(flags::has<LegacyFlags::Flashlight>(legacy_flags)) neoflags |= ModFlags::Flashlight;
    if(flags::has<LegacyFlags::SpunOut>(legacy_flags)) neoflags |= ModFlags::SpunOut;
    if(flags::has<LegacyFlags::Autopilot>(legacy_flags)) neoflags |= ModFlags::Autopilot;
    if(flags::has<LegacyFlags::Perfect>(legacy_flags)) neoflags |= ModFlags::Perfect;
    if(flags::has<LegacyFlags::Target>(legacy_flags)) neoflags |= ModFlags::Target;
    if(flags::has<LegacyFlags::ScoreV2>(legacy_flags)) neoflags |= ModFlags::ScoreV2;
    if(flags::has<LegacyFlags::Nightmare>(legacy_flags)) neoflags |= ModFlags::Nightmare;
    if(flags::has<LegacyFlags::FPoSu>(legacy_flags)) neoflags |= ModFlags::FPoSu;
    if(flags::has<LegacyFlags::Mirror>(legacy_flags)) {
        // NOTE: We don't know whether the original score was only horizontal, only vertical, or both
        neoflags |= (ModFlags::MirrorHorizontal | ModFlags::MirrorVertical);
    }
    if(flags::has<LegacyFlags::Autoplay>(legacy_flags)) {
        neoflags &= ~(ModFlags::Relax | ModFlags::Autopilot);
        neoflags |= ModFlags::Autoplay;
    }

    Mods mods;
    mods.flags = neoflags;
    if(flags::has<LegacyFlags::DoubleTime>(legacy_flags))
        mods.speed = 1.5f;
    else if(flags::has<LegacyFlags::HalfTime>(legacy_flags))
        mods.speed = 0.75f;
    return mods;
}

void Mods::use(const Mods &mods) {
    using enum ModFlags;
    // Reset mod selector buttons and sliders
    const auto &mod_selector = osu->getModSelector();
    mod_selector->resetMods();

    // Set cvars
    // FIXME: NoHP should not be changed here, it's a global option
    cv::drain_disabled.setValue(flags::has<NoHP>(mods.flags));
    cv::mod_nofail.setValue(flags::has<NoFail>(mods.flags));
    cv::mod_easy.setValue(flags::has<Easy>(mods.flags));
    cv::mod_hidden.setValue(flags::has<Hidden>(mods.flags));
    cv::mod_hardrock.setValue(flags::has<HardRock>(mods.flags));
    cv::mod_flashlight.setValue(flags::has<Flashlight>(mods.flags));
    cv::mod_suddendeath.setValue(flags::has<SuddenDeath>(mods.flags));
    cv::mod_perfect.setValue(flags::has<Perfect>(mods.flags));
    cv::mod_nightmare.setValue(flags::has<Nightmare>(mods.flags));
    cv::nightcore_enjoyer.setValue(flags::has<NoPitchCorrection>(mods.flags));
    cv::mod_touchdevice.setValue(flags::has<TouchDevice>(mods.flags));
    cv::mod_spunout.setValue(flags::has<SpunOut>(mods.flags));
    cv::mod_scorev2.setValue(flags::has<ScoreV2>(mods.flags));
    cv::mod_fposu.setValue(flags::has<FPoSu>(mods.flags));
    cv::mod_target.setValue(flags::has<Target>(mods.flags));
    cv::ar_override_lock.setValue(flags::has<AROverrideLock>(mods.flags));
    cv::od_override_lock.setValue(flags::has<ODOverrideLock>(mods.flags));
    cv::mod_timewarp.setValue(flags::has<Timewarp>(mods.flags));
    cv::mod_artimewarp.setValue(flags::has<ARTimewarp>(mods.flags));
    cv::mod_minimize.setValue(flags::has<Minimize>(mods.flags));
    cv::mod_jigsaw1.setValue(flags::has<Jigsaw1>(mods.flags));
    cv::mod_jigsaw2.setValue(flags::has<Jigsaw2>(mods.flags));
    cv::mod_wobble.setValue(flags::has<Wobble1>(mods.flags));
    cv::mod_wobble2.setValue(flags::has<Wobble2>(mods.flags));
    cv::mod_arwobble.setValue(flags::has<ARWobble>(mods.flags));
    cv::mod_fullalternate.setValue(flags::has<FullAlternate>(mods.flags));
    cv::mod_shirone.setValue(flags::has<Shirone>(mods.flags));
    cv::mod_mafham.setValue(flags::has<Mafham>(mods.flags));
    cv::mod_halfwindow.setValue(flags::has<HalfWindow>(mods.flags));
    cv::mod_halfwindow_allow_300s.setValue(flags::has<HalfWindowAllow300s>(mods.flags));
    cv::mod_ming3012.setValue(flags::has<Ming3012>(mods.flags));
    cv::mod_no100s.setValue(flags::has<No100s>(mods.flags));
    cv::mod_no50s.setValue(flags::has<No50s>(mods.flags));
    cv::mod_singletap.setValue(flags::has<Singletap>(mods.flags));
    cv::mod_no_keylock.setValue(flags::has<NoKeylock>(mods.flags));
    cv::mod_no_pausing.setValue(flags::has<NoPausing>(mods.flags));
    cv::notelock_type.setValue(mods.notelock_type);
    cv::autopilot_lenience.setValue(mods.autopilot_lenience);
    cv::mod_timewarp_multiplier.setValue(mods.timewarp_multiplier);
    cv::mod_minimize_multiplier.setValue(mods.minimize_multiplier);
    cv::mod_artimewarp_multiplier.setValue(mods.artimewarp_multiplier);
    cv::mod_arwobble_strength.setValue(mods.arwobble_strength);
    cv::mod_arwobble_interval.setValue(mods.arwobble_interval);
    cv::mod_wobble_strength.setValue(mods.wobble_strength);
    cv::mod_wobble_rotation_speed.setValue(mods.wobble_rotation_speed);
    cv::mod_jigsaw_followcircle_radius_factor.setValue(mods.jigsaw_followcircle_radius_factor);
    cv::mod_shirone_combo.setValue(mods.shirone_combo);
    cv::ar_override.setValue(mods.ar_override);
    cv::ar_overridenegative.setValue(mods.ar_overridenegative);
    cv::cs_override.setValue(mods.cs_override);
    cv::cs_overridenegative.setValue(mods.cs_overridenegative);
    cv::hp_override.setValue(mods.hp_override);
    cv::od_override.setValue(mods.od_override);
    if(flags::has<Autoplay>(mods.flags)) {
        cv::mod_autoplay.setValue(true);
        cv::mod_autopilot.setValue(false);
        cv::mod_relax.setValue(false);
    } else {
        cv::mod_autoplay.setValue(false);
        cv::mod_autopilot.setValue(flags::has<Autopilot>(mods.flags));
        cv::mod_relax.setValue(flags::has<Relax>(mods.flags));
    }

    f32 speed_override = mods.speed == 1.f ? -1.f : mods.speed;
    cv::speed_override.setValue(speed_override);

    // Update mod selector UI
    mod_selector->enableModsFromFlags(mods.to_legacy());
    cv::speed_override.setValue(speed_override);  // enableModsFromFlags() edits cv::speed_override
    mod_selector->ARLock->setChecked(flags::has<AROverrideLock>(mods.flags));
    mod_selector->ODLock->setChecked(flags::has<ODOverrideLock>(mods.flags));
    mod_selector->speedSlider->setValue(mods.speed, false, false);
    mod_selector->CSSlider->setValue(mods.cs_override, false, false);
    mod_selector->ARSlider->setValue(mods.ar_override, false, false);
    mod_selector->ODSlider->setValue(mods.od_override, false, false);
    mod_selector->HPSlider->setValue(mods.hp_override, false, false);
    mod_selector->updateOverrideSliderLabels();
    mod_selector->updateExperimentalButtons();

    // FIXME: this is already called like 5 times from the previous calls
    osu->updateMods();
}
}  // namespace Replay
