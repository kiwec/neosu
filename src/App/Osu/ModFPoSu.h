#pragma once
// Copyright (c) 2019, Colin Brook & PG, All rights reserved.
#include "cbase.h"

#include <list>
#include <memory>

class Camera;
class ConVar;
class Image;
class Shader;
class VertexArrayObject;

class ModFPoSu3DModel;

class ModFPoSu {
   public:
    static constexpr const float SIZEDIV3D = 1.0f / 512.0f;  // 1.0f / (float)GameRules::OSU_COORD_WIDTH

   private:
    static constexpr const int SUBDIVISIONS = 4;

   public:
    ModFPoSu();
    ~ModFPoSu();

    void draw();
    void update();

    void onKeyDown(KeyboardEvent &key);
    void onKeyUp(KeyboardEvent &key);

    //[[nodiscard]] inline const Camera *getCamera() const { return this->camera; }

    [[nodiscard]] inline float getEdgeDistance() const { return this->fEdgeDistance; }
    [[nodiscard]] inline bool isCrosshairIntersectingScreen() const { return this->bCrosshairIntersectsScreen; }
    [[nodiscard]] float get3DPlayfieldScale() const;

    [[nodiscard]] inline Shader *getHitcircleShader() const { return this->hitcircleShader; }
    void resetCamera();

   private:
    void handleZoomedChange();
    void noclipMove();

    void handleInputOverrides(bool required);
    void setMousePosCompensated(vec2 newMousePos);
    vec2 intersectRayMesh(vec3 pos, vec3 dir);
    vec3 calculateUnProjectedVector(vec2 pos);

    void makePlayfield();
    void makeBackgroundCube();
    void handleLazyLoad3DModels();

    void onCurvedChange();
    void onDistanceChange();
    void onNoclipChange();

   private:
    struct VertexPair {
        vec3 a{0.f};
        vec3 b{0.f};
        float textureCoordinate;
        vec3 normal{0.f};

        VertexPair(vec3 a, vec3 b, float tc) : a(a), b(b), textureCoordinate(tc) { ; }
    };

   private:
    static float subdivide(std::list<VertexPair> &meshList, const std::list<VertexPair>::iterator &begin,
                           const std::list<VertexPair>::iterator &end, int n, float edgeDistance);
    static vec3 normalFromTriangle(vec3 p1, vec3 p2, vec3 p3);

   private:
    VertexArrayObject *vao;
    VertexArrayObject *vaoCube;

    std::list<VertexPair> meshList;
    float fCircumLength;

    Matrix4 modelMatrix;
    std::unique_ptr<Camera> camera{nullptr};
    vec3 vPrevNoclipCameraPos{0.f};
    bool bKeyLeftDown;
    bool bKeyUpDown;
    bool bKeyRightDown;
    bool bKeyDownDown;
    bool bKeySpaceDown;
    bool bKeySpaceUpDown;
    vec3 vVelocity{0.f};
    bool bZoomKeyDown;
    bool bZoomed;
    float fZoomFOVAnimPercent;

    float fEdgeDistance;
    bool bCrosshairIntersectsScreen;
    bool bAlreadyWarnedAboutRawInputOverride;

    std::unique_ptr<ModFPoSu3DModel> skyboxModel;

    Shader *hitcircleShader;
};

class ModFPoSu3DModel {
   public:
    ModFPoSu3DModel(const UString &objFilePath, Image *texture = nullptr)
        : ModFPoSu3DModel(objFilePath, texture, false) {
        ;
    }
    ModFPoSu3DModel(const UString &objFilePathOrContents, Image *texture, bool source);
    ~ModFPoSu3DModel();

    void draw3D();

   private:
    VertexArrayObject *vao;
    Image *texture;
};
