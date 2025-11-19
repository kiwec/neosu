//================ Copyright (c) 2022, PG, All rights reserved. =================//
//
// Purpose:		DirectX baking support for vao
//
// $NoKeywords: $dxvao
//===============================================================================//

#pragma once
#ifndef DIRECTX11VERTEXARRAYOBJECT_H
#define DIRECTX11VERTEXARRAYOBJECT_H

#include "config.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "VertexArrayObject.h"

#include "DirectX11Interface.h"

class DirectX11VertexArrayObject final : public VertexArrayObject {
    NOCOPY_NOMOVE(DirectX11VertexArrayObject)
   public:
    DirectX11VertexArrayObject(Graphics::PRIMITIVE primitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES,
                               Graphics::USAGE_TYPE usage = Graphics::USAGE_TYPE::USAGE_STATIC,
                               bool keepInSystemMemory = false);
    ~DirectX11VertexArrayObject() override { destroy(); }

    void draw() override;

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    static int primitiveToDirectX(Graphics::PRIMITIVE primitive);
    static int usageToDirectX(Graphics::USAGE_TYPE usage);

    std::vector<DirectX11Interface::SimpleVertex> convertedVertices;

    ID3D11Buffer *vertexBuffer{};

    Graphics::PRIMITIVE convertedPrimitive;
};

#endif

#endif
