#include "HitSounds.h"

#include "BeatmapInterface.h"
#include "ConVar.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SoundEngine.h"

i32 HitSamples::getNormalSet() {
    if(cv::skin_force_hitsound_sample_set.getInt() > 0) return cv::skin_force_hitsound_sample_set.getInt();

    if(this->normalSet != 0) return this->normalSet;

    // Fallback to timing point sample set
    i32 tp_sampleset = osu->getMapInterface()->getTimingPoint().sampleSet;
    if(tp_sampleset != 0) return tp_sampleset;

    // ...Fallback to beatmap sample set
    return osu->getMapInterface()->getDefaultSampleSet();
}

i32 HitSamples::getAdditionSet() {
    if(cv::skin_force_hitsound_sample_set.getInt() > 0) return cv::skin_force_hitsound_sample_set.getInt();

    if(this->additionSet != 0) return this->additionSet;

    // Fallback to normal sample set
    return this->getNormalSet();
}

f32 HitSamples::getVolume(i32 hitSoundType, bool is_sliderslide) {
    f32 volume = 1.0;

    // Some hardcoded modifiers for hitcircle sounds
    if(!is_sliderslide) {
        switch(hitSoundType) {
            case HitSoundType::NORMAL:
                volume *= 0.8;
                break;
            case HitSoundType::WHISTLE:
                volume *= 0.85;
                break;
            case HitSoundType::FINISH:
                volume *= 1.0;
                break;
            case HitSoundType::CLAP:
                volume *= 0.85;
                break;
            default:
                assert(false);  // unreachable
        }
    }

    if(cv::ignore_beatmap_sample_volume.getBool()) return volume;

    if(this->volume > 0) {
        volume *= (f32)this->volume / 100.0;
    } else {
        volume *= (f32)osu->getMapInterface()->getTimingPoint().volume / 100.0;
    }

    return volume;
}

void HitSamples::play(f32 pan, i32 delta, bool is_sliderslide) {
    // Don't play hitsounds when seeking
    if(osu->getMapInterface()->bWasSeekFrame) return;

    if(!cv::sound_panning.getBool() || (cv::mod_fposu.getBool() && !cv::mod_fposu_sound_panning.getBool()) ||
       (cv::mod_fps.getBool() && !cv::mod_fps_sound_panning.getBool())) {
        pan = 0.0f;
    } else {
        pan *= cv::sound_panning_multiplier.getFloat();
    }

    f32 pitch = 0.f;
    if(cv::snd_pitch_hitsounds.getBool()) {
        f32 range = osu->getMapInterface()->getHitWindow100();
        pitch = (f32)delta / range * cv::snd_pitch_hitsounds_factor.getFloat();
    }

    // O(1) lookup table for sound names
    // [set][is_sliderslide][hitSound]
#define A_ std::array
    static constexpr const auto SOUND_NAMES =
        A_{   // SampleSetType::NORMAL
           A_{// HIT sounds
              A_{
                  "SKIN_NORMALHITNORMAL_SND",   // HitSoundType::NORMAL
                  "SKIN_NORMALHITWHISTLE_SND",  // HitSoundType::WHISTLE
                  "SKIN_NORMALHITFINISH_SND",   // HitSoundType::FINISH
                  "SKIN_NORMALHITCLAP_SND"      // HitSoundType::CLAP
              },
              // SLIDER sounds
              A_{
                  "SKIN_NORMALSLIDERSLIDE_SND",    // HitSoundType::NORMAL
                  "SKIN_NORMALSLIDERWHISTLE_SND",  // HitSoundType::WHISTLE
                  "SKIN_NORMALSLIDERFINISH_SND",   // HitSoundType::FINISH
                  "SKIN_NORMALSLIDERCLAP_SND"      // HitSoundType::CLAP
              }},
           // SampleSetType::SOFT
           A_{// HIT sounds
              A_{"SKIN_SOFTHITNORMAL_SND", "SKIN_SOFTHITWHISTLE_SND", "SKIN_SOFTHITFINISH_SND", "SKIN_SOFTHITCLAP_SND"},
              // SLIDER sounds
              A_{"SKIN_SOFTSLIDERSLIDE_SND", "SKIN_SOFTSLIDERWHISTLE_SND", "SKIN_SOFTSLIDERFINISH_SND",
                 "SKIN_SOFTSLIDERCLAP_SND"}},
           // SampleSetType::DRUM
           A_{// HIT sounds
              A_{"SKIN_DRUMHITNORMAL_SND", "SKIN_DRUMHITWHISTLE_SND", "SKIN_DRUMHITFINISH_SND", "SKIN_DRUMHITCLAP_SND"},
              // SLIDER sounds
              A_{"SKIN_DRUMSLIDERSLIDE_SND", "SKIN_DRUMSLIDERWHISTLE_SND", "SKIN_DRUMSLIDERFINISH_SND",
                 "SKIN_DRUMSLIDERCLAP_SND"}}};
#undef A_

    auto get_default_sound = [is_sliderslide](i32 set, i32 hitSound) -> Sound* {
        // map indices
        u8 set_idx, slider_idx, hit_idx;
        switch(set) {
            default:
            case SampleSetType::NORMAL:
                set_idx = 0;
                break;
            case SampleSetType::SOFT:
                set_idx = 1;
                break;
            case SampleSetType::DRUM:
                set_idx = 2;
                break;
        }

        if(is_sliderslide) {
            slider_idx = 1;
        } else {
            slider_idx = 0;
        }

        switch(hitSound) {
            default:
            case HitSoundType::NORMAL:
                hit_idx = 0;
                break;
            case HitSoundType::WHISTLE:
                hit_idx = 1;
                break;
            case HitSoundType::FINISH:
                hit_idx = 2;
                break;
            case HitSoundType::CLAP:
                hit_idx = 3;
                break;
        }

        return resourceManager->getSound(SOUND_NAMES[set_idx][slider_idx][hit_idx]);
    };

    auto get_map_sound = [get_default_sound](i32 set, i32 hitSound) {
        // TODO @kiwec: map hitsounds are not supported

        return get_default_sound(set, hitSound);
    };

    auto try_play = [&](i32 set, i32 hitSound) {
        auto snd = get_map_sound(set, hitSound);
        if(!snd) return;

        f32 volume = this->getVolume(hitSound, is_sliderslide);
        if(volume == 0.0) return;

        if(is_sliderslide && snd->isPlaying()) return;

        soundEngine->play(snd, pan, pitch, volume);
    };

    // NOTE: osu->getSkin()->getLayeredHitSounds() seems to be forced even if the map uses custom hitsounds
    //       according to https://osu.ppy.sh/community/forums/topics/15937
    if((this->hitSounds & HitSoundType::NORMAL) || (this->hitSounds == 0) || osu->getSkin()->getLayeredHitSounds()) {
        try_play(this->getNormalSet(), HitSoundType::NORMAL);
    }

    if(this->hitSounds & HitSoundType::WHISTLE) {
        try_play(this->getAdditionSet(), HitSoundType::WHISTLE);
    }

    if(this->hitSounds & HitSoundType::FINISH) {
        try_play(this->getAdditionSet(), HitSoundType::FINISH);
    }

    if(this->hitSounds & HitSoundType::CLAP) {
        try_play(this->getAdditionSet(), HitSoundType::CLAP);
    }
}

void HitSamples::stop() {
    // TODO @kiwec: map hitsounds are not supported

    // NOTE: Timing point might have changed since the time we called play().
    //       So for now we're stopping ALL slider sounds, but in the future
    //       we'll need to store the started sounds somewhere.

    // Bruteforce approach. Will be rewritten when adding map hitsounds.
    auto sets = {"NORMAL", "SOFT", "DRUM"};
    auto types = {"SLIDE", "WHISTLE"};
    for(auto& set : sets) {
        for(auto& type : types) {
            std::string sound_name = "SKIN_";
            sound_name.append(set);
            sound_name.append("SLIDER");
            sound_name.append(type);
            sound_name.append("_SND");

            auto sound = resourceManager->getSound(sound_name);
            if(sound) soundEngine->stop(sound);
        }
    }
}
