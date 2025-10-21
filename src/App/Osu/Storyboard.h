#pragma once
// Copyright (c) 2025, kiwec, All rights reserved.

// Parsing order:
// - all .osb files in the beatmapset directory, ordered alphabetically (?)
// - [Events] in .osu file
// events are appended, always

namespace Easing {
enum {
    Linear = 0,
    EasingOut,
    EasingIn,
    QuadIn,
    QuadOut,
    QuadInOut,
    CubicIn,
    CubicOut,
    CubicInOut,
    QuartIn,
    QuartOut,
    QuartInOut,
    QuintIn,
    QuintOut,
    QuintInOut,
    SineIn,
    SineOut,
    SineInOut,
    ExpoIn,
    ExpoOut,
    ExpoInOut,
    CircIn,
    CircOut,
    CircInOut,
    ElasticIn,
    ElasticOut,
    ElasticHalfOut,
    ElasticQuarterOut,
    ElasticInOut,
    BackIn,
    BackOut,
    BackInOut,
    BounceIn,
    BounceOut,
    BounceInOut,

    NB_EASINGS,
};
}

enum class Trigger {
    Hitsound,  // a hitsound is played (filtered by hitsound type, or not)
    Passing,   // (transition from fail state to pass state)
    Failing,   // (transition from pass state to fail state)
};

// NOTE: also see command quirks https://osu.ppy.sh/wiki/en/Storyboard/Scripting/Shorthand
// NOTE: underscore can also be a space!
enum class StoryboardCommand {
    // _F,(easing),(starttime),(endtime),(start_opacity),(end_opacity)
    // NOTE: opacity is floating point between 0 and 1
    FADE,

    // _M,(easing),(starttime),(endtime),(start_x),(start_y),(end_x),(end_y)
    MOVE,

    // _MX,(easing),(starttime),(endtime),(start_x),(end_x)
    MOVE_X,

    // _MY,(easing),(starttime),(endtime),(start_y),(end_y)
    MOVE_Y,

    // _S,(easing),(starttime),(endtime),(start_scale),(end_scale)
    // NOTE: scale is floating point between 0 and infinite
    // NOTE: scaling is affected by its origin point
    SCALE,

    // _V,(easing),(starttime),(endtime),(start_scale_x),(start_scale_y),(end_scale_x),(end_scale_y)
    // XXX: Just merge this one with SCALE
    VECTOR_SCALE,

    // _R,(easing),(starttime),(endtime),(start_rotate),(end_rotate)
    // NOTE: in radians
    ROTATE,

    // _C,(easing),(starttime),(endtime),(start_r),(start_g),(start_b),(end_r),(end_g),(end_b)
    // NOTE: 0-255
    COLOR,

    // _P,(easing),(starttime),(endtime),(parameter)
    // "H" - flip the image horizontally
    // "V" - flip the image vertically
    // "A" - use additive-colour blending instead of alpha-blending
    // NOTE: Unlike other commands, these only apply while the parameter is active
    FLIP_HORIZONTALLY,
    FLIP_VERTICALLY,
    USE_ADDITIVE_COLOR_BLENDING,

    // _L,(starttime),(loopcount)
    // __(event),(easing),(relative_starttime),(relative_endtime),(params...)
    // ...more events
    // NOTE: (relative_starttime) and (relative_endtime) are relative to the start of the current loop iteration
    LOOP,

    // _T,(triggerType),(starttime),(endtime)
    // __(event),(easing),(relative_starttime),(relative_endtime),(params...)
    // ...more events
    TRIGGER,
};

enum class StoryboardLayer {
    BACKGROUND,
    FAIL,
    PASS,
    FOREGROUND,
};

enum class StoryboardOrigin {
    TopLeft,
    Centre,
    CentreLeft,
    TopRight,
    BottomCentre,
    TopCentre,
    Custom = TopLeft,
    CentreRight,
    BottomLeft,
    BottomRight,
};

struct StoryboardState {
    MD5Hash md5; // TODO
    bool use_skin_sprites = false;
    bool widescreen = false;

    // Below: can be changed with gameplay

    // https://osu.ppy.sh/wiki/en/Storyboard/Scripting/General_Rules#game-state
    // Before playtime: PASS always
    // During playtime: PASS if this is the first color combo or previous color combo was all 300s
    // During breaks: PASS if player got OK animation (eg. health was above 50%)
    // After playtime, if at least 1 break: >50% of breakts were PASSes
    // After playtime, if there were no breaks: health above 50%
    bool pass = true;
}

struct StoryboardSprite {
    // Can be initialized by either:

    // Sprite,(layer),(origin),"(filepath)",(x),(y)
    // NOTE: Quotes around filepath are optional

    // Animation,(layer),(origin),"(filepath)",(x),(y),(frameCount),(frameDelay),(looptype)
    // NOTE: Filepath will be "some/thing.png", but animation will be "some/thing0.png", "some/thing1.png" etc
    // NOTE: see https://osu.ppy.sh/wiki/en/Storyboard/Scripting/osu%21_File_Toggles

    StoryboardLayer layer;
    StoryboardOrigin origin;

    Image* images;
    u32 nb_images;
    u32 ms_between_frames;
    bool looping;

    f64 x;
    f64 y;

    // A storyboard element stays active until its last event ends.
};

struct StoryboardSound {
    // Sample,(time),(layer_num),"(filepath)",(volume)
    // NOTE: see https://osu.ppy.sh/wiki/en/Storyboard/Scripting/osu%21_File_Toggles
    u32 tms;
    StoryboardLayer layer;
    Sound* sound;
    u32 volume;  // NOTE: 1-100, 100 by default
};

struct StoryboardEvent {
    // _(event),(easing),(starttime),(endtime),(params...)
    // NOTE: '_' can be replaced by a space
    StoryboardSprite* sprite;
    StoryboardCommand command;
    Easing easing;
    u32 start_time;
    u32 end_time;
};
