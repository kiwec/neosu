// Copyright (c) 2016, PG, All rights reserved.
#ifndef VERTEXARRAYOBJECT_H
#define VERTEXARRAYOBJECT_H

#include "Resource.h"
#include "Graphics.h"

class VertexArrayObject : public Resource {
    NOCOPY_NOMOVE(VertexArrayObject)
   public:
    VertexArrayObject(DrawPrimitive primitive = DrawPrimitive::PRIMITIVE_TRIANGLES,
                      DrawUsageType usage = DrawUsageType::USAGE_STATIC, bool keepInSystemMemory = false);
    ~VertexArrayObject() override = default;

    void clear();

    void addVertex(vec2 v);
    void addVertex(vec3 v);
    void addVertex(float x, float y, float z = 0);
    void addVertices(std::vector<vec3> vertices);

    void addTexcoord(vec2 uv);
    void addTexcoord(float u, float v);
    void addTexcoords(std::vector<vec2> texcoords);

    void addNormal(vec3 normal);
    void addNormal(float x, float y, float z);
    void addNormals(std::vector<vec3> normals);

    void addColor(Color color);
    void addColors(std::vector<Color> color);

    void setVertex(int index, vec2 v);
    void setVertex(int index, vec3 v);
    void setVertex(int index, float x, float y, float z = 0);
    inline void setVertices(const std::vector<vec3> &vertices) {
        this->vertices = vertices;
        this->iNumVertices = this->vertices.size();
    }
    void setTexcoords(const std::vector<vec2> &texcoords);
    inline void setNormals(const std::vector<vec3> &normals) { this->normals = normals; }
    inline void setColors(const std::vector<Color> &colors) { this->colors = colors; }
    void setColor(int index, Color color);

    void setType(DrawPrimitive primitive);
    void setDrawRange(int fromIndex, int toIndex);
    void setDrawPercent(float fromPercent = 0.0f, float toPercent = 1.0f, int nearestMultiple = 0);  // DEPRECATED

    // optimization: pre-allocate space to avoid reallocations during batch operations
    void reserve(size_t vertexCount);

    [[nodiscard]] inline DrawPrimitive getPrimitive() const { return this->primitive; }
    [[nodiscard]] inline DrawUsageType getUsage() const { return this->usage; }

    [[nodiscard]] const std::vector<vec3> &getVertices() const { return this->vertices; }
    [[nodiscard]] const std::vector<vec2> &getTexcoords() const { return this->texcoords; }
    [[nodiscard]] const std::vector<vec3> &getNormals() const { return this->normals; }
    [[nodiscard]] const std::vector<Color> &getColors() const { return this->colors; }

    [[nodiscard]] inline unsigned int getNumVertices() const { return this->iNumVertices; }
    [[nodiscard]] inline bool hasTexcoords() const { return this->bHasTexcoords; }

    virtual void draw() { assert(false); }  // implementation dependent (gl/dx11/etc.)

    VertexArrayObject *asVAO() final { return this; }
    [[nodiscard]] const VertexArrayObject *asVAO() const final { return this; }

   protected:
    static int nearestMultipleUp(int number, int multiple);
    static int nearestMultipleDown(int number, int multiple);

    void init() override;
    void initAsync() override;
    void destroy() override;

    std::vector<vec3> vertices;
    std::vector<vec2> texcoords;
    std::vector<vec3> normals;
    std::vector<Color> colors;

    std::vector<int> partialUpdateVertexIndices;
    std::vector<int> partialUpdateColorIndices;

    unsigned int iNumVertices;
    int iDrawRangeFromIndex;
    int iDrawRangeToIndex;
    int iDrawPercentNearestMultiple;
    float fDrawPercentFromPercent;
    float fDrawPercentToPercent;

    DrawPrimitive primitive;
    DrawUsageType usage;
    bool bKeepInSystemMemory;
    bool bHasTexcoords;
};

#endif
