#include "HitSounds.h"

#include "BeatmapInterface.h"
#include "ConVar.h"
// #include "Logging.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SoundEngine.h"

i32 HitSamples::getNormalSet(std::optional<i32> play_time) {
    if(cv::skin_force_hitsound_sample_set.getInt() > 0) return cv::skin_force_hitsound_sample_set.getInt();

    if(this->normalSet != 0) return this->normalSet;

    const auto& map_iface = osu->getMapInterface();
    // Fallback to timing point sample set
    const i32 tp_sampleset = play_time.has_value()
                                 ? map_iface->getBeatmap()->getTimingInfoForTime(play_time.value()).sampleSet
                                 : map_iface->getCurrentTimingInfo().sampleSet;
    if(tp_sampleset != 0) return tp_sampleset;

    // ...Fallback to beatmap sample set
    return map_iface->getDefaultSampleSet();
}

i32 HitSamples::getAdditionSet(std::optional<i32> play_time) {
    if(cv::skin_force_hitsound_sample_set.getInt() > 0) return cv::skin_force_hitsound_sample_set.getInt();

    if(this->additionSet != 0) return this->additionSet;

    // Fallback to normal sample set
    return this->getNormalSet(play_time);
}

f32 HitSamples::getVolume(i32 hitSoundType, bool is_sliderslide, std::optional<i32> play_time) {
    f32 volume = 1.0f;

    // Some hardcoded modifiers for hitcircle sounds
    if(!is_sliderslide) {
        switch(hitSoundType) {
            case HitSoundType::NORMAL:
                volume *= 0.8f;
                break;
            case HitSoundType::WHISTLE:
                volume *= 0.85f;
                break;
            case HitSoundType::FINISH:
                volume *= 1.0f;
                break;
            case HitSoundType::CLAP:
                volume *= 0.85f;
                break;
            default:
                std::unreachable();  // unreachable
        }
    }

    if(cv::ignore_beatmap_sample_volume.getBool()) return volume;

    if(this->volume > 0) {
        volume *= (f32)this->volume / 100.0f;
    } else {
        const auto& map_iface = osu->getMapInterface();
        const auto mapTimingPointVol = play_time.has_value()
                                           ? map_iface->getBeatmap()->getTimingInfoForTime(play_time.value()).volume
                                           : map_iface->getCurrentTimingInfo().volume;
        volume *= (f32)mapTimingPointVol / 100.0f;
    }

    return volume;
}

// O(1) lookup table for sound names
// [set][is_sliderslide][hitSound]
static constexpr const i32 HIT_IDX = 0;
static constexpr const i32 SLIDER_IDX = 1;
#define A_ std::array
static constexpr auto SOUND_METHODS =  //
    A_{                                //
       // SampleSetType::NORMAL            //
       A_{//
          // HIT sounds
          A_{
              &Skin::s_normal_hitnormal,   // HitSoundType::NORMAL
              &Skin::s_normal_hitwhistle,  // HitSoundType::WHISTLE
              &Skin::s_normal_hitfinish,   // HitSoundType::FINISH
              &Skin::s_normal_hitclap      // HitSoundType::CLAP
          },
          // SLIDER sounds
          A_{
              &Skin::s_normal_sliderslide,    //
              &Skin::s_normal_sliderwhistle,  //
              (Sound* Skin::*)nullptr,        // SET-sliderfinish and SET-sliderclap aren't actually valid
              (Sound* Skin::*)nullptr         //
          }},
       // SampleSetType::SOFT
       A_{//
          // HIT sounds
          A_{
              &Skin::s_soft_hitnormal,   // ditto...
              &Skin::s_soft_hitwhistle,  //
              &Skin::s_soft_hitfinish,   //
              &Skin::s_soft_hitclap      //
          },                             //
          // SLIDER sounds
          A_{
              &Skin::s_soft_sliderslide,    //
              &Skin::s_soft_sliderwhistle,  //
              (Sound* Skin::*)nullptr,      //
              (Sound* Skin::*)nullptr       //
          }},                               //
       // SampleSetType::DRUM
       A_{//
          // HIT sounds
          A_{
              &Skin::s_drum_hitnormal,   //
              &Skin::s_drum_hitwhistle,  //
              &Skin::s_drum_hitfinish,   //
              &Skin::s_drum_hitclap      //
          },                             //
          // SLIDER sounds
          A_{
              &Skin::s_drum_sliderslide,    //
              &Skin::s_drum_sliderwhistle,  //
              (Sound* Skin::*)nullptr,      //
              (Sound* Skin::*)nullptr       //
          }}};  //
#undef A_

std::vector<HitSamples::Set_Slider_Hit> HitSamples::play(f32 pan, i32 delta, std::optional<i32> play_time,
                                                         bool is_sliderslide) {
    // Don't play hitsounds when seeking
    if(osu->getMapInterface()->bWasSeekFrame) return {};

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

    const auto* skin = osu->getMapInterface()->getSkin();

    Set_Slider_Hit potentially_played;
    std::vector<Set_Slider_Hit> played_list;

    auto get_default_sound = [&potentially_played, skin, is_sliderslide](i32 set, i32 hitSound) -> Sound* {
        // map indices
        i32 set_idx, slider_or_circle_idx, hit_idx;
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

        slider_or_circle_idx = is_sliderslide ? SLIDER_IDX : HIT_IDX;

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

        Sound* Skin::* sound_ptr = SOUND_METHODS[set_idx][slider_or_circle_idx][hit_idx];
        // debugLog("got {} for set_idx {} slider_or_circle_idx {} hit_idx {}", !!sound_ptr, set_idx, slider_or_circle_idx,
        //          hit_idx);
        if(sound_ptr != nullptr) {
            auto ret = skin->*sound_ptr;
            if(ret) {
                // debugLog("returning {}", ret->getFilePath());
                potentially_played = Set_Slider_Hit{set_idx, slider_or_circle_idx, hit_idx};
            }
            return ret;
        }

        return nullptr;
    };

    auto get_map_sound = [get_default_sound](i32 set, i32 hitSound) {
        // TODO @kiwec: map hitsounds are not supported

        return get_default_sound(set, hitSound);
    };

    auto try_play = [pan, pitch, is_sliderslide, &get_map_sound](i32 set, i32 hitSound, f32 volume) -> bool {
        auto snd = get_map_sound(set, hitSound);
        if(!snd) return false;

        if(is_sliderslide && snd->isPlaying()) return false;

        return soundEngine->play(snd, pan, pitch, volume);
    };

    namespace HT = HitSoundType;
    for(const auto type : {HT::NORMAL, HT::WHISTLE, HT::FINISH, HT::CLAP}) {
        // special case for NORMAL (play if this->hitSounds == 0 or layered hitsounds are enabled)

        // NOTE: LayeredHitSounds seems to be forced even if the map uses custom hitsounds
        //       according to https://osu.ppy.sh/community/forums/topics/15937
        if(!(this->hitSounds & type) &&
           !((type == HT::NORMAL) && ((this->hitSounds == 0) || skin->o_layered_hitsounds)))
            continue;

        const f32 volume = this->getVolume(type, is_sliderslide, play_time);
        if(volume <= 0.) continue;  // don't play silence

        const auto set = type == HT::NORMAL ? this->getNormalSet(play_time) : this->getAdditionSet(play_time);

        if(try_play(set, type, volume)) {
            played_list.push_back(potentially_played);
            potentially_played = {};
        }
    }

    return played_list;
}

void HitSamples::stop(const std::vector<Set_Slider_Hit>& specific_sets) {
    // TODO @kiwec: map hitsounds are not supported
    const auto* skin = osu->getMapInterface()->getSkin();

    // stop specified previously played sounds, otherwise stop everything
    if(!specific_sets.empty()) {
        for(const auto& triple : specific_sets) {
            assert(SOUND_METHODS[triple.set][triple.slider][triple.hit]);
            const auto& to_stop = skin->*SOUND_METHODS[triple.set][triple.slider][triple.hit];

            if(to_stop && to_stop->isPlaying()) {
                // debugLog("stopping specific set {} {} {} {}", triple.set, triple.slider, triple.hit,
                //          to_stop->getFilePath());
                soundEngine->stop(to_stop);
            }
        }
        return;
    }

    // NOTE: Timing point might have changed since the time we called play().
    //       So for now we're stopping ALL slider sounds, but in the future
    //       we'll need to store the started sounds somewhere.

    // Bruteforce approach. Will be rewritten when adding map hitsounds.
    for(const auto& sample_set : SOUND_METHODS) {
        const auto& slider_sounds = sample_set[SLIDER_IDX];
        for(const auto& slider_snd_ptr : slider_sounds) {
            if(slider_snd_ptr == nullptr) continue;  // ugly
            const auto& snd_memb = skin->*slider_snd_ptr;
            if(snd_memb != nullptr && snd_memb->isPlaying()) {
                // debugLog("stopping {}", snd_memb->getFilePath());
                soundEngine->stop(snd_memb);
            }
        }
    }
}
