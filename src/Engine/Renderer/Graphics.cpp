// Copyright (c) 2016, PG, All rights reserved.
#include "Graphics.h"

#include <cmath>

#include "Camera.h"
#include "ConVar.h"
#include "Engine.h"

Graphics::Graphics() {
    // init matrix stacks
    this->bTransformUpToDate = false;
    this->worldTransformStack.emplace();
    this->projectionTransformStack.emplace();

    // init 3d gui scene stack
    this->bIs3dScene = false;
    this->scene3d_stack.push(false);
}

void Graphics::pushTransform() {
    this->worldTransformStack.emplace(this->worldTransformStack.top());
    this->projectionTransformStack.emplace(this->projectionTransformStack.top());
}

void Graphics::popTransform() {
    if(this->worldTransformStack.size() < 2) {
        engine->showMessageErrorFatal("World Transform Stack Underflow", "Too many pop*()s!");
        engine->shutdown();
        return;
    }

    if(this->projectionTransformStack.size() < 2) {
        engine->showMessageErrorFatal("Projection Transform Stack Underflow", "Too many pop*()s!");
        engine->shutdown();
        return;
    }

    this->worldTransformStack.pop();
    this->projectionTransformStack.pop();
    this->bTransformUpToDate = false;
}

void Graphics::translate(float x, float y, float z) {
    this->worldTransformStack.top().translate(x, y, z);
    this->bTransformUpToDate = false;
}

void Graphics::rotate(float deg, float x, float y, float z) {
    this->worldTransformStack.top().rotate(deg, x, y, z);
    this->bTransformUpToDate = false;
}

void Graphics::scale(float x, float y, float z) {
    this->worldTransformStack.top().scale(x, y, z);
    this->bTransformUpToDate = false;
}

void Graphics::translate3D(float x, float y, float z) {
    Matrix4 translation;
    translation.translate(x, y, z);
    this->setWorldMatrixMul(translation);
}

void Graphics::rotate3D(float deg, float x, float y, float z) {
    Matrix4 rotation;
    rotation.rotate(deg, x, y, z);
    this->setWorldMatrixMul(rotation);
}

void Graphics::setWorldMatrix(Matrix4 &worldMatrix) {
    this->worldTransformStack.pop();
    this->worldTransformStack.push(worldMatrix);
    this->bTransformUpToDate = false;
}

void Graphics::setWorldMatrixMul(Matrix4 &worldMatrix) {
    this->worldTransformStack.top() *= worldMatrix;
    this->bTransformUpToDate = false;
}

void Graphics::setProjectionMatrix(Matrix4 &projectionMatrix) {
    this->projectionTransformStack.pop();
    this->projectionTransformStack.push(projectionMatrix);
    this->bTransformUpToDate = false;
}

Matrix4 Graphics::getWorldMatrix() { return this->worldTransformStack.top(); }

Matrix4 Graphics::getProjectionMatrix() { return this->projectionTransformStack.top(); }

void Graphics::push3DScene(McRect region) {
    if(cv::r_debug_disable_3dscene.getBool()) return;

    // you can't yet stack 3d scenes!
    if(this->scene3d_stack.top()) {
        this->scene3d_stack.push(false);
        return;
    }

    // reset & init
    this->v3dSceneOffset.x = this->v3dSceneOffset.y = this->v3dSceneOffset.z = 0;
    float fov = 60.0f;

    // push true, set region
    this->bIs3dScene = true;
    this->scene3d_stack.push(true);
    this->scene3d_region = region;

    // backup transforms
    this->pushTransform();

    // calculate height to fit viewport angle
    float angle = (180.0f - fov) / 2.0f;
    float b = (engine->getScreenHeight() / std::sin(glm::radians(fov))) * std::sin(glm::radians(angle));
    float hc = std::sqrt(std::pow(b, 2.0f) - std::pow((engine->getScreenHeight() / 2.0f), 2.0f));

    // set projection matrix
    Matrix4 trans2 = Matrix4().translate(-1 + (region.getWidth()) / (float)engine->getScreenWidth() +
                                             (region.getX() * 2) / (float)engine->getScreenWidth(),
                                         1 - region.getHeight() / (float)engine->getScreenHeight() -
                                             (region.getY() * 2) / (float)engine->getScreenHeight(),
                                         0);
    Matrix4 projectionMatrix =
        trans2 * Camera::buildMatrixPerspectiveFov(
                     glm::radians(fov), ((float)engine->getScreenWidth()) / ((float)engine->getScreenHeight()),
                     cv::r_3dscene_zn.getFloat(), cv::r_3dscene_zf.getFloat());
    this->scene3d_projection_matrix = projectionMatrix;

    // set world matrix
    Matrix4 trans = Matrix4().translate(-(float)region.getWidth() / 2 - region.getX(),
                                        -(float)region.getHeight() / 2 - region.getY(), 0);
    this->scene3d_world_matrix = Camera::buildMatrixLookAt(vec3(0, 0, -hc), vec3(0, 0, 0), vec3(0, -1, 0)) * trans;

    // force transform update
    this->updateTransform(true);
}

void Graphics::pop3DScene() {
    if(!this->scene3d_stack.top()) return;

    this->scene3d_stack.pop();

    // restore transforms
    this->popTransform();

    this->bIs3dScene = false;
}

void Graphics::translate3DScene(float x, float y, float z) {
    if(!this->scene3d_stack.top()) return;  // block if we're not in a 3d scene

    // translate directly
    this->scene3d_world_matrix.translate(x, y, z);

    // force transform update
    this->updateTransform(true);
}

void Graphics::rotate3DScene(float rotx, float roty, float rotz) {
    if(!this->scene3d_stack.top()) return;  // block if we're not in a 3d scene

    // first translate to the center of the 3d region, then rotate, then translate back
    Matrix4 rot;
    vec3 centerVec = vec3(this->scene3d_region.getX() + this->scene3d_region.getWidth() / 2 + this->v3dSceneOffset.x,
                          this->scene3d_region.getY() + this->scene3d_region.getHeight() / 2 + this->v3dSceneOffset.y,
                          this->v3dSceneOffset.z);
    rot.translate(-centerVec);

    // rotate
    if(rotx != 0) rot.rotateX(-rotx);
    if(roty != 0) rot.rotateY(-roty);
    if(rotz != 0) rot.rotateZ(-rotz);

    rot.translate(centerVec);

    // apply the rotation
    this->scene3d_world_matrix = this->scene3d_world_matrix * rot;

    // force transform update
    this->updateTransform(true);
}

void Graphics::offset3DScene(float x, float y, float z) { this->v3dSceneOffset = vec3(x, y, z); }

void Graphics::forceUpdateTransform() { this->updateTransform(); }

void Graphics::updateTransform(bool force) {
    if(!this->bTransformUpToDate || force) {
        this->worldMatrix = this->worldTransformStack.top();
        this->projectionMatrix = this->projectionTransformStack.top();

        // HACKHACK: 3d gui scenes
        if(this->bIs3dScene) {
            this->worldMatrix = this->scene3d_world_matrix * this->worldTransformStack.top();
            this->projectionMatrix = this->scene3d_projection_matrix;
        }

        this->MP = this->projectionMatrix * this->worldMatrix;

        this->onTransformUpdate();

        this->bTransformUpToDate = true;
    }
}

void Graphics::checkStackLeaks() {
    if(this->worldTransformStack.size() > 1) {
        engine->showMessageErrorFatal("World Transform Stack Leak", "Make sure all push*() have a pop*()!");
        engine->shutdown();
    }

    if(this->projectionTransformStack.size() > 1) {
        engine->showMessageErrorFatal("Projection Transform Stack Leak", "Make sure all push*() have a pop*()!");
        engine->shutdown();
    }

    if(this->scene3d_stack.size() > 1) {
        engine->showMessageErrorFatal("3DScene Stack Leak", "Make sure all push*() have a pop*()!");
        engine->shutdown();
    }
}
