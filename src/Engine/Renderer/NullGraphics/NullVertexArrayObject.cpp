// Copyright (c) 2026, WH, All rights reserved.
#include "NullVertexArrayObject.h"

NullVertexArrayObject::NullVertexArrayObject(DrawPrimitive primitive, DrawUsageType usage, bool keepInSystemMemory)
    : VertexArrayObject(primitive, usage, keepInSystemMemory) {}

void NullVertexArrayObject::draw() {}
