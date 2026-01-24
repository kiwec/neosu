#pragma once
// Copyright (c) 2017, PG, All rights reserved.
#ifndef OPENGLVERTEXARRAYOBJECT_H
#define OPENGLVERTEXARRAYOBJECT_H

#include "config.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "VertexArrayObject.h"

class OpenGLVertexArrayObject final : public VertexArrayObject {
    NOCOPY_NOMOVE(OpenGLVertexArrayObject)
   public:
    OpenGLVertexArrayObject(DrawPrimitive primitive = DrawPrimitive::TRIANGLES,
                            DrawUsageType usage = DrawUsageType::STATIC, bool keepInSystemMemory = false);
    ~OpenGLVertexArrayObject() override { destroy(); }

    void draw() override;

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    unsigned int iVertexBuffer;
    unsigned int iTexcoordBuffer;
    unsigned int iColorBuffer;
    unsigned int iNormalBuffer;

    unsigned int iNumTexcoords;
    unsigned int iNumColors;
    unsigned int iNumNormals;

    unsigned int iVertexArray;
};

#endif

#endif
