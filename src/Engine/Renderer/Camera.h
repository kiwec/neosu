#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#ifndef CAMERA_H
#define CAMERA_H

#include "Vectors.h"
#include "Matrices.h"
#include "Quaternion.h"

class Camera {
   public:
    static Matrix4 buildMatrixOrtho2D(float left, float right, float bottom, float top, float zn,
                                      float zf);  // DEPRECATED (OpenGL specific)
    static Matrix4 buildMatrixOrtho2DGLLH(float left, float right, float bottom, float top, float zn,
                                          float zf);  // OpenGL
    static Matrix4 buildMatrixOrtho2DDXLH(float left, float right, float bottom, float top, float zn,
                                          float zf);                            // DirectX
    static Matrix4 buildMatrixLookAt(vec3 eye, vec3 target, vec3 up);  // DEPRECATED
    static Matrix4 buildMatrixLookAtLH(vec3 eye, vec3 target, vec3 up);
    static Matrix4 buildMatrixPerspectiveFov(float fovRad, float aspect, float zn,
                                             float zf);  // DEPRECATED (OpenGL specific)
    static Matrix4 buildMatrixPerspectiveFovVertical(float fovRad, float aspectRatioWidthToHeight, float zn,
                                                     float zf);  // DEPRECATED
    static Matrix4 buildMatrixPerspectiveFovVerticalDXLH(float fovRad, float aspectRatioWidthToHeight, float zn,
                                                         float zf);
    static Matrix4 buildMatrixPerspectiveFovHorizontal(float fovRad, float aspectRatioHeightToWidth, float zn,
                                                       float zf);  // DEPRECATED
    static Matrix4 buildMatrixPerspectiveFovHorizontalDXLH(float fovRad, float aspectRatioHeightToWidth, float zn,
                                                           float zf);

   public:
    enum CAMERA_TYPE : uint8_t { CAMERA_TYPE_FIRST_PERSON, CAMERA_TYPE_ORBIT };

    Camera(vec3 pos = vec3(0, 0, 0), vec3 viewDir = vec3(0, 0, 1), float fovDeg = 90.0f,
           CAMERA_TYPE camType = CAMERA_TYPE_FIRST_PERSON);

    void rotateX(float pitchDeg);
    void rotateY(float yawDeg);

    void lookAt(vec3 target);

    // set
    void setType(CAMERA_TYPE camType);
    void setPos(vec3 pos);
    void setFov(float fovDeg) { this->fFov = glm::radians(fovDeg); }
    void setFovRad(float fovRad) { this->fFov = fovRad; }
    void setOrbitDistance(float orbitDistance);
    void setOrbitYAxis(bool orbitYAxis) { this->bOrbitYAxis = orbitYAxis; }

    void setRotation(float yawDeg, float pitchDeg, float rollDeg);
    void setYaw(float yawDeg);
    void setPitch(float pitchDeg);
    void setRoll(float rollDeg);
    void setWorldOrientation(Quaternion worldRotation) {
        this->worldRotation = worldRotation;
        this->updateVectors();
    }

    // get
    [[nodiscard]] inline CAMERA_TYPE getType() const { return this->camType; }
    [[nodiscard]] inline vec3 getPos() const { return this->vPos; }
    [[nodiscard]] vec3 getNextPosition(vec3 velocity) const;

    [[nodiscard]] inline float getFov() const { return glm::degrees(this->fFov); }
    [[nodiscard]] inline float getFovRad() const { return this->fFov; }
    [[nodiscard]] inline float getOrbitDistance() const { return this->fOrbitDistance; }

    [[nodiscard]] inline vec3 getWorldXAxis() const { return this->worldRotation * this->vXAxis; }
    [[nodiscard]] inline vec3 getWorldYAxis() const { return this->worldRotation * this->vYAxis; }
    [[nodiscard]] inline vec3 getWorldZAxis() const { return this->worldRotation * this->vZAxis; }

    [[nodiscard]] inline vec3 getViewDirection() const { return this->vViewDir; }
    [[nodiscard]] inline vec3 getViewUp() const { return this->vViewUp; }
    [[nodiscard]] inline vec3 getViewRight() const { return this->vViewRight; }

    [[nodiscard]] inline float getPitch() const { return this->fPitch; }
    [[nodiscard]] inline float getYaw() const { return this->fYaw; }
    [[nodiscard]] inline float getRoll() const { return this->fRoll; }

    [[nodiscard]] inline Quaternion getRotation() const { return this->rotation; }

    [[nodiscard]] vec3 getProjectedVector(vec3 point, float screenWidth, float screenHeight, float zn = 0.1f,
                                             float zf = 1.0f) const;
    [[nodiscard]] vec3 getUnProjectedVector(vec2 point, float screenWidth, float screenHeight, float zn = 0.1f,
                                               float zf = 1.0f) const;

    [[nodiscard]] bool isPointVisibleFrustum(vec3 point) const;  // within our viewing frustum
    [[nodiscard]] bool isPointVisiblePlane(vec3 point) const;    // just in front of the camera plane

   private:
    struct CAM_PLANE {
        float a, b, c, d;
    };

    static float planeDotCoord(CAM_PLANE plane, vec3 point);
    static float planeDotCoord(vec3 planeNormal, vec3 planePoint, vec3 &pv);

    void updateVectors();
    void updateViewFrustum();

    void lookAt(vec3 eye, vec3 target);

    // vars
    CAMERA_TYPE camType;
    vec3 vPos{0.f};
    vec3 vOrbitTarget{0.f};
    float fFov;
    float fOrbitDistance;
    bool bOrbitYAxis;

    // base axes
    vec3 vWorldXAxis{0.f};
    vec3 vWorldYAxis{0.f};
    vec3 vWorldZAxis{0.f};

    // derived axes
    vec3 vXAxis{0.f};
    vec3 vYAxis{0.f};
    vec3 vZAxis{0.f};

    // rotation
    Quaternion rotation;
    Quaternion worldRotation;
    float fPitch;
    float fYaw;
    float fRoll;

    // relative coordinate system
    vec3 vViewDir{0.f};
    vec3 vViewRight{0.f};
    vec3 vViewUp{0.f};

    // custom
    CAM_PLANE viewFrustum[4];
};

#endif
