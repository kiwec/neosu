// Copyright (c) 2024, kiwec, All rights reserved.
#include "LoudnessCalcThread.h"

#include "OsuConVars.h"
#include "DatabaseBeatmap.h"
#include "Database.h"
#include "Osu.h"
#include "Engine.h"
#include "Sound.h"
#include "SoundEngine.h"
#include "Thread.h"
#include "Timing.h"
#include "Logging.h"
#include "SyncOnce.h"
#include "SyncJthread.h"

#ifdef MCENGINE_FEATURE_BASS
#include "BassManager.h"
#endif

#include <atomic>
#include <utility>

struct VolNormalization::LoudnessCalcThread {
    NOCOPY_NOMOVE(LoudnessCalcThread)
   public:
    std::atomic<u32> nb_computed{0};
    std::atomic<u32> nb_total{0};
#ifdef MCENGINE_FEATURE_BASS
   public:
    LoudnessCalcThread(std::vector<DatabaseBeatmap *> maps_to_calc) {
        this->maps = std::move(maps_to_calc);
        this->nb_total = this->maps.size() + 1;
        if(soundEngine->getTypeId() == SoundEngine::BASS) {  // TODO
            this->thr = Sync::jthread([this](const Sync::stop_token &stoken) { return this->run(stoken); });
        }
    }

    ~LoudnessCalcThread() = default;

   private:
    std::vector<DatabaseBeatmap *> maps;
    Sync::jthread thr;

    void run(const Sync::stop_token &stoken) {
        McThread::set_current_thread_name(ULITERAL("loudness_calc"));
        McThread::set_current_thread_prio(McThread::Priority::NORMAL);  // reset priority

        UString last_song = "";
        f32 last_loudness = 0.f;
        std::array<f32, 44100> buf{};

        while(!BassManager::isLoaded()) {  // this should never happen, but just in case
            Timing::sleepMS(100);
        }

        BASS_SetDevice(0);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);

        for(auto map : this->maps) {
            while(osu->shouldPauseBGThreads() && !stoken.stop_requested()) {
                Timing::sleepMS(100);
            }
            Timing::sleep(1);

            if(stoken.stop_requested()) return;
            if(map->loudness.load(std::memory_order_acquire) != 0.f) continue;

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
                if(cv::debug_snd.getBool()) {
                    BassManager::printBassError(fmt::format("BASS_StreamCreateFile({:s})", song.toUtf8()),
                                                BASS_ErrorGetCode());
                }
                this->nb_computed++;
                continue;
            }

            auto loudness = BASS_Loudness_Start(decoder, BASS_LOUDNESS_INTEGRATED, 0);
            if(!loudness) {
                BassManager::printBassError("BASS_Loudness_Start()", BASS_ErrorGetCode());
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
            const bool succeeded = BASS_Loudness_GetLevel(loudness, BASS_LOUDNESS_INTEGRATED, &integrated_loudness);
            const int errc = succeeded ? 0 : BASS_ErrorGetCode();

            BASS_Loudness_Stop(loudness);

            if(!succeeded || integrated_loudness == -HUGE_VAL) {
                debugLog("No loudness information available for '{:s}' {}", song.toUtf8(),
                         !succeeded ? BassManager::getErrorString(errc) : "(silent song?)");

                integrated_loudness = std::clamp<f32>(cv::loudness_fallback.getFloat(), -16.f, 0.f);
            }

            // it seems safe to assume integrated_loudness == -HUGE_VAL means it's actually silent
            // so store it (with loudness_fallback), so we don't constantly re-calculate silent tracks
            if(succeeded) {
                map->loudness = integrated_loudness;
                db->update_overrides(map);
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

void VolNormalization::loudness_cb() {
    // Restart loudness calc.
    VolNormalization::abort();
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return;  // TODO
    if(db && cv::normalize_loudness.getBool()) {
        VolNormalization::start_calc(db->loudness_to_calc);
    }
}

u32 VolNormalization::get_computed_instance() {
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return 0;  // TODO
    u32 x = 0;
    for(const auto &thr : this->threads) {
        x += thr->nb_computed.load(std::memory_order_acquire);
    }
    return x;
}

u32 VolNormalization::get_total_instance() {
    if(!Env::cfg(AUD::BASS) || soundEngine->getTypeId() != SoundEngine::BASS) return 0;  // TODO
    u32 x = 0;
    for(const auto &thr : this->threads) {
        x += thr->nb_total.load(std::memory_order_acquire);
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
        nb_threads = std::max(McThread::get_logical_cpu_count() / 3, 1);
    }
    if(maps_to_calc.size() < nb_threads) nb_threads = maps_to_calc.size();
    int chunk_size = maps_to_calc.size() / nb_threads;
    int remainder = maps_to_calc.size() % nb_threads;

    auto it = maps_to_calc.begin();
    for(int i = 0; i < nb_threads; i++) {
        int cur_chunk_size = chunk_size + (i < remainder ? 1 : 0);

        auto chunk = std::vector<DatabaseBeatmap *>(it, it + cur_chunk_size);
        it += cur_chunk_size;

        this->threads.emplace_back(std::make_unique<LoudnessCalcThread>(chunk));
    }
}

void VolNormalization::abort_instance() { this->threads.clear(); }

VolNormalization::~VolNormalization() {
    cv::loudness_calc_threads.removeAllCallbacks();
    // only clean up this instance's resources
    abort_instance();
}

VolNormalization &VolNormalization::get_instance() {
    static VolNormalization instance;
    static Sync::once_flag once;

    Sync::call_once(once, []() { cv::loudness_calc_threads.setCallback(CFUNC(VolNormalization::loudness_cb)); });

    return instance;
}
