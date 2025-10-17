// Copyright (c) 2012, PG, All rights reserved.
#include "AnimationHandler.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

AnimationHandler *anim = nullptr;

AnimationHandler::AnimationHandler() { anim = this; }

AnimationHandler::~AnimationHandler() {
    this->vAnimations.clear();

    anim = nullptr;
}

void AnimationHandler::update() {
    const auto curFrameTime = static_cast<float>(engine->getTime());
    const bool doLogging = cv::debug_anim.getBool();

    for(sSz i = 0; i < this->vAnimations.size(); i++) {
        // start animation
        Animation &animation = this->vAnimations[i];
        if(curFrameTime < animation.fStartTime)
            continue;
        else if(!animation.bStarted) {
            // after our delay, take the current value as startValue, then start animating to the target
            animation.fStartValue = *animation.fBase;
            animation.bStarted = true;
        }

        // check if animation is close enough to target
        // use relative epsilon for large values, absolute epsilon for small values
        const float diff = std::abs(*animation.fBase - animation.fTarget);
        const float absMax = std::max(std::abs(*animation.fBase), std::abs(animation.fTarget));
        const float threshold = std::max(ANIM_EPSILON, absMax * ANIM_EPSILON);

        if(diff <= threshold) {
            *animation.fBase = animation.fTarget;

            logIf(doLogging, "removing animation #{:d} (epsilon completion), dtime = {:f}", i,
                  curFrameTime - animation.fStartTime);

            this->vAnimations.erase(this->vAnimations.begin() + i);
            i--;
            continue;
        }

        // calculate percentage
        float percent = std::clamp<float>((curFrameTime - animation.fStartTime) / (animation.fDuration), 0.0f, 1.0f);

        logIf(doLogging, "animation #{:d}, percent = {:f}", i, percent);

        // check if finished
        if(percent >= 1.0f) {
            *animation.fBase = animation.fTarget;

            logIf(doLogging, "removing animation #{:d}, dtime = {:f}", i, curFrameTime - animation.fStartTime);

            this->vAnimations.erase(this->vAnimations.begin() + i);
            i--;

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

    if(overrideExisting) this->overrideExistingAnimation(base);

    this->vAnimations.push_back(Animation{
        .fBase = base,
        .fTarget = target,
        .fDuration = duration,
        .fStartValue = *base,
        .fStartTime = static_cast<float>(engine->getTime()) + delay,
        .fFactor = smoothFactor,
        .animType = type,
        .bStarted = (delay == 0.0f),
    });
}

void AnimationHandler::overrideExistingAnimation(float *base) { this->deleteExistingAnimation(base); }

void AnimationHandler::deleteExistingAnimation(float *base) {
    for(sSz i = 0; i < this->vAnimations.size(); i++) {
        if(this->vAnimations[i].fBase == base) {
            this->vAnimations.erase(this->vAnimations.begin() + i);
            i--;
        }
    }
}

float AnimationHandler::getRemainingDuration(float *base) const {
    const auto curFrameTime = static_cast<float>(engine->getTime());
    for(const auto &vAnimation : this->vAnimations) {
        if(vAnimation.fBase == base)
            return std::max(0.0f, (vAnimation.fStartTime + vAnimation.fDuration) - curFrameTime);
    }

    return 0.0f;
}

bool AnimationHandler::isAnimating(float *base) const {
    for(const auto &vAnimation : this->vAnimations) {
        if(vAnimation.fBase == base) return true;
    }

    return false;
}
