#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "BeatmapInterface.h"
#include "ConVar.h"
#include "Osu.h"

class GameRules {
   public:
    //********************//
    //  Positional Audio  //
    //********************//

    static forceinline float osuCoords2Pan(float x) { return (x / (float)GameRules::OSU_COORD_WIDTH - 0.5f) * 0.8f; }

    //************************//
    //	Hitobject Animations  //
    //************************//

    // this scales the fadeout duration with the current speed multiplier
    static inline float getFadeOutTime() {
        const float fade_out_time = cv::hitobject_fade_out_time.getFloat();
        const float multiplier_min = cv::hitobject_fade_out_time_speed_multiplier_min.getFloat();
        return fade_out_time * (1.0f / std::max(osu->getAnimationSpeedMultiplier(), multiplier_min));
    }

    static inline i32 getFadeInTime() { return (i32)cv::hitobject_fade_in_time.getInt(); }

    //********************//
    //	Hitobject Timing  //
    //********************//

    static constexpr float getMinHitWindow300() { return 80.f; }
    static constexpr float getMidHitWindow300() { return 50.f; }
    static constexpr float getMaxHitWindow300() { return 20.f; }

    static constexpr float getMinHitWindow100() { return 140.f; }
    static constexpr float getMidHitWindow100() { return 100.f; }
    static constexpr float getMaxHitWindow100() { return 60.f; }

    static constexpr float getMinHitWindow50() { return 200.f; }
    static constexpr float getMidHitWindow50() { return 150.f; }
    static constexpr float getMaxHitWindow50() { return 100.f; }

    static constexpr float getHitWindowMiss() { return 400.f; }

    // respect mods and overrides
    static inline float getMinApproachTime() {
        return cv::approachtime_min.getFloat() *
               (cv::mod_millhioref.getBool() ? cv::mod_millhioref_multiplier.getFloat() : 1.0f);
    }
    static inline float getMidApproachTime() {
        return cv::approachtime_mid.getFloat() *
               (cv::mod_millhioref.getBool() ? cv::mod_millhioref_multiplier.getFloat() : 1.0f);
    }
    static inline float getMaxApproachTime() {
        return cv::approachtime_max.getFloat() *
               (cv::mod_millhioref.getBool() ? cv::mod_millhioref_multiplier.getFloat() : 1.0f);
    }

    // AR 5 -> 1200 ms
    template <typename T>
    static forceinline T mapDifficultyRange(T scaledDiff, T min, T mid, T max) {
        if(scaledDiff == 5.0f)
            return mid;
        else if(scaledDiff > 5.0f)
            return mid + (max - mid) * (scaledDiff - 5.0f) / 5.0f;
        else
            return mid - (mid - min) * (5.0f - scaledDiff) / 5.0f;
    }
    static inline INLINE_BODY float arToMilliseconds(float AR) {
        return mapDifficultyRange(AR, cv::approachtime_min.getFloat(), cv::approachtime_mid.getFloat(),
                                  cv::approachtime_max.getFloat());
    }
    static inline INLINE_BODY float odTo50HitWindowMS(float OD) {
        return mapDifficultyRange(OD, getMinHitWindow50(), getMidHitWindow50(), getMaxHitWindow50());
    }
    static inline INLINE_BODY float odTo100HitWindowMS(float OD) {
        return mapDifficultyRange(OD, getMinHitWindow100(), getMidHitWindow100(), getMaxHitWindow100());
    }
    static inline INLINE_BODY float odTo300HitWindowMS(float OD) {
        return mapDifficultyRange(OD, getMinHitWindow300(), getMidHitWindow300(), getMaxHitWindow300());
    }

    // 1200 ms -> AR 5
    static forceinline float mapDifficultyRangeInv(float val, float min, float mid, float max) {
        if(val == mid)
            return 5.0f;
        else if(val < mid)  // > 5.0f (inverted)
            return ((val * 5.0f - mid * 5.0f) / (max - mid)) + 5.0f;
        else  // < 5.0f (inverted)
            return 5.0f - ((mid * 5.0f - val * 5.0f) / (mid - min));
    }

    // AR 9, speed 1.5 -> AR 10.3
    static inline INLINE_BODY float arWithSpeed(float AR, float speed) {
        float approachTime = arToMilliseconds(AR);
        return mapDifficultyRangeInv(approachTime / speed, cv::approachtime_min.getFloat(),
                                     cv::approachtime_mid.getFloat(), cv::approachtime_max.getFloat());
    }

    // OD 9, speed 1.5 -> OD 10.4
    static inline INLINE_BODY float odWithSpeed(float OD, float speed) {
        float hittableTime = odTo300HitWindowMS(OD);
        return mapDifficultyRangeInv(hittableTime / speed, getMinHitWindow300(), getMidHitWindow300(),
                                     getMaxHitWindow300());
    }

    static inline INLINE_BODY float getApproachTimeForStacking(float AR) {
        return mapDifficultyRange(AR, getMinApproachTime(), getMidApproachTime(), getMaxApproachTime());
    }

    // raw spins required per second
    static inline INLINE_BODY float getSpinnerSpinsPerSecond(AbstractBeatmapInterface *beatmap) {
        return mapDifficultyRange(beatmap->getOD(), 3.0f, 5.0f, 7.5f);
    }

    static inline INLINE_BODY float getSpinnerRotationsForSpeedMultiplier(AbstractBeatmapInterface *beatmap,
                                                                          i32 spinnerDuration, float speedMultiplier) {
        /// return (int)((float)spinnerDuration / 1000.0f * getSpinnerSpinsPerSecond(beatmap)); // actual
        return (int)((((float)spinnerDuration / 1000.0f * getSpinnerSpinsPerSecond(beatmap)) * 0.5f) *
                     (std::min(1.0f / speedMultiplier, 1.0f)));  // Mc
    }

    // spinner length compensated rotations
    // respect all mods and overrides
    static inline INLINE_BODY float getSpinnerRotationsForSpeedMultiplier(AbstractBeatmapInterface *beatmap,
                                                                          i32 spinnerDuration) {
        return getSpinnerRotationsForSpeedMultiplier(beatmap, spinnerDuration, beatmap->getSpeedMultiplier());
    }

    //*********************//
    //	Hitobject Scaling  //
    //*********************//

    // "Builds of osu! up to 2013-05-04 had the gamefield being rounded down, which caused incorrect radius calculations
    // in widescreen cases. This ratio adjusts to allow for old replays to work post-fix, which in turn increases the
    // lenience for all plays, but by an amount so small it should only be effective in replays."
    static constexpr const float broken_gamefield_rounding_allowance = 1.00041f;

    static forceinline f32 getRawHitCircleScale(f32 CS) {
        return std::max(0.0f, ((1.0f - 0.7f * (CS - 5.0f) / 5.0f) / 2.0f) * broken_gamefield_rounding_allowance);
    }

    // gives the circle diameter in osu!pixels, goes negative above CS 12.1429
    static forceinline f32 getRawHitCircleDiameter(f32 CS) { return getRawHitCircleScale(CS) * 128.0f; }

    // scales osu!pixels to the actual playfield size
    static forceinline f32 getHitCircleXMultiplier() { return getPlayfieldSize().x / OSU_COORD_WIDTH; }

    //*************//
    //	Playfield  //
    //*************//

    static constexpr const int OSU_COORD_WIDTH = 512;
    static constexpr const int OSU_COORD_HEIGHT = 384;

    static forceinline float getPlayfieldScaleFactor() {
        const float &osu_screen_width = osu->getVirtScreenSize().x;
        const float &osu_screen_height = osu->getVirtScreenSize().y;
        const float top_border_size = cv::playfield_border_top_percent.getFloat() * osu_screen_height;
        const float bottom_border_size = cv::playfield_border_bottom_percent.getFloat() * osu_screen_height;

        const float adjusted_playfield_height = osu_screen_height - bottom_border_size - top_border_size;

        return (osu_screen_width / (float)OSU_COORD_WIDTH) > (adjusted_playfield_height / (float)OSU_COORD_HEIGHT)
                   ? (adjusted_playfield_height / (float)OSU_COORD_HEIGHT)
                   : (osu_screen_width / (float)OSU_COORD_WIDTH);
    }

    static forceinline vec2 getPlayfieldSize() {
        const float scaleFactor = getPlayfieldScaleFactor();

        return {(float)OSU_COORD_WIDTH * scaleFactor, (float)OSU_COORD_HEIGHT * scaleFactor};
    }

    static inline vec2 getPlayfieldOffset() {
        const float &osu_screen_width = osu->getVirtScreenSize().x;
        const float &osu_screen_height = osu->getVirtScreenSize().y;
        const vec2 playfield_size = getPlayfieldSize();
        const float bottom_border_size = cv::playfield_border_bottom_percent.getFloat() * osu_screen_height;

        // first person mode doesn't need any offsets, cursor/crosshair should be centered on screen
        const float playfield_y_offset =
            cv::mod_fps.getBool() ? 0.f : (osu_screen_height / 2.0f - (playfield_size.y / 2.0f)) - bottom_border_size;

        return {(osu_screen_width - playfield_size.x) / 2.0f,
                (osu_screen_height - playfield_size.y) / 2.0f + playfield_y_offset};
    }

    static inline vec2 getPlayfieldCenter() {
        const float scaleFactor = getPlayfieldScaleFactor();
        const vec2 playfieldOffset = getPlayfieldOffset();

        return {(OSU_COORD_WIDTH / 2.f) * scaleFactor + playfieldOffset.x,
                (OSU_COORD_HEIGHT / 2.f) * scaleFactor + playfieldOffset.y};
    }
};
