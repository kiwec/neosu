// Copyright (c) 2012, PG, All rights reserved.
#ifndef ANIMATIONHANDLER_H
#define ANIMATIONHANDLER_H

#include "types.h"

namespace AnimationHandler {

// called by engine once per frame, after updating time
void update();
void clearAll();  // called when shutting down, for safety

// base
void moveLinear(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveQuadIn(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveQuadOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveQuadInOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveCubicIn(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveCubicOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveQuartIn(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);
void moveQuartOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting = false);

// simplified, without delay
inline void moveLinear(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveLinear(base, target, duration, 0.0f, overrideExisting);
}
inline void moveQuadIn(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveQuadIn(base, target, duration, 0.0f, overrideExisting);
}
inline void moveQuadOut(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveQuadOut(base, target, duration, 0.0f, overrideExisting);
}
inline void moveQuadInOut(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveQuadInOut(base, target, duration, 0.0f, overrideExisting);
}
inline void moveCubicIn(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveCubicIn(base, target, duration, 0.0f, overrideExisting);
}
inline void moveCubicOut(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveCubicOut(base, target, duration, 0.0f, overrideExisting);
}
inline void moveQuartIn(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveQuartIn(base, target, duration, 0.0f, overrideExisting);
}
inline void moveQuartOut(f32 *base, f32 target, f32 duration, bool overrideExisting = false) {
    moveQuartOut(base, target, duration, 0.0f, overrideExisting);
}

// DEPRECATED:
void moveSmoothEnd(f32 *base, f32 target, f32 duration, f32 smoothFactor = 20.f, f32 delay = 0.0f);

void deleteExistingAnimation(f32 *base);

[[nodiscard]] f32 getRemainingDuration(f32 *base);

[[nodiscard]] bool isAnimating(f32 *base);

[[nodiscard]] uSz getNumActiveAnimations();
}  // namespace AnimationHandler

namespace anim = AnimationHandler;

#endif
