// Copyright (c) 2012, PG, All rights reserved.
#include "AnimationHandler.h"

#include "noinclude.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include <algorithm>
#include <vector>

namespace AnimationHandler {
namespace {  // static namespace

// TODO: fix songbrowser to not rely on this hack to prevent never-settling animations
static constexpr const f32 ANIM_EPSILON_ABS{5e-7f};
static constexpr const f32 ANIM_EPSILON_REL{1e-4f};
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

struct Animation {
    f32 *fBase;
    f32 fTarget;
    f32 fDuration;

    f32 fStartValue;
    f32 fDelay;
    f32 fElapsedTime;
    f32 fFactor;

    ANIMATION_TYPE animType;
    bool bStarted;
};

std::vector<Animation> s_animations;

void addAnimation(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting, ANIMATION_TYPE type,
                  f32 smoothFactor = 0.f) {
    if(base == nullptr) return;

    if(overrideExisting) deleteExistingAnimation(base);

    s_animations.push_back({
        .fBase = base,
        .fTarget = target,
        .fDuration = duration,
        .fStartValue = *base,
        .fDelay = delay,
        .fElapsedTime = 0.0f,
        .fFactor = smoothFactor,
        .animType = type,
        .bStarted = (delay == 0.0f),
    });
}

}  // namespace

void clearAll() { s_animations.clear(); }

void update() {
    const auto frameTime = static_cast<f32>(engine->getFrameTime());
    const bool doLogging = cv::debug_anim.getBool();

    int idx = 0;
    for(auto it = s_animations.begin(); it != s_animations.end(); idx++) {
        Animation &animation = *it;

        // handle delay before animation starts
        if(!animation.bStarted) {
            animation.fElapsedTime += frameTime;
            if(animation.fElapsedTime < animation.fDelay) {
                ++it;
                continue;
            }
            // delay has elapsed, capture the current value and start animating
            animation.fStartValue = *animation.fBase;
            animation.bStarted = true;
            animation.fElapsedTime = 0.0f;
        }

        // update elapsed time for running animation
        animation.fElapsedTime += frameTime;

        // check if animation is close enough to target
        // use relative epsilon for large values, absolute epsilon for small values
        const f32 diff = std::abs(*animation.fBase - animation.fTarget);
        const f32 absMax = std::max(std::abs(*animation.fBase), std::abs(animation.fTarget));
        const f32 threshold = std::max(ANIM_EPSILON_REL, absMax * ANIM_EPSILON_ABS);

        if(diff <= threshold) {
            *animation.fBase = animation.fTarget;

            logIf(doLogging, "removing animation #{:d} (epsilon completion), elapsed = {:f}", idx,
                  animation.fElapsedTime);

            it = s_animations.erase(it);
            continue;
        }

        // calculate percentage
        f32 percent = std::clamp<f32>(animation.fElapsedTime / animation.fDuration, 0.0f, 1.0f);

        logIf(doLogging, "animation #{:d}, percent = {:f}", idx, percent);

        // check if finished
        if(percent >= 1.0f) {
            *animation.fBase = animation.fTarget;

            logIf(doLogging, "removing animation #{:d}, elapsed = {:f}", idx, animation.fElapsedTime);

            it = s_animations.erase(it);
            continue;
        }

        // modify percentage
        using enum ANIMATION_TYPE;
        switch(animation.animType) {
            case MOVE_SMOOTH_END:
                percent = std::clamp<f32>(1.0f - std::pow(1.0f - percent, animation.fFactor), 0.0f, 1.0f);
                if((int)(percent * (animation.fTarget - animation.fStartValue) + animation.fStartValue) ==
                   (int)animation.fTarget)
                    percent = 1.0f;
                break;

            case MOVE_QUAD_IN:
                percent = percent * percent;
                break;

            case MOVE_QUAD_OUT:
                percent = -percent * (percent - 2.0f);
                break;

            case MOVE_QUAD_INOUT:
                if((percent *= 2.0f) < 1.0f)
                    percent = 0.5f * percent * percent;
                else {
                    percent -= 1.0f;
                    percent = -0.5f * ((percent) * (percent - 2.0f) - 1.0f);
                }
                break;

            case MOVE_CUBIC_IN:
                percent = percent * percent * percent;
                break;

            case MOVE_CUBIC_OUT:
                percent = percent - 1.0f;
                percent = percent * percent * percent + 1.0f;
                break;

            case MOVE_QUART_IN:
                percent = percent * percent * percent * percent;
                break;

            case MOVE_QUART_OUT:
                percent = percent - 1.0f;
                percent = 1.0f - percent * percent * percent * percent;
                break;
            default:
                break;
        }

        // set new value
        *animation.fBase = animation.fStartValue * (1.0f - percent) + animation.fTarget * percent;

        ++it;
    }

    if(s_animations.size() > 512) {
        debugLog("WARNING: AnimationHandler has {:d} animations!", s_animations.size());
    }

    // printf("AnimStackSize = %i\n", s_animations.size());
}

void moveLinear(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_LINEAR);
}

void moveQuadIn(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_IN);
}

void moveQuadOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_OUT);
}

void moveQuadInOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_INOUT);
}

void moveCubicIn(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_CUBIC_IN);
}

void moveCubicOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_CUBIC_OUT);
}

void moveQuartIn(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUART_IN);
}

void moveQuartOut(f32 *base, f32 target, f32 duration, f32 delay, bool overrideExisting) {
    addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUART_OUT);
}

void moveSmoothEnd(f32 *base, f32 target, f32 duration, f32 smoothFactor, f32 delay) {
    addAnimation(base, target, duration, delay, true, ANIMATION_TYPE::MOVE_SMOOTH_END, smoothFactor);
}

void deleteExistingAnimation(f32 *base) {
    std::erase_if(s_animations, [base](const auto &a) -> bool { return a.fBase == base; });
}

f32 getRemainingDuration(f32 *base) {
    if(const auto &it = std::ranges::find_if(s_animations, [base](const auto &a) -> bool { return a.fBase == base; });
       it != s_animations.end()) {
        const auto &animation = *it;
        if(!animation.bStarted) {
            // still in delay phase
            return (animation.fDelay - animation.fElapsedTime) + animation.fDuration;
        }
        // in animation phase
        return std::max(0.0f, animation.fDuration - animation.fElapsedTime);
    }

    return 0.0f;
}

bool isAnimating(f32 *base) {
    return std::ranges::contains(s_animations, base, [](const auto &a) -> f32 * { return a.fBase; });
}

uSz getNumActiveAnimations() { return s_animations.size(); }

}  // namespace AnimationHandler
