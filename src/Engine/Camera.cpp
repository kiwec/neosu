// Copyright (c) 2015, PG, All rights reserved.
#include "Camera.h"

#include "ConVar.h"
#include "Engine.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"

Matrix4 Camera::buildMatrixOrtho2D(float left, float right, float bottom, float top, float zn, float zf) {
    return buildMatrixOrtho2DGLLH(left, right, bottom, top, zn, zf);
}

Matrix4 Camera::buildMatrixOrtho2DGLLH(float left, float right, float bottom, float top, float zn, float zf) {
    Matrix4 result;
    glm::mat4 glmMatrix = glm::ortho(left, right, bottom, top, zn, zf);
    result = Matrix4(&glmMatrix[0][0]);
    return result;
}

Matrix4 Camera::buildMatrixOrtho2DDXLH(float left, float right, float bottom, float top, float zn, float zf) {
    Matrix4 result;
    glm::mat4 glmMatrix = glm::orthoLH_ZO(left, right, bottom, top, zn, zf);
    result = Matrix4(&glmMatrix[0][0]);
    return result;
}

Matrix4 Camera::buildMatrixLookAt(vec3 eye, vec3 target, vec3 up) {
    Matrix4 result;
    glm::mat4 glmMatrix = glm::lookAt(glm::vec3(eye.x, eye.y, eye.z), glm::vec3(target.x, target.y, target.z),
                                      glm::vec3(up.x, up.y, up.z));
    result = Matrix4(&glmMatrix[0][0]);
    return result;
}

Matrix4 Camera::buildMatrixLookAtLH(vec3 eye, vec3 target, vec3 up) {
    Matrix4 result;
    glm::mat4 glmMatrix = glm::lookAtLH(glm::vec3(eye.x, eye.y, eye.z), glm::vec3(target.x, target.y, target.z),
                                        glm::vec3(up.x, up.y, up.z));
    result = Matrix4(&glmMatrix[0][0]);
    return result;
}

Matrix4 Camera::buildMatrixPerspectiveFov(float fovRad, float aspect, float zn, float zf) {
    return buildMatrixPerspectiveFovVertical(fovRad, aspect, zn, zf);
}

Matrix4 Camera::buildMatrixPerspectiveFovVertical(float fovRad, float aspectRatioWidthToHeight, float zn, float zf) {
    Matrix4 result;
    glm::mat4 glmMatrix = glm::perspective(fovRad, aspectRatioWidthToHeight, zn, zf);
    result = Matrix4(&glmMatrix[0][0]);
    return result;
}

Matrix4 Camera::buildMatrixPerspectiveFovVerticalDXLH(float fovRad, float aspectRatioWidthToHeight, float zn,
                                                      float zf) {
    Matrix4 result;
    glm::mat4 glmMatrix = glm::perspectiveLH_ZO(fovRad, aspectRatioWidthToHeight, zn, zf);
    result = Matrix4(&glmMatrix[0][0]);
    return result;
}

Matrix4 Camera::buildMatrixPerspectiveFovHorizontal(float fovRad, float aspectRatioHeightToWidth, float zn, float zf) {
    float verticalFov = 2.0f * glm::atan(glm::tan(fovRad * 0.5f) * aspectRatioHeightToWidth);
    return buildMatrixPerspectiveFovVertical(verticalFov, 1.0f / aspectRatioHeightToWidth, zn, zf);
}

Matrix4 Camera::buildMatrixPerspectiveFovHorizontalDXLH(float fovRad, float aspectRatioHeightToWidth, float zn,
                                                        float zf) {
    float verticalFov = 2.0f * glm::atan(glm::tan(fovRad * 0.5f) * aspectRatioHeightToWidth);
    return buildMatrixPerspectiveFovVerticalDXLH(verticalFov, 1.0f / aspectRatioHeightToWidth, zn, zf);
}

static vec3 vec3TransformCoord(const vec3 &in, const Matrix4 &mat) {
    glm::vec4 homogeneous(in.x, in.y, in.z, 1.0f);
    const glm::mat4 &glmMat = mat.getGLM();
    glm::vec4 result = glmMat * homogeneous;

    // perspective divide
    if(result.w != 0.0f) {
        result.x /= result.w;
        result.y /= result.w;
        result.z /= result.w;
    }

    return {result.x, result.y, result.z};
}

//*************************//
//	Camera implementation  //
//*************************//

Camera::Camera(vec3 pos, vec3 viewDir, float fovDeg, CAMERA_TYPE camType) {
    this->vPos = pos;
    this->vViewDir = viewDir;
    this->fFov = glm::radians(fovDeg);
    this->camType = camType;

    this->fOrbitDistance = 5.0f;
    this->bOrbitYAxis = true;

    this->fPitch = 0;
    this->fYaw = 0;
    this->fRoll = 0;

    // base axes
    this->vWorldXAxis = vec3(1, 0, 0);
    this->vWorldYAxis = vec3(0, 1, 0);
    this->vWorldZAxis = vec3(0, 0, 1);

    // derived axes
    this->vXAxis = this->vWorldXAxis;
    this->vYAxis = this->vWorldYAxis;
    this->vZAxis = this->vWorldZAxis;

    this->vViewRight = this->vWorldXAxis;
    this->vViewUp = this->vWorldYAxis;

    this->lookAt(pos + viewDir);
}

void Camera::updateVectors() {
    // update rotation
    if(this->camType == CAMERA_TYPE_FIRST_PERSON)
        this->rotation.fromEuler(this->fRoll, this->fYaw, -this->fPitch);
    else if(this->camType == CAMERA_TYPE_ORBIT) {
        this->rotation.identity();

        if(this->bOrbitYAxis) {
            // yaw
            Quaternion tempQuat;
            tempQuat.fromAxis(this->vYAxis, this->fYaw);

            this->rotation = tempQuat * this->rotation;

            // pitch
            tempQuat.fromAxis(this->vXAxis, -this->fPitch);
            this->rotation = this->rotation * tempQuat;

            this->rotation.normalize();
        }
    }

    // calculate new coordinate system
    this->vViewDir = (this->worldRotation * this->rotation) * this->vZAxis;
    this->vViewRight = (this->worldRotation * this->rotation) * this->vXAxis;
    this->vViewUp = (this->worldRotation * this->rotation) * this->vYAxis;

    // update pos if we are orbiting (with the new coordinate system from above)
    if(this->camType == CAMERA_TYPE_ORBIT) this->setPos(this->vOrbitTarget);

    this->updateViewFrustum();
}

// using view matrix from camera position
void Camera::updateViewFrustum() {
    Matrix4 viewMatrix = this->buildMatrixLookAt(this->vPos, this->vPos + this->vViewDir, this->vViewUp);
    Matrix4 projectionMatrix = this->buildMatrixPerspectiveFov(
        this->fFov, (float)engine->getScreenWidth() / (float)engine->getScreenHeight(), 0.1f, 100.0f);
    Matrix4 viewProj = projectionMatrix * viewMatrix;

    // extract frustum planes from view-projection matrix
    // left plane
    this->viewFrustum[0].a = viewProj[3] + viewProj[0];
    this->viewFrustum[0].b = viewProj[7] + viewProj[4];
    this->viewFrustum[0].c = viewProj[11] + viewProj[8];
    this->viewFrustum[0].d = viewProj[15] + viewProj[12];

    // right plane
    this->viewFrustum[1].a = viewProj[3] - viewProj[0];
    this->viewFrustum[1].b = viewProj[7] - viewProj[4];
    this->viewFrustum[1].c = viewProj[11] - viewProj[8];
    this->viewFrustum[1].d = viewProj[15] - viewProj[12];

    // top plane
    this->viewFrustum[2].a = viewProj[3] - viewProj[1];
    this->viewFrustum[2].b = viewProj[7] - viewProj[5];
    this->viewFrustum[2].c = viewProj[11] - viewProj[9];
    this->viewFrustum[2].d = viewProj[15] - viewProj[13];

    // bottom plane
    this->viewFrustum[3].a = viewProj[3] + viewProj[1];
    this->viewFrustum[3].b = viewProj[7] + viewProj[5];
    this->viewFrustum[3].c = viewProj[11] + viewProj[9];
    this->viewFrustum[3].d = viewProj[15] + viewProj[13];

    // normalize planes
    for(auto &i : this->viewFrustum) {
        glm::vec3 normal(i.a, i.b, i.c);
        float length = glm::length(normal);

        if(length > 0.0f) {
            normal = glm::normalize(normal);
            i.a = normal.x;
            i.b = normal.y;
            i.c = normal.z;
            i.d = i.d / length;
        } else {
            i.a = 0.0f;
            i.b = 0.0f;
            i.c = 0.0f;
            i.d = 0.0f;
        }
    }
}

void Camera::rotateX(float pitchDeg) {
    this->fPitch += pitchDeg;

    if(this->fPitch > 89.0f)
        this->fPitch = 89.0f;
    else if(this->fPitch < -89.0f)
        this->fPitch = -89.0f;

    this->updateVectors();
}

void Camera::rotateY(float yawDeg) {
    this->fYaw += yawDeg;

    if(this->fYaw > 360.0f)
        this->fYaw = this->fYaw - 360.0f;
    else if(this->fYaw < 0.0f)
        this->fYaw = 360.0f + this->fYaw;

    this->updateVectors();
}

void Camera::lookAt(vec3 target) { this->lookAt(this->vPos, target); }

void Camera::lookAt(vec3 eye, vec3 target) {
    if(vec::length(eye - target) < 0.001f) return;

    this->vPos = eye;

    // https://stackoverflow.com/questions/1996957/conversion-euler-to-matrix-and-matrix-to-euler
    // https://gamedev.stackexchange.com/questions/50963/how-to-extract-euler-angles-from-transformation-matrix

    Matrix4 lookAtMatrix = this->buildMatrixLookAt(eye, target, this->vYAxis);

    // extract Euler angles from the matrix (NOTE: glm::extractEulerAngleYXZ works differently for some reason?)
    const float yaw = glm::atan2(-lookAtMatrix[8], lookAtMatrix[0]);
    const float pitch = glm::asin(-lookAtMatrix[6]);
    /// float roll = glm::atan2(lookAtMatrix[4], lookAtMatrix[5]);

    this->fYaw = 180.0f + glm::degrees(yaw);
    this->fPitch = glm::degrees(pitch);

    this->updateVectors();
}

void Camera::setType(CAMERA_TYPE camType) {
    if(camType == this->camType) return;

    this->camType = camType;

    if(this->camType == CAMERA_TYPE_ORBIT)
        this->setPos(this->vOrbitTarget);
    else
        this->vPos = this->vOrbitTarget;
}

void Camera::setPos(vec3 pos) {
    this->vOrbitTarget = pos;

    if(this->camType == CAMERA_TYPE_ORBIT)
        this->vPos = this->vOrbitTarget + this->vViewDir * -this->fOrbitDistance;
    else if(this->camType == CAMERA_TYPE_FIRST_PERSON)
        this->vPos = pos;
}

void Camera::setOrbitDistance(float orbitDistance) {
    this->fOrbitDistance = orbitDistance;
    if(this->fOrbitDistance < 0) this->fOrbitDistance = 0;
}

void Camera::setRotation(float yawDeg, float pitchDeg, float rollDeg) {
    this->fYaw = yawDeg;
    this->fPitch = pitchDeg;
    this->fRoll = rollDeg;
    this->updateVectors();
}

void Camera::setYaw(float yawDeg) {
    this->fYaw = yawDeg;
    this->updateVectors();
}

void Camera::setPitch(float pitchDeg) {
    this->fPitch = pitchDeg;
    this->updateVectors();
}

void Camera::setRoll(float rollDeg) {
    this->fRoll = rollDeg;
    this->updateVectors();
}

vec3 Camera::getNextPosition(vec3 velocity) const {
    return this->vPos + ((this->worldRotation * this->rotation) * velocity);
}

vec3 Camera::getProjectedVector(vec3 point, float screenWidth, float screenHeight, float zn, float zf) const {
    Matrix4 viewMatrix = this->buildMatrixLookAt(this->vPos, this->vPos + this->vViewDir, this->vViewUp);
    Matrix4 projectionMatrix = this->buildMatrixPerspectiveFov(this->fFov, screenWidth / screenHeight, zn, zf);

    // complete matrix
    Matrix4 viewProj = projectionMatrix * viewMatrix;

    // transform 3d point to 2d
    vec3 result = vec3TransformCoord(point, viewProj);

    // convert projected screen coordinates to real screen coordinates
    result.x = screenWidth * ((result.x + 1.0f) / 2.0f);
    result.y = screenHeight * ((1.0f - result.y) / 2.0f);  // flip Y for screen coordinates
    result.z = zn + result.z * (zf - zn);

    return result;
}

vec3 Camera::getUnProjectedVector(vec2 point, float screenWidth, float screenHeight, float zn, float zf) const {
    Matrix4 projectionMatrix = this->buildMatrixPerspectiveFov(this->fFov, screenWidth / screenHeight, zn, zf);
    Matrix4 viewMatrix = this->buildMatrixLookAt(this->vPos, this->vPos + this->vViewDir, this->vViewUp);

    // transform pick position from screen space into 3d space
    glm::vec4 viewport(0, 0, screenWidth, screenHeight);
    glm::mat4 glmModel(1.0f);  // identity model matrix
    const glm::mat4 &glmView = viewMatrix.getGLM();
    const glm::mat4 &glmProj = projectionMatrix.getGLM();

    // combine model and view matrices (required by glm::unProject)
    glm::mat4 modelView = glmView * glmModel;

    glm::vec3 unprojected = glm::unProject(glm::vec3(point.x, screenHeight - point.y, 0.0f),  // flip Y for OpenGL
                                           modelView, glmProj, viewport);

    return {unprojected.x, unprojected.y, unprojected.z};
}

bool Camera::isPointVisibleFrustum(vec3 point) const {
    const float epsilon = 0.01f;

    // left
    float d11 = this->planeDotCoord(this->viewFrustum[0], point);

    // right
    float d21 = this->planeDotCoord(this->viewFrustum[1], point);

    // top
    float d31 = this->planeDotCoord(this->viewFrustum[2], point);

    // bottom
    float d41 = this->planeDotCoord(this->viewFrustum[3], point);

    if((d11 < epsilon) || (d21 < epsilon) || (d31 < epsilon) || (d41 < epsilon)) return false;

    return true;
}

bool Camera::isPointVisiblePlane(vec3 point) const {
    constexpr float epsilon = 0.0f;  // ?

    if(!(this->planeDotCoord(this->vViewDir, this->vPos, point) < epsilon)) return true;

    return false;
}

float Camera::planeDotCoord(CAM_PLANE plane, vec3 point) {
    return ((plane.a * point.x) + (plane.b * point.y) + (plane.c * point.z) + plane.d);
}

float Camera::planeDotCoord(vec3 planeNormal, vec3 planePoint, vec3 &pv) {
    return vec::dot(planeNormal, pv - planePoint);
}
