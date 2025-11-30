// Copyright (c) 2016, PG, All rights reserved.

#include "GameRules.h"
#include "AbstractBeatmapInterface.h"
#include "Osu.h"
#include "OsuConVars.h"

float GameRules::getFadeOutTime() {
    const float fade_out_time = cv::hitobject_fade_out_time.getFloat();
    const float multiplier_min = cv::hitobject_fade_out_time_speed_multiplier_min.getFloat();
    return fade_out_time * (1.0f / std::max(osu ? osu->getAnimationSpeedMultiplier() : 1.f, multiplier_min));
}

i32 GameRules::getFadeInTime() { return (i32)cv::hitobject_fade_in_time.getInt(); }

float GameRules::getMinApproachTime() {
    return cv::approachtime_min.getFloat() *
           (cv::mod_millhioref.getBool() ? cv::mod_millhioref_multiplier.getFloat() : 1.0f);
}

float GameRules::getMidApproachTime() {
    return cv::approachtime_mid.getFloat() *
           (cv::mod_millhioref.getBool() ? cv::mod_millhioref_multiplier.getFloat() : 1.0f);
}

float GameRules::getMaxApproachTime() {
    return cv::approachtime_max.getFloat() *
           (cv::mod_millhioref.getBool() ? cv::mod_millhioref_multiplier.getFloat() : 1.0f);
}

float GameRules::arToMilliseconds(float AR) {
    return mapDifficultyRange(AR, cv::approachtime_min.getFloat(), cv::approachtime_mid.getFloat(),
                              cv::approachtime_max.getFloat());
}

float GameRules::arWithSpeed(float AR, float speed) {
    float approachTime = arToMilliseconds(AR);
    return mapDifficultyRangeInv(approachTime / speed, cv::approachtime_min.getFloat(), cv::approachtime_mid.getFloat(),
                                 cv::approachtime_max.getFloat());
}

// raw spins required per second
float GameRules::getSpinnerSpinsPerSecond(const AbstractBeatmapInterface *beatmap) {
    return mapDifficultyRange(beatmap->getOD(), 3.0f, 5.0f, 7.5f);
}

// spinner length compensated rotations
// respect all mods and overrides
float GameRules::getSpinnerRotationsForSpeedMultiplier(const AbstractBeatmapInterface *beatmap, i32 spinnerDuration) {
    return getSpinnerRotationsForSpeedMultiplier(beatmap, spinnerDuration, beatmap->getSpeedMultiplier());
}

vec2 GameRules::getPlayfieldOffset() {
    const vec2 res = osu ? osu->getVirtScreenSize() : Osu::osuBaseResolution;

    const float osu_screen_width = res.x;
    const float osu_screen_height = res.y;
    const vec2 playfield_size = getPlayfieldSize();
    const float bottom_border_size = cv::playfield_border_bottom_percent.getFloat() * osu_screen_height;

    // first person mode doesn't need any offsets, cursor/crosshair should be centered on screen
    const float playfield_y_offset =
        cv::mod_fps.getBool() ? 0.f : (osu_screen_height / 2.0f - (playfield_size.y / 2.0f)) - bottom_border_size;

    return {(osu_screen_width - playfield_size.x) / 2.0f,
            (osu_screen_height - playfield_size.y) / 2.0f + playfield_y_offset};
}

float GameRules::getPlayfieldScaleFactor() {
    const vec2 res = osu ? osu->getVirtScreenSize() : Osu::osuBaseResolution;

    const float osu_screen_width = res.x;
    const float osu_screen_height = res.y;
    const float top_border_size = cv::playfield_border_top_percent.getFloat() * osu_screen_height;
    const float bottom_border_size = cv::playfield_border_bottom_percent.getFloat() * osu_screen_height;

    const float adjusted_playfield_height = osu_screen_height - bottom_border_size - top_border_size;

    return (osu_screen_width / (float)OSU_COORD_WIDTH) > (adjusted_playfield_height / (float)OSU_COORD_HEIGHT)
               ? (adjusted_playfield_height / (float)OSU_COORD_HEIGHT)
               : (osu_screen_width / (float)OSU_COORD_WIDTH);
}
