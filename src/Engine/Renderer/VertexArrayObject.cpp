// Copyright (c) 2016, PG, All rights reserved.
#include "VertexArrayObject.h"

#include "Engine.h"

VertexArrayObject::VertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage, bool keepInSystemMemory)
    : Resource() {
    this->primitive = primitive;
    this->usage = usage;
    this->bKeepInSystemMemory = keepInSystemMemory;

    this->iNumVertices = 0;
    this->bHasTexcoords = false;

    this->iDrawRangeFromIndex = -1;
    this->iDrawRangeToIndex = -1;
    this->iDrawPercentNearestMultiple = 0;
    this->fDrawPercentFromPercent = 0.0f;
    this->fDrawPercentToPercent = 1.0f;
}

void VertexArrayObject::init() {
    // this->bReady may only be set in inheriting classes, if baking was successful
}

void VertexArrayObject::initAsync() { this->bAsyncReady = true; }

void VertexArrayObject::destroy() {
    this->clear();

    this->iNumVertices = 0;
    this->bHasTexcoords = false;
}

void VertexArrayObject::clear() {
    this->vertices.clear();
    this->texcoords.clear();
    this->normals.clear();
    this->colors.clear();

    this->partialUpdateVertexIndices.clear();
    this->partialUpdateColorIndices.clear();

    // NOTE: do NOT set m_iNumVertices to 0! (also don't change m_bHasTexcoords)
}

void VertexArrayObject::addVertex(vec2 v) {
    this->vertices.emplace_back(v.x, v.y, 0);
    this->iNumVertices = this->vertices.size();
}

void VertexArrayObject::addVertex(float x, float y, float z) {
    this->vertices.emplace_back(x, y, z);
    this->iNumVertices = this->vertices.size();
}

void VertexArrayObject::addVertex(vec3 v) {
    this->vertices.push_back(v);
    this->iNumVertices = this->vertices.size();
}

void VertexArrayObject::addTexcoord(float u, float v) {
    this->texcoords.emplace_back(u, v);
    this->bHasTexcoords = true;
}

void VertexArrayObject::addTexcoord(vec2 uv) {
    this->texcoords.push_back(uv);
    this->bHasTexcoords = true;
}

void VertexArrayObject::addNormal(vec3 normal) { this->normals.push_back(normal); }

void VertexArrayObject::addNormal(float x, float y, float z) { this->normals.emplace_back(x, y, z); }

void VertexArrayObject::addColor(Color color) { this->colors.push_back(color); }

void VertexArrayObject::setVertex(int index, vec2 v) {
    if(index < 0 || index > (this->vertices.size() - 1)) return;

    this->vertices[index].x = v.x;
    this->vertices[index].y = v.y;

    this->partialUpdateVertexIndices.push_back(index);
}

void VertexArrayObject::setVertex(int index, vec3 v) {
    if(index < 0 || index > (this->vertices.size() - 1)) return;

    this->vertices[index].x = v.x;
    this->vertices[index].y = v.y;
    this->vertices[index].z = v.z;

    this->partialUpdateVertexIndices.push_back(index);
}

void VertexArrayObject::setVertex(int index, float x, float y, float z) {
    if(index < 0 || index > (this->vertices.size() - 1)) return;

    this->vertices[index].x = x;
    this->vertices[index].y = y;
    this->vertices[index].z = z;

    this->partialUpdateVertexIndices.push_back(index);
}

void VertexArrayObject::setTexcoords(const std::vector<vec2>& texcoords) {
    this->texcoords = texcoords;
    this->bHasTexcoords = !texcoords.empty();
}

void VertexArrayObject::setColor(int index, Color color) {
    if(index < 0 || index > (this->colors.size() - 1)) return;

    this->colors[index] = color;

    this->partialUpdateColorIndices.push_back(index);
}

void VertexArrayObject::setType(Graphics::PRIMITIVE primitive) { this->primitive = primitive; }

void VertexArrayObject::setDrawRange(int fromIndex, int toIndex) {
    this->iDrawRangeFromIndex = fromIndex;
    this->iDrawRangeToIndex = toIndex;
}

void VertexArrayObject::setDrawPercent(float fromPercent, float toPercent, int nearestMultiple) {
    this->fDrawPercentFromPercent = std::clamp<float>(fromPercent, 0.0f, 1.0f);
    this->fDrawPercentToPercent = std::clamp<float>(toPercent, 0.0f, 1.0f);
    this->iDrawPercentNearestMultiple = nearestMultiple;
}

void VertexArrayObject::reserve(size_t vertexCount) {
    this->vertices.reserve(vertexCount);
    this->texcoords.reserve(vertexCount);
}

int VertexArrayObject::nearestMultipleUp(int number, int multiple) {
    if(multiple == 0) return number;

    int remainder = number % multiple;
    if(remainder == 0) return number;

    return number + multiple - remainder;
}

int VertexArrayObject::nearestMultipleDown(int number, int multiple) {
    if(multiple == 0) return number;

    int remainder = number % multiple;
    if(remainder == 0) return number;

    return number - remainder;
}
