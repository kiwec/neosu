// Copyright (c) 2012, PG, All rights reserved.
#ifndef ANIMATIONHANDLER_H
#define ANIMATIONHANDLER_H

#include "types.h"
#include <concepts>

namespace AnimationHandler {

// cv::debug_anim change callback
void onDebugAnimChange(float newVal);

template <typename T>
concept AnimFloat = std::same_as<T, f32> || std::same_as<T, f64>;

// called by engine once per frame, after updating time
void update();
void clearAll();  // called when shutting down, for safety

// base
template <AnimFloat T>
void moveLinear(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveQuadIn(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveQuadOut(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveQuadInOut(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveCubicIn(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveCubicOut(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveQuartIn(T *base, T target, T duration, T delay, bool overrideExisting = false);
template <AnimFloat T>
void moveQuartOut(T *base, T target, T duration, T delay, bool overrideExisting = false);

// simplified, without delay
template <AnimFloat T>
inline void moveLinear(T *base, T target, T duration, bool overrideExisting = false) {
    moveLinear(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveQuadIn(T *base, T target, T duration, bool overrideExisting = false) {
    moveQuadIn(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveQuadOut(T *base, T target, T duration, bool overrideExisting = false) {
    moveQuadOut(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveQuadInOut(T *base, T target, T duration, bool overrideExisting = false) {
    moveQuadInOut(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveCubicIn(T *base, T target, T duration, bool overrideExisting = false) {
    moveCubicIn(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveCubicOut(T *base, T target, T duration, bool overrideExisting = false) {
    moveCubicOut(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveQuartIn(T *base, T target, T duration, bool overrideExisting = false) {
    moveQuartIn(base, target, duration, T{0}, overrideExisting);
}
template <AnimFloat T>
inline void moveQuartOut(T *base, T target, T duration, bool overrideExisting = false) {
    moveQuartOut(base, target, duration, T{0}, overrideExisting);
}

// DEPRECATED:
template <AnimFloat T>
void moveSmoothEnd(T *base, T target, T duration, T smoothFactor = T{20}, T delay = T{0});

template <AnimFloat T>
void deleteExistingAnimation(T *base);

template <AnimFloat T>
[[nodiscard]] T getRemainingDuration(T *base);

template <AnimFloat T>
[[nodiscard]] bool isAnimating(T *base);

[[nodiscard]] uSz getNumActiveAnimations();

}  // namespace AnimationHandler

namespace anim = AnimationHandler;

#endif
