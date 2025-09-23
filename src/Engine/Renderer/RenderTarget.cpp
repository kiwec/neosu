// Copyright (c) 2013, PG, All rights reserved.
#include "RenderTarget.h"

#include "ConVar.h"
#include "Engine.h"
#include "VertexArrayObject.h"
#include "Logging.h"

RenderTarget::RenderTarget(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType)
    : Resource(),
      vao1(g->createVertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES, Graphics::USAGE_TYPE::USAGE_DYNAMIC,
                                      true)),
      vao2(g->createVertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES, Graphics::USAGE_TYPE::USAGE_DYNAMIC,
                                      true)),
      vao3(g->createVertexArrayObject(Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES, Graphics::USAGE_TYPE::USAGE_DYNAMIC,
                                      true)),
      vPos(vec2{x, y}),
      vSize(width, height),
      multiSampleType(multiSampleType) {}

RenderTarget::~RenderTarget() = default;

void RenderTarget::draw(int x, int y) {
    if(!this->bReady) {
        if(cv::debug_rt.getBool()) debugLog("WARNING: RenderTarget is not ready!");
        return;
    }

    g->setColor(this->color);

    this->bind();
    {
        // all draw*() functions of the RenderTarget class guarantee correctly flipped images
        // if bind() is used, no guarantee can be made about the texture orientation (assuming an anonymous
        // Renderer)
        static std::vector<vec3> vertices(6, vec3{0.f, 0.f, 0.f});

        // clang-format off
        std::vector<vec3> newVertices = {
            {x, y, 0.f},
            {x, y + this->vSize.y, 0.f},
            {x + this->vSize.x, y + this->vSize.y, 0.f},
            {x + this->vSize.x, y + this->vSize.y, 0.f},
            {x + this->vSize.x, y, 0.f},
            {x, y, 0.f}
        };
        // clang-format on

        if(!this->vao1->isReady() || vertices != newVertices) {
            this->vao1->release();

            vertices = newVertices;

            this->vao1->setVertices(vertices);

            static std::vector<vec2> texcoords(
                {vec2{0.f, 1.f}, vec2{0.f, 0.f}, vec2{1.f, 0.f}, vec2{1.f, 0.f}, vec2{1.f, 1.f}, vec2{0.f, 1.f}});

            this->vao1->setTexcoords(texcoords);

            this->vao1->loadAsync();
            this->vao1->load();
        }

        g->drawVAO(this->vao1.get());
    }
    this->unbind();
}

void RenderTarget::draw(int x, int y, int width, int height) {
    if(!this->bReady) {
        if(cv::debug_rt.getBool()) debugLog("WARNING: RenderTarget is not ready!");
        return;
    }

    g->setColor(this->color);

    this->bind();
    {
        static std::vector<vec3> vertices(6, vec3{0.f, 0.f, 0.f});

        // clang-format off
        std::vector<vec3> newVertices = {
            {x, y, 0.f},
            {x, y + height, 0.f},
            {x + width, y + height, 0.f},
            {x + width, y + height, 0.f},
            {x + width, y, 0.f},
            {x, y, 0.f}
        };
        // clang-format on

        if(!this->vao2->isReady() || vertices != newVertices) {
            this->vao2->release();

            vertices = newVertices;

            this->vao2->setVertices(vertices);

            static std::vector<vec2> texcoords(
                {vec2{0.f, 1.f}, vec2{0.f, 0.f}, vec2{1.f, 0.f}, vec2{1.f, 0.f}, vec2{1.f, 1.f}, vec2{0.f, 1.f}});

            this->vao2->setTexcoords(texcoords);

            this->vao2->loadAsync();
            this->vao2->load();
        }

        g->drawVAO(this->vao2.get());
    }
    this->unbind();
}

void RenderTarget::drawRect(int x, int y, int width, int height) {
    if(!this->bReady) {
        if(cv::debug_rt.getBool()) debugLog("WARNING: RenderTarget is not ready!");
        return;
    }

    const float texCoordWidth0 = x / this->vSize.x;
    const float texCoordWidth1 = (x + width) / this->vSize.x;
    const float texCoordHeight1 = 1.0f - y / this->vSize.y;
    const float texCoordHeight0 = 1.0f - (y + height) / this->vSize.y;

    g->setColor(this->color);

    this->bind();
    {
        static std::vector<vec3> vertices(6, vec3{0.f, 0.f, 0.f});
        static std::vector<vec2> texcoords(6, vec2{0.f, 0.f});

        // clang-format off
        std::vector<vec3> newVertices = {
            {x, y, 0.f},
            {x, y + height, 0.f},
            {x + width, y + height, 0.f},
            {x + width, y + height, 0.f},
            {x + width, y, 0.f},
            {x, y, 0.f}
        };
        // clang-format on

        std::vector<vec2> newTexcoords = {{texCoordWidth0, texCoordHeight1}, {texCoordWidth0, texCoordHeight0},
                                          {texCoordWidth1, texCoordHeight0}, {texCoordWidth1, texCoordHeight0},
                                          {texCoordWidth1, texCoordHeight1}, {texCoordWidth0, texCoordHeight1}};

        if(!this->vao3->isReady() || vertices != newVertices || texcoords != newTexcoords) {
            this->vao3->release();

            texcoords = newTexcoords;
            vertices = newVertices;

            this->vao3->setVertices(vertices);
            this->vao3->setTexcoords(texcoords);

            this->vao3->loadAsync();
            this->vao3->load();
        }

        g->drawVAO(this->vao3.get());
    }
    this->unbind();
}

void RenderTarget::rebuild(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType) {
    this->vPos.x = x;
    this->vPos.y = y;
    this->vSize.x = width;
    this->vSize.y = height;
    this->multiSampleType = multiSampleType;

    this->reload();
    this->vao1->release();
    this->vao2->release();
    this->vao3->release();
}

void RenderTarget::rebuild(int x, int y, int width, int height) {
    this->rebuild(x, y, width, height, this->multiSampleType);
}

void RenderTarget::rebuild(int width, int height) {
    this->rebuild(this->vPos.x, this->vPos.y, width, height, this->multiSampleType);
}

void RenderTarget::rebuild(int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType) {
    this->rebuild(this->vPos.x, this->vPos.y, width, height, multiSampleType);
}
