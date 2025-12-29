// Copyright (c) 2012, PG, All rights reserved.
#include "AnimationHandler.h"

#include "noinclude.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include <algorithm>
#include <variant>
#include <vector>

namespace AnimationHandler {
namespace {

enum class ANIMATION_TYPE : uint8_t {
    MOVE_LINEAR,
    MOVE_SMOOTH_END,
    MOVE_QUAD_INOUT,
    MOVE_QUAD_IN,
    MOVE_QUAD_OUT,
    MOVE_CUBIC_IN,
    MOVE_CUBIC_OUT,
    MOVE_QUART_IN,
    MOVE_QUART_OUT
};

template <typename FltType>
    requires(std::is_same_v<FltType, f32> || std::is_same_v<FltType, f64>)
struct BaseAnim {
    FltType *fBase;
    FltType fTarget;
    FltType fDuration;
    FltType fStartValue;
    FltType fDelay;
    FltType fElapsedTime;
    FltType fFactor;
    ANIMATION_TYPE animType;
    bool bStarted;
};

using Animation = std::variant<BaseAnim<f32>, BaseAnim<f64>>;

std::vector<Animation> s_animations;

template <typename FltType>
void deleteExistingAnimationImpl(FltType *base) {
    std::erase_if(s_animations, [base](const Animation &anim) -> bool {
        if(auto *typed = std::get_if<BaseAnim<FltType>>(&anim)) return typed->fBase == base;
        return false;
    });
}

template <typename FltType>
void addAnimation(FltType *base, FltType target, FltType duration, FltType delay, bool overrideExisting,
                  ANIMATION_TYPE type, FltType smoothFactor = FltType{0}) {
    if(base == nullptr) return;
    if(overrideExisting) deleteExistingAnimationImpl(base);

    s_animations.emplace_back(BaseAnim<FltType>{
        .fBase = base,
        .fTarget = target,
        .fDuration = duration,
        .fStartValue = *base,
        .fDelay = delay,
        .fElapsedTime = FltType{0},
        .fFactor = smoothFactor,
        .animType = type,
        .bStarted = (delay == FltType{0}),
    });
}

template <typename FltType>
forceinline bool updateAnimation(BaseAnim<FltType> &anim, FltType frameTime, bool doLogging, int idx) {
    constexpr FltType zero{0};
    constexpr FltType half{0.5};
    constexpr FltType one{1};
    constexpr FltType two{2};

    if(!anim.bStarted) {
        anim.fElapsedTime += frameTime;
        if(anim.fElapsedTime < anim.fDelay) return false;

        anim.fStartValue = *anim.fBase;
        anim.bStarted = true;
        anim.fElapsedTime = zero;
    }

    anim.fElapsedTime += frameTime;

    const FltType diff = std::abs(*anim.fBase - anim.fTarget);
    const FltType absMax = std::max(std::abs(*anim.fBase), std::abs(anim.fTarget));
    const FltType threshold = std::max(FltType{1e-4}, absMax * FltType{1e-6});

    if(diff <= threshold) {
        *anim.fBase = anim.fTarget;
        logIf(doLogging, "removing animation #{:d} (epsilon completion), elapsed = {:f}", idx, anim.fElapsedTime);
        return true;
    }

    FltType percent = std::clamp(anim.fElapsedTime / anim.fDuration, zero, one);

    logIf(doLogging, "animation #{:d}, percent = {:f}", idx, percent);

    if(percent >= one) {
        *anim.fBase = anim.fTarget;
        logIf(doLogging, "removing animation #{:d}, elapsed = {:f}", idx, anim.fElapsedTime);
        return true;
    }

    using enum ANIMATION_TYPE;
    switch(anim.animType) {
        case MOVE_SMOOTH_END:
            percent = std::clamp(one - std::pow(one - percent, anim.fFactor), zero, one);
            if(static_cast<int>(percent * (anim.fTarget - anim.fStartValue) + anim.fStartValue) ==
               static_cast<int>(anim.fTarget))
                percent = one;
            break;
        case MOVE_QUAD_IN:
            percent = percent * percent;
            break;
        case MOVE_QUAD_OUT:
            percent = -percent * (percent - two);
            break;
        case MOVE_QUAD_INOUT:
            if((percent *= two) < one)
                percent = half * percent * percent;
            else {
                percent -= one;
                percent = -half * (percent * (percent - two) - one);
            }
            break;
        case MOVE_CUBIC_IN:
            percent = percent * percent * percent;
            break;
        case MOVE_CUBIC_OUT:
            percent -= one;
            percent = percent * percent * percent + one;
            break;
        case MOVE_QUART_IN:
            percent = percent * percent * percent * percent;
            break;
        case MOVE_QUART_OUT:
            percent -= one;
            percent = one - percent * percent * percent * percent;
            break;
        default:
            break;
    }

    *anim.fBase = anim.fStartValue * (one - percent) + anim.fTarget * percent;
    return false;
}

}  // namespace

void clearAll() { s_animations.clear(); }

void update() {
    const f64 frameTime = engine->getFrameTime();
    const bool doLogging = cv::debug_anim.getBool();

    for(uSz i = 0; i < s_animations.size();) {
        const bool remove = std::visit(
            [&](auto &anim) {
                using FltType = std::remove_pointer_t<decltype(anim.fBase)>;
                return updateAnimation(anim, static_cast<FltType>(frameTime), doLogging, static_cast<int>(i));
            },
            s_animations[i]);

        if(remove) {
            s_animations[i] = s_animations.back();
            s_animations.pop_back();
        } else {
            ++i;
        }
    }

    if(s_animations.size() > 512) {
        debugLog("WARNING: AnimationHandler has {:d} animations!", s_animations.size());
    }
}

template <AnimFloat T>
void moveLinear(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_LINEAR);
}

template <AnimFloat T>
void moveQuadIn(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_IN);
}

template <AnimFloat T>
void moveQuadOut(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_OUT);
}

template <AnimFloat T>
void moveQuadInOut(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_INOUT);
}

template <AnimFloat T>
void moveCubicIn(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_CUBIC_IN);
}

template <AnimFloat T>
void moveCubicOut(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_CUBIC_OUT);
}

template <AnimFloat T>
void moveQuartIn(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUART_IN);
}

template <AnimFloat T>
void moveQuartOut(T *base, T target, T duration, T delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUART_OUT);
}

template <AnimFloat T>
void moveSmoothEnd(T *base, T target, T duration, T smoothFactor, T delay) {
    addAnimation(base, target, duration, delay, true, ANIMATION_TYPE::MOVE_SMOOTH_END, smoothFactor);
}

template <AnimFloat T>
void deleteExistingAnimation(T *base) {
    deleteExistingAnimationImpl(base);
}

template <AnimFloat T>
T getRemainingDuration(T *base) {
    auto it = std::ranges::find_if(s_animations, [base](const Animation &anim) -> bool {
        if(auto *typed = std::get_if<BaseAnim<T>>(&anim)) return typed->fBase == base;
        return false;
    });
    if(it == s_animations.end()) return T{0};

    const auto &anim = std::get<BaseAnim<T>>(*it);
    if(!anim.bStarted) return (anim.fDelay - anim.fElapsedTime) + anim.fDuration;
    return std::max(T{0}, anim.fDuration - anim.fElapsedTime);
}

template <AnimFloat T>
bool isAnimating(T *base) {
    return std::ranges::any_of(s_animations, [base](const Animation &anim) -> bool {
        if(auto *typed = std::get_if<BaseAnim<T>>(&anim)) return typed->fBase == base;
        return false;
    });
}

// explicit instantiations
template void moveLinear(f32 *, f32, f32, f32, bool);
template void moveQuadIn(f32 *, f32, f32, f32, bool);
template void moveQuadOut(f32 *, f32, f32, f32, bool);
template void moveQuadInOut(f32 *, f32, f32, f32, bool);
template void moveCubicIn(f32 *, f32, f32, f32, bool);
template void moveCubicOut(f32 *, f32, f32, f32, bool);
template void moveQuartIn(f32 *, f32, f32, f32, bool);
template void moveQuartOut(f32 *, f32, f32, f32, bool);
template void moveSmoothEnd(f32 *, f32, f32, f32, f32);
template void deleteExistingAnimation(f32 *);
template f32 getRemainingDuration(f32 *);
template bool isAnimating(f32 *);

template void moveLinear(f64 *, f64, f64, f64, bool);
template void moveQuadIn(f64 *, f64, f64, f64, bool);
template void moveQuadOut(f64 *, f64, f64, f64, bool);
template void moveQuadInOut(f64 *, f64, f64, f64, bool);
template void moveCubicIn(f64 *, f64, f64, f64, bool);
template void moveCubicOut(f64 *, f64, f64, f64, bool);
template void moveQuartIn(f64 *, f64, f64, f64, bool);
template void moveQuartOut(f64 *, f64, f64, f64, bool);
template void moveSmoothEnd(f64 *, f64, f64, f64, f64);
template void deleteExistingAnimation(f64 *);
template f64 getRemainingDuration(f64 *);
template bool isAnimating(f64 *);

uSz getNumActiveAnimations() { return s_animations.size(); }

}  // namespace AnimationHandler
