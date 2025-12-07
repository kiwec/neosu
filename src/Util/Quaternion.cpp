// Copyright (c) 2013, PG, All rights reserved.
#include "Quaternion.h"
#include "Matrices.h"

#include "noinclude.h"

void Quaternion::set(float x, float y, float z, float w) {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
}

void Quaternion::normalize() {
    float magnitudeSquared = this->w * this->w + this->x * this->x + this->y * this->y + this->z * this->z;
    if(std::abs(magnitudeSquared) > EPSILON && std::abs(magnitudeSquared - 1.0f) > EPSILON) {
        float magnitude = std::sqrt(magnitudeSquared);
        this->w /= magnitude;
        this->x /= magnitude;
        this->y /= magnitude;
        this->z /= magnitude;
    }
}

void Quaternion::fromAxis(const vec3 &axis, float angleDeg) {
    angleDeg = glm::radians(angleDeg);

    angleDeg *= 0.5f;

    vec3 axisCopy = vec::normalize(axis);

    const float sinAngle = std::sin(angleDeg);

    this->x = (axisCopy.x * sinAngle);
    this->y = (axisCopy.y * sinAngle);
    this->z = (axisCopy.z * sinAngle);
    this->w = std::cos(angleDeg);
}

void Quaternion::fromEuler(float yawDeg, float pitchDeg, float rollDeg) {
    float y = yawDeg * PIOVER180 / 2.0f;
    float p = pitchDeg * PIOVER180 / 2.0f;
    float r = rollDeg * PIOVER180 / 2.0f;

    float sinYaw = std::sin(y);
    float sinPitch = std::sin(p);
    float sinRoll = std::sin(r);

    float cosYaw = std::cos(y);
    float cosPitch = std::cos(p);
    float cosRoll = std::cos(r);

    this->x = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
    this->y = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
    this->z = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
    this->w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;

    this->normalize();
}

Matrix4 Quaternion::getMatrix() const {
    return {1.0f - 2.0f * this->y * this->y - 2.0f * this->z * this->z,
            2.0f * this->x * this->y - 2.0f * this->z * this->w,
            2.0f * this->x * this->z + 2.0f * this->y * this->w,
            0.0f,
            2.0f * this->x * this->y + 2.0f * this->z * this->w,
            1.0f - 2.0f * this->x * this->x - 2.0f * this->z * this->z,
            2.0f * this->y * this->z - 2.0f * this->x * this->w,
            0.0f,
            2.0f * this->x * this->z - 2.0f * this->y * this->w,
            2.0f * this->y * this->z + 2.0f * this->x * this->w,
            1.0f - 2.0f * this->x * this->x - 2.0f * this->y * this->y,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f};
}

Matrix3 Quaternion::getMatrix3() const {
    return {
        1.0f - 2.0f * this->y * this->y - 2.0f * this->z * this->z, 2.0f * this->x * this->y - 2.0f * this->z * this->w,
        2.0f * this->x * this->z + 2.0f * this->y * this->w,        2.0f * this->x * this->y + 2.0f * this->z * this->w,
        1.0f - 2.0f * this->x * this->x - 2.0f * this->z * this->z, 2.0f * this->y * this->z - 2.0f * this->x * this->w,
        2.0f * this->x * this->z - 2.0f * this->y * this->w,        2.0f * this->y * this->z + 2.0f * this->x * this->w,
        1.0f - 2.0f * this->x * this->x - 2.0f * this->y * this->y};
}

Quaternion Quaternion::operator*(const Quaternion &quat) const {
    return {this->w * quat.x + this->x * quat.w + this->y * quat.z - this->z * quat.y,
            this->w * quat.y + this->y * quat.w + this->z * quat.x - this->x * quat.z,
            this->w * quat.z + this->z * quat.w + this->x * quat.y - this->y * quat.x,
            this->w * quat.w - this->x * quat.x - this->y * quat.y - this->z * quat.z};
}

vec3 Quaternion::operator*(const vec3 &vec) const {
    vec3 vecCopy(vec);

    Quaternion vecQuaternion, resQuaternion;
    vecQuaternion.x = vecCopy.x;
    vecQuaternion.y = vecCopy.y;
    vecQuaternion.z = vecCopy.z;
    vecQuaternion.w = 0.0f;

    resQuaternion = vecQuaternion * this->getConjugate();
    resQuaternion = *this * resQuaternion;

    return {resQuaternion.x, resQuaternion.y, resQuaternion.z};
}
