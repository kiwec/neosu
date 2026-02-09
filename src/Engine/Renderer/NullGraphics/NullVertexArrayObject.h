// Copyright (c) 2026, WH, All rights reserved.
// dummy VAO
#pragma once
#include "VertexArrayObject.h"

class NullVertexArrayObject : public VertexArrayObject {
   public:
    NullVertexArrayObject(DrawPrimitive primitive = DrawPrimitive::TRIANGLES,
                          DrawUsageType usage = DrawUsageType::STATIC, bool keepInSystemMemory = false);

    void draw() override;
};
