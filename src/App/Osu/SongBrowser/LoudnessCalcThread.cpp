// Copyright (c) 2024, kiwec, All rights reserved.
#include "LoudnessCalcThread.h"

#include <atomic>
#include <utility>

#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Sound.h"
#include "SoundEngine.h"
#include "Thread.h"

#ifdef MCENGINE_FEATURE_BASS
#include "BassManager.h"
#endif

// static member definitions
std::unique_ptr<VolNormalization> VolNormalization::instance = nullptr;
std::once_flag VolNormalization::instance_flag;
std::once_flag VolNormalization::shutdown_flag;

struct VolNormalization::LoudnessCalcThread {
    NOCOPY_NOMOVE(LoudnessCalcThread)
   public:
    std::thread thr;
    std::atomic<bool> dead{true};
    std::vector<DatabaseBeatmap *> maps;

    std::atomic<u32> nb_computed{0};
    std::atomic<u32> nb_total{0};
#ifdef MCENGINE_FEATURE_BASS
   public:
    LoudnessCalcThread(std::vector<DatabaseBeatmap *> maps_to_calc) {
        this->dead = false;
        this->maps = std::move(maps_to_calc);
        this->nb_total = this->maps.size() + 1;
        if(soundEngine->getTypeId() == SoundEngine::BASS) {  // TODO
            this->thr = std::thread(&LoudnessCalcThread::run, this);
        }
    }

    ~LoudnessCalcThread() {
        this->dead = true;
        if(this->thr.joinable()) {
            this->thr.join();
        }
    }

   private:
    void run() {
        McThread::set_current_thread_name("loudness_calc");
        McThread::set_current_thread_prio(false); // reset priority

        UString last_song = "";
        f32 last_loudness = 0.f;
        std::array<f32, 44100> buf{};

        while(!BassManager::isLoaded()) {  // this should never happen, but just in case
            Timing::sleepMS(100);
        }

        BASS_SetDevice(0);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

        for(auto map : this->maps) {
            while(osu->should_pause_background_threads.load() && !this->dead.load()) {
                Timing::sleepMS(100);
            }
            Timing::sleep(1);

            if(this->dead.load()) return;
            if(map->loudness.load() != 0.f) continue;

            UString song{map->getFullSoundFilePath()};
            if(song == last_song) {
                map->loudness = last_loudness;
                this->nb_computed++;
                continue;
            }

            constexpr unsigned int flags =
                BASS_STREAM_DECODE | BASS_SAMPLE_MONO | (Env::cfg(OS::WINDOWS) ? BASS_UNICODE : 0U);
            auto decoder = BASS_StreamCreateFile(BASS_FILE_NAME, song.plat_str(), 0, 0, flags);
            if(!decoder) {
                auto err_str = BassManager::getErrorUString();
                debugLog("BASS_StreamCreateFile({:s}): {:s}\n", song.toUtf8(), err_str.toUtf8());
                this->nb_computed++;
                continue;
            }

            auto loudness = BASS_Loudness_Start(decoder, BASS_LOUDNESS_INTEGRATED, 0);
            if(!loudness) {
                auto err_str = BassManager::getErrorUString();
                debugLog("BASS_Loudness_Start(): {:s}\n", err_str.toUtf8());
                BASS_ChannelFree(decoder);
                this->nb_computed++;
                continue;
            }

            // Did you know?
            // If we do while(BASS_ChannelGetData(decoder, buf, sizeof(buf) >= 0), we get an infinite loop!
            // Thanks, Microsoft!
            int c;
            do {
                c = BASS_ChannelGetData(decoder, buf.data(), buf.size());
            } while(c >= 0);

            BASS_ChannelFree(decoder);

            f32 integrated_loudness = 0.f;
            BASS_Loudness_GetLevel(loudness, BASS_LOUDNESS_INTEGRATED, &integrated_loudness);
            BASS_Loudness_Stop(loudness);
            if(integrated_loudness == -HUGE_VAL) {
                debugLog("No loudness information available for '{:s}' (silent song?)\n", song.toUtf8());
            } else {
                map->loudness = integrated_loudness;
                map->update_overrides();
                last_loudness = integrated_loudness;
                last_song = song;
            }

            this->nb_computed++;
        }

        this->nb_computed++;
    }
#else  // TODO:
    LoudnessCalcThread(std::vector<DatabaseBeatmap *> maps_to_calc) { (void)maps_to_calc; }
#endif
};

u32 VolNormalization::get_computed_instance() {
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return 0;  // TODO
    u32 x = 0;
    for(auto thr : this->threads) {
        x += thr->nb_computed.load();
    }
    return x;
}

u32 VolNormalization::get_total_instance() {
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return 0;  // TODO
    u32 x = 0;
    for(auto thr : this->threads) {
        x += thr->nb_total.load();
    }
    return x;
}

void VolNormalization::start_calc_instance(const std::vector<DatabaseBeatmap *> &maps_to_calc) {
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return;  // TODO
    this->abort_instance();
    if(maps_to_calc.empty()) return;
    if(!cv::normalize_loudness.getBool()) return;

    i32 nb_threads = cv::loudness_calc_threads.getInt();
    if(nb_threads <= 0) {
        // dividing by 2 still burns cpu if hyperthreading is enabled, let's keep it at a sane amount of threads
        nb_threads = std::max(std::thread::hardware_concurrency() / 3, 1u);
    }
    if(maps_to_calc.size() < nb_threads) nb_threads = maps_to_calc.size();
    int chunk_size = maps_to_calc.size() / nb_threads;
    int remainder = maps_to_calc.size() % nb_threads;

    auto it = maps_to_calc.begin();
    for(int i = 0; i < nb_threads; i++) {
        int cur_chunk_size = chunk_size + (i < remainder ? 1 : 0);

        auto chunk = std::vector<DatabaseBeatmap *>(it, it + cur_chunk_size);
        it += cur_chunk_size;

        auto lct = new LoudnessCalcThread(chunk);
        this->threads.push_back(lct);
    }
}

void VolNormalization::abort_instance() {
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return;  // TODO
    for(auto thr : this->threads) {
        delete thr;
    }
    this->threads.clear();
}

VolNormalization &VolNormalization::get_instance() {
    std::call_once(instance_flag, []() { instance = std::make_unique<VolNormalization>(); });
    return *instance;
}

VolNormalization *VolNormalization::get_instance_ptr() {
    // return existing instance without creating it
    return instance.get();
}
