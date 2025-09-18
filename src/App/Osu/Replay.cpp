// Copyright (c) 2016, PG, All rights reserved.
#include "Replay.h"

#include "BeatmapInterface.h"
#include "CBaseUICheckbox.h"
#include "CBaseUISlider.h"
#include "DatabaseBeatmap.h"
#include "GameRules.h"
#include "Osu.h"

namespace Replay {

u32 Mods::to_legacy() const {
    using namespace ModMasks;
    u32 legacy_flags = 0;
    if(this->speed > 1.f) {
        legacy_flags |= LegacyFlags::DoubleTime;
        if(eq(this->flags, ModFlags::NoPitchCorrection)) legacy_flags |= LegacyFlags::Nightcore;
    } else if(this->speed < 1.f) {
        legacy_flags |= LegacyFlags::HalfTime;
    }

    if(eq(this->flags, ModFlags::NoFail)) legacy_flags |= LegacyFlags::NoFail;
    if(eq(this->flags, ModFlags::Easy)) legacy_flags |= LegacyFlags::Easy;
    if(eq(this->flags, ModFlags::TouchDevice)) legacy_flags |= LegacyFlags::TouchDevice;
    if(eq(this->flags, ModFlags::Hidden)) legacy_flags |= LegacyFlags::Hidden;
    if(eq(this->flags, ModFlags::HardRock)) legacy_flags |= LegacyFlags::HardRock;
    if(eq(this->flags, ModFlags::SuddenDeath)) legacy_flags |= LegacyFlags::SuddenDeath;
    if(eq(this->flags, ModFlags::Relax)) legacy_flags |= LegacyFlags::Relax;
    if(eq(this->flags, ModFlags::Flashlight)) legacy_flags |= LegacyFlags::Flashlight;
    if(eq(this->flags, ModFlags::SpunOut)) legacy_flags |= LegacyFlags::SpunOut;
    if(eq(this->flags, ModFlags::Autopilot)) legacy_flags |= LegacyFlags::Autopilot;
    if(eq(this->flags, ModFlags::Perfect)) legacy_flags |= LegacyFlags::Perfect;
    if(eq(this->flags, ModFlags::Target)) legacy_flags |= LegacyFlags::Target;
    if(eq(this->flags, ModFlags::ScoreV2)) legacy_flags |= LegacyFlags::ScoreV2;
    if(eq(this->flags, ModFlags::Autoplay)) {
        legacy_flags &= ~(LegacyFlags::Relax | LegacyFlags::Autopilot);
        legacy_flags |= LegacyFlags::Autoplay;
    }

    // NOTE: Ignoring nightmare, fposu

    return legacy_flags;
}

f32 Mods::get_naive_ar(DatabaseBeatmap *map) const {
    float ARdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->flags, ModFlags::HardRock))) ARdifficultyMultiplier = 1.4f;
    if((ModMasks::eq(this->flags, ModFlags::Easy))) ARdifficultyMultiplier = 0.5f;

    f32 AR = std::clamp<f32>(map->getAR() * ARdifficultyMultiplier, 0.0f, 10.0f);
    if(this->ar_override >= 0.0f) AR = this->ar_override;
    if(this->ar_overridenegative < 0.0f) AR = this->ar_overridenegative;

    if(ModMasks::eq(this->flags, ModFlags::AROverrideLock)) {
        AR = GameRules::arWithSpeed(AR, 1.f / this->speed);
    }

    return AR;
}

f32 Mods::get_naive_cs(DatabaseBeatmap *map) const {
    float CSdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->flags, ModFlags::HardRock))) CSdifficultyMultiplier = 1.3f;  // different!
    if((ModMasks::eq(this->flags, ModFlags::Easy))) CSdifficultyMultiplier = 0.5f;

    f32 CS = std::clamp<f32>(map->getCS() * CSdifficultyMultiplier, 0.0f, 10.0f);
    if(this->cs_override >= 0.0f) CS = this->cs_override;
    if(this->cs_overridenegative < 0.0f) CS = this->cs_overridenegative;

    return CS;
}

f32 Mods::get_naive_od(DatabaseBeatmap *map) const {
    float ODdifficultyMultiplier = 1.0f;
    if((ModMasks::eq(this->flags, ModFlags::HardRock))) ODdifficultyMultiplier = 1.4f;
    if((ModMasks::eq(this->flags, ModFlags::Easy))) ODdifficultyMultiplier = 0.5f;

    f32 OD = std::clamp<f32>(map->getOD() * ODdifficultyMultiplier, 0.0f, 10.0f);
    if(this->od_override >= 0.0f) OD = this->od_override;

    if(ModMasks::eq(this->flags, ModFlags::ODOverrideLock)) {
        OD = GameRules::odWithSpeed(OD, 1.f / this->speed);
    }

    return OD;
}

Mods Mods::from_cvars() {
    using namespace ModFlags;
    Mods mods;

    if(cv::mod_nofail.getBool()) mods.flags |= NoFail;
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
    if(cv::mod_autoplay.getBool()) {
        mods.flags &= ~(Relax | Autopilot);
        mods.flags |= Autoplay;
    }

    mods.speed = osu->active_map->getSpeedMultiplier();

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

Mods Mods::from_legacy(u32 legacy_flags) {
    using namespace ModMasks;
    u64 neoflags = 0;
    if(legacy_eq(legacy_flags, LegacyFlags::NoFail)) neoflags |= ModFlags::NoFail;
    if(legacy_eq(legacy_flags, LegacyFlags::Easy)) neoflags |= ModFlags::Easy;
    if(legacy_eq(legacy_flags, LegacyFlags::TouchDevice)) neoflags |= ModFlags::TouchDevice;
    if(legacy_eq(legacy_flags, LegacyFlags::Hidden)) neoflags |= ModFlags::Hidden;
    if(legacy_eq(legacy_flags, LegacyFlags::HardRock)) neoflags |= ModFlags::HardRock;
    if(legacy_eq(legacy_flags, LegacyFlags::SuddenDeath)) neoflags |= ModFlags::SuddenDeath;
    if(legacy_eq(legacy_flags, LegacyFlags::Relax)) neoflags |= ModFlags::Relax;
    if(legacy_eq(legacy_flags, LegacyFlags::Nightcore)) neoflags |= ModFlags::NoPitchCorrection;
    if(legacy_eq(legacy_flags, LegacyFlags::Flashlight)) neoflags |= ModFlags::Flashlight;
    if(legacy_eq(legacy_flags, LegacyFlags::SpunOut)) neoflags |= ModFlags::SpunOut;
    if(legacy_eq(legacy_flags, LegacyFlags::Autopilot)) neoflags |= ModFlags::Autopilot;
    if(legacy_eq(legacy_flags, LegacyFlags::Perfect)) neoflags |= ModFlags::Perfect;
    if(legacy_eq(legacy_flags, LegacyFlags::Target)) neoflags |= ModFlags::Target;
    if(legacy_eq(legacy_flags, LegacyFlags::ScoreV2)) neoflags |= ModFlags::ScoreV2;
    if(legacy_eq(legacy_flags, LegacyFlags::Nightmare)) neoflags |= ModFlags::Nightmare;
    if(legacy_eq(legacy_flags, LegacyFlags::FPoSu)) neoflags |= ModFlags::FPoSu;
    if(legacy_eq(legacy_flags, LegacyFlags::Mirror)) {
        // NOTE: We don't know whether the original score was only horizontal, only vertical, or both
        neoflags |= (ModFlags::MirrorHorizontal | ModFlags::MirrorVertical);
    }
    if(legacy_eq(legacy_flags, LegacyFlags::Autoplay)) {
        neoflags &= ~(ModFlags::Relax | ModFlags::Autopilot);
        neoflags |= ModFlags::Autoplay;
    }

    Mods mods;
    mods.flags = neoflags;
    if(legacy_eq(legacy_flags, LegacyFlags::DoubleTime))
        mods.speed = 1.5f;
    else if(legacy_eq(legacy_flags, LegacyFlags::HalfTime))
        mods.speed = 0.75f;
    return mods;
}

void Mods::use(const Mods &mods) {
    using namespace ModFlags;
    using namespace ModMasks;
    // Reset mod selector buttons and sliders
    auto mod_selector = osu->getModSelector();
    mod_selector->resetMods();

    // Set cvars
    cv::mod_nofail.setValue(eq(mods.flags, NoFail));
    cv::mod_easy.setValue(eq(mods.flags, Easy));
    cv::mod_hidden.setValue(eq(mods.flags, Hidden));
    cv::mod_hardrock.setValue(eq(mods.flags, HardRock));
    cv::mod_flashlight.setValue(eq(mods.flags, Flashlight));
    cv::mod_suddendeath.setValue(eq(mods.flags, SuddenDeath));
    cv::mod_perfect.setValue(eq(mods.flags, Perfect));
    cv::mod_nightmare.setValue(eq(mods.flags, Nightmare));
    cv::nightcore_enjoyer.setValue(eq(mods.flags, NoPitchCorrection));
    cv::mod_touchdevice.setValue(eq(mods.flags, TouchDevice));
    cv::mod_spunout.setValue(eq(mods.flags, SpunOut));
    cv::mod_scorev2.setValue(eq(mods.flags, ScoreV2));
    cv::mod_fposu.setValue(eq(mods.flags, FPoSu));
    cv::mod_target.setValue(eq(mods.flags, Target));
    cv::ar_override_lock.setValue(eq(mods.flags, AROverrideLock));
    cv::od_override_lock.setValue(eq(mods.flags, ODOverrideLock));
    cv::mod_timewarp.setValue(eq(mods.flags, Timewarp));
    cv::mod_artimewarp.setValue(eq(mods.flags, ARTimewarp));
    cv::mod_minimize.setValue(eq(mods.flags, Minimize));
    cv::mod_jigsaw1.setValue(eq(mods.flags, Jigsaw1));
    cv::mod_jigsaw2.setValue(eq(mods.flags, Jigsaw2));
    cv::mod_wobble.setValue(eq(mods.flags, Wobble1));
    cv::mod_wobble2.setValue(eq(mods.flags, Wobble2));
    cv::mod_arwobble.setValue(eq(mods.flags, ARWobble));
    cv::mod_fullalternate.setValue(eq(mods.flags, FullAlternate));
    cv::mod_shirone.setValue(eq(mods.flags, Shirone));
    cv::mod_mafham.setValue(eq(mods.flags, Mafham));
    cv::mod_halfwindow.setValue(eq(mods.flags, HalfWindow));
    cv::mod_halfwindow_allow_300s.setValue(eq(mods.flags, HalfWindowAllow300s));
    cv::mod_ming3012.setValue(eq(mods.flags, Ming3012));
    cv::mod_no100s.setValue(eq(mods.flags, No100s));
    cv::mod_no50s.setValue(eq(mods.flags, No50s));
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
    if(eq(mods.flags, Autoplay)) {
        cv::mod_autoplay.setValue(true);
        cv::mod_autopilot.setValue(false);
        cv::mod_relax.setValue(false);
    } else {
        cv::mod_autoplay.setValue(false);
        cv::mod_autopilot.setValue(eq(mods.flags, Autopilot));
        cv::mod_relax.setValue(eq(mods.flags, Relax));
    }

    f32 speed_override = mods.speed == 1.f ? -1.f : mods.speed;
    cv::speed_override.setValue(speed_override);

    // Update mod selector UI
    mod_selector->enableModsFromFlags(mods.to_legacy());
    cv::speed_override.setValue(speed_override);  // enableModsFromFlags() edits cv::speed_override
    mod_selector->ARLock->setChecked(eq(mods.flags, AROverrideLock));
    mod_selector->ODLock->setChecked(eq(mods.flags, ODOverrideLock));
    mod_selector->speedSlider->setValue(mods.speed, false, false);
    mod_selector->CSSlider->setValue(mods.cs_override, false, false);
    mod_selector->ARSlider->setValue(mods.ar_override, false, false);
    mod_selector->ODSlider->setValue(mods.od_override, false, false);
    mod_selector->HPSlider->setValue(mods.hp_override, false, false);
    mod_selector->updateOverrideSliderLabels();
    mod_selector->updateExperimentalButtons();

    osu->updateMods();
}
}  // namespace Replay
