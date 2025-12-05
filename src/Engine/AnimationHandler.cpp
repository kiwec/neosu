// Copyright (c) 2012, PG, All rights reserved.
#include "AnimationHandler.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include <algorithm>

void AnimationHandler::update() {
    const auto frameTime = static_cast<float>(engine->getFrameTime());
    const bool doLogging = cv::debug_anim.getBool();

    int idx = 0;
    for(auto it = this->vAnimations.begin(); it != this->vAnimations.end(); idx++) {
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
        const float diff = std::abs(*animation.fBase - animation.fTarget);
        const float absMax = std::max(std::abs(*animation.fBase), std::abs(animation.fTarget));
        const float threshold = std::max(ANIM_EPSILON, absMax * ANIM_EPSILON);

        if(diff <= threshold) {
            *animation.fBase = animation.fTarget;

            logIf(doLogging, "removing animation #{:d} (epsilon completion), elapsed = {:f}", idx,
                  animation.fElapsedTime);

            it = this->vAnimations.erase(it);
            continue;
        }

        // calculate percentage
        float percent = std::clamp<float>(animation.fElapsedTime / animation.fDuration, 0.0f, 1.0f);

        logIf(doLogging, "animation #{:d}, percent = {:f}", idx, percent);

        // check if finished
        if(percent >= 1.0f) {
            *animation.fBase = animation.fTarget;

            logIf(doLogging, "removing animation #{:d}, elapsed = {:f}", idx, animation.fElapsedTime);

            it = this->vAnimations.erase(it);
            continue;
        }

        // modify percentage
        switch(animation.animType) {
            case ANIMATION_TYPE::MOVE_SMOOTH_END:
                percent = std::clamp<float>(1.0f - std::pow(1.0f - percent, animation.fFactor), 0.0f, 1.0f);
                if((int)(percent * (animation.fTarget - animation.fStartValue) + animation.fStartValue) ==
                   (int)animation.fTarget)
                    percent = 1.0f;
                break;

            case ANIMATION_TYPE::MOVE_QUAD_IN:
                percent = percent * percent;
                break;

            case ANIMATION_TYPE::MOVE_QUAD_OUT:
                percent = -percent * (percent - 2.0f);
                break;

            case ANIMATION_TYPE::MOVE_QUAD_INOUT:
                if((percent *= 2.0f) < 1.0f)
                    percent = 0.5f * percent * percent;
                else {
                    percent -= 1.0f;
                    percent = -0.5f * ((percent) * (percent - 2.0f) - 1.0f);
                }
                break;

            case ANIMATION_TYPE::MOVE_CUBIC_IN:
                percent = percent * percent * percent;
                break;

            case ANIMATION_TYPE::MOVE_CUBIC_OUT:
                percent = percent - 1.0f;
                percent = percent * percent * percent + 1.0f;
                break;

            case ANIMATION_TYPE::MOVE_QUART_IN:
                percent = percent * percent * percent * percent;
                break;

            case ANIMATION_TYPE::MOVE_QUART_OUT:
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

    if(this->vAnimations.size() > 512) {
        debugLog("WARNING: AnimationHandler has {:d} animations!", this->vAnimations.size());
    }

    // printf("AnimStackSize = %i\n", this->vAnimations.size());
}

void AnimationHandler::moveLinear(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_LINEAR);
}

void AnimationHandler::moveQuadIn(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_IN);
}

void AnimationHandler::moveQuadOut(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_OUT);
}

void AnimationHandler::moveQuadInOut(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUAD_INOUT);
}

void AnimationHandler::moveCubicIn(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_CUBIC_IN);
}

void AnimationHandler::moveCubicOut(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_CUBIC_OUT);
}

void AnimationHandler::moveQuartIn(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUART_IN);
}

void AnimationHandler::moveQuartOut(float *base, float target, float duration, float delay, bool overrideExisting) {
    this->addAnimation(base, target, duration, delay, overrideExisting, ANIMATION_TYPE::MOVE_QUART_OUT);
}

void AnimationHandler::moveSmoothEnd(float *base, float target, float duration, float smoothFactor, float delay) {
    this->addAnimation(base, target, duration, delay, true, ANIMATION_TYPE::MOVE_SMOOTH_END, smoothFactor);
}

void AnimationHandler::addAnimation(float *base, float target, float duration, float delay, bool overrideExisting,
                                    AnimationHandler::ANIMATION_TYPE type, float smoothFactor) {
    if(base == nullptr) return;

    if(overrideExisting) this->deleteExistingAnimation(base);

    this->vAnimations.push_back({
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

void AnimationHandler::deleteExistingAnimation(float *base) {
    std::erase_if(this->vAnimations, [base](const auto &a) -> bool { return a.fBase == base; });
}

float AnimationHandler::getRemainingDuration(float *base) const {
    if(const auto &it = std::ranges::find(this->vAnimations, base, [](const auto &a) -> float * { return a.fBase; });
       it != this->vAnimations.end()) {
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

bool AnimationHandler::isAnimating(float *base) const {
    return std::ranges::contains(this->vAnimations, base, [](const auto &a) -> float * { return a.fBase; });
}
