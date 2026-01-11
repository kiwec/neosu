// Copyright (c) 2013, PG, All rights reserved.
#ifndef QUATERNION_H
#define QUATERNION_H

#include "Vectors.h"

#include "glm/trigonometric.hpp"

struct Matrix4;
struct Matrix3;

class Quaternion {
   public:
    float x, y, z, w;

    Quaternion() { this->identity(); }
    Quaternion(float x, float y, float z, float w) {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }

    void identity() {
        this->x = this->y = this->z = 0.0f;
        this->w = 1.0f;
    }
    void set(float x, float y, float z, float w);
    void normalize();

    void fromAxis(const vec3 &axis, float angleDeg);
    void fromEuler(float yawDeg, float pitchDeg, float rollDeg);

    [[nodiscard]] inline float getYaw() const {
        return glm::degrees(std::atan2(2.0f * (this->y * this->z + this->w * this->x),
                                       this->w * this->w - this->x * this->x - this->y * this->y + this->z * this->z));
    }
    [[nodiscard]] inline float getPitch() const {
        return glm::degrees(std::asin(-2.0f * (this->x * this->z - this->w * this->y)));
    }
    [[nodiscard]] inline float getRoll() const {
        return glm::degrees(std::atan2(2.0f * (this->x * this->y + this->w * this->z),
                                       this->w * this->w + this->x * this->x - this->y * this->y - this->z * this->z));
    }

    [[nodiscard]] inline Quaternion getConjugate() const { return Quaternion(-this->x, -this->y, -this->z, this->w); }
    [[nodiscard]] Matrix4 getMatrix() const;
    [[nodiscard]] Matrix3 getMatrix3() const;

    Quaternion operator*(const Quaternion &quat) const;
    vec3 operator*(const vec3 &vec) const;

   private:
    static constexpr float EPSILON = 0.00001f;
};

#endif
