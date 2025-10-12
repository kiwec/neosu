// Copyright (c) 2012, PG, All rights reserved.
#ifndef ANIMATIONHANDLER_H
#define ANIMATIONHANDLER_H

#include "cbase.h"

class AnimationHandler {
    NOCOPY_NOMOVE(AnimationHandler)

   public:
    AnimationHandler();
    ~AnimationHandler();

    void update();

    // base
    void moveLinear(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveQuadIn(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveQuadOut(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveQuadInOut(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveCubicIn(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveCubicOut(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveQuartIn(float *base, float target, float duration, float delay, bool overrideExisting = false);
    void moveQuartOut(float *base, float target, float duration, float delay, bool overrideExisting = false);

    // simplified, without delay
    void moveLinear(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveLinear(base, target, duration, 0.0f, overrideExisting);
    }
    void moveQuadIn(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveQuadIn(base, target, duration, 0.0f, overrideExisting);
    }
    void moveQuadOut(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveQuadOut(base, target, duration, 0.0f, overrideExisting);
    }
    void moveQuadInOut(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveQuadInOut(base, target, duration, 0.0f, overrideExisting);
    }
    void moveCubicIn(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveCubicIn(base, target, duration, 0.0f, overrideExisting);
    }
    void moveCubicOut(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveCubicOut(base, target, duration, 0.0f, overrideExisting);
    }
    void moveQuartIn(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveQuartIn(base, target, duration, 0.0f, overrideExisting);
    }
    void moveQuartOut(float *base, float target, float duration, bool overrideExisting = false) {
        this->moveQuartOut(base, target, duration, 0.0f, overrideExisting);
    }

    // DEPRECATED:
    void moveSmoothEnd(float *base, float target, float duration, float smoothFactor = 20.f, float delay = 0.0f);

    void deleteExistingAnimation(float *base);

    float getRemainingDuration(float *base) const;
    bool isAnimating(float *base) const;

    [[nodiscard]] inline size_t getNumActiveAnimations() const { return this->vAnimations.size(); }

   private:
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
        float *fBase;
        float fTarget;
        float fDuration;

        float fStartValue;
        float fStartTime;
        float fFactor;

        ANIMATION_TYPE animType;
        bool bStarted;
    };

    void addAnimation(float *base, float target, float duration, float delay, bool overrideExisting,
                      ANIMATION_TYPE type, float smoothFactor = 0.0f);
    void overrideExistingAnimation(float *base);

    std::vector<Animation> vAnimations;

    static constexpr const float ANIM_EPSILON{1e-6f};
};

extern AnimationHandler *anim;

#endif
