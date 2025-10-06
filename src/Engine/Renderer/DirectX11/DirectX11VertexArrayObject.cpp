//================ Copyright (c) 2022, PG, All rights reserved. =================//
//
// Purpose:		DirectX baking support for vao
//
// $NoKeywords: $dxvao
//===============================================================================//

#include "DirectX11VertexArrayObject.h"

#ifdef MCENGINE_FEATURE_DIRECTX11

#include "Engine.h"
#include "Logging.h"

#include "DirectX11Interface.h"

DirectX11VertexArrayObject::DirectX11VertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage,
                                                       bool keepInSystemMemory)
    : VertexArrayObject(primitive, usage, keepInSystemMemory), convertedPrimitive(primitive) {}

void DirectX11VertexArrayObject::init() {
    if(!this->bAsyncReady || this->vertices.size() < 2) return;

    auto* device = static_cast<DirectX11Interface*>(g.get())->getDevice();
    auto* context = static_cast<DirectX11Interface*>(g.get())->getDeviceContext();

    if(this->bReady) {
        const D3D11_USAGE usage = (D3D11_USAGE)usageToDirectX(this->usage);

        // TODO: somehow merge this with the partialUpdateColorIndices, annoying
        // TODO: also support converted meshes, extremely annoying. currently this will crash for converted meshes!

        // update vertex buffer

        if(this->partialUpdateVertexIndices.size() > 0) {
            for(size_t i = 0; i < this->partialUpdateVertexIndices.size(); i++) {
                const int offsetIndex = this->partialUpdateVertexIndices[i];

                this->convertedVertices[offsetIndex].pos = this->vertices[offsetIndex];

                // group by continuous chunks to reduce calls
                int numContinuousIndices = 1;
                while((i + 1) < this->partialUpdateVertexIndices.size()) {
                    if((this->partialUpdateVertexIndices[i + 1] - this->partialUpdateVertexIndices[i]) == 1) {
                        numContinuousIndices++;
                        i++;

                        this->convertedVertices[this->partialUpdateVertexIndices[i]].pos =
                            this->vertices[this->partialUpdateVertexIndices[i]];
                    } else
                        break;
                }

                if(usage == D3D11_USAGE_DEFAULT) {
                    D3D11_BOX box;
                    {
                        box.left = sizeof(DirectX11Interface::SimpleVertex) * offsetIndex;
                        box.right = box.left + (sizeof(DirectX11Interface::SimpleVertex) * numContinuousIndices);
                        box.top = 0;
                        box.bottom = 1;
                        box.front = 0;
                        box.back = 1;
                    }
                    context->UpdateSubresource(this->vertexBuffer, 0, &box, &this->convertedVertices[offsetIndex], 0,
                                               0);
                } else if(usage == D3D11_USAGE_DYNAMIC) {
                    D3D11_MAPPED_SUBRESOURCE mappedResource{};
                    if(SUCCEEDED(context->Map(this->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
                        memcpy(mappedResource.pData, &this->convertedVertices[0],
                               sizeof(DirectX11Interface::SimpleVertex) * this->convertedVertices.size());
                        context->Unmap(this->vertexBuffer, 0);
                    }
                }
            }
            this->partialUpdateVertexIndices.clear();
            this->partialUpdateColorIndices.clear();
        }

        // TODO: update color buffer
    }

    if(this->vertexBuffer != nullptr && (!this->bKeepInSystemMemory || this->bReady))
        return;  // only fully load if we are not already loaded

    // TODO: optimize this piece of shit

    this->convertedVertices.clear();
    {
        std::vector<vec3> finalVertices = this->vertices;
        std::vector<std::vector<vec2>> finalTexcoords = this->texcoords;
        std::vector<vec4> colors;
        std::vector<vec4> finalColors;

        for(auto clr : this->colors) {
            const vec4 color = vec4(clr.Rf(), clr.Gf(), clr.Bf(), clr.Af());
            colors.push_back(color);
            finalColors.push_back(color);
        }
        const size_t maxColorIndex = (finalColors.size() > 0 ? finalColors.size() - 1 : 0);

        if(this->primitive == Graphics::PRIMITIVE::PRIMITIVE_QUADS) {
            for(auto& finalTexcoord : finalTexcoords) {
                finalTexcoord.clear();
            }
            finalColors.clear();
            this->convertedPrimitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES;

            if(this->vertices.size() > 3) {
                for(size_t i = 0; i < this->vertices.size(); i += 4) {
                    finalVertices.push_back(this->vertices[i + 0]);
                    finalVertices.push_back(this->vertices[i + 1]);
                    finalVertices.push_back(this->vertices[i + 2]);

                    for(size_t t = 0; t < this->texcoords.size(); t++) {
                        finalTexcoords[t].push_back(this->texcoords[t][i + 0]);
                        finalTexcoords[t].push_back(this->texcoords[t][i + 1]);
                        finalTexcoords[t].push_back(this->texcoords[t][i + 2]);
                    }

                    if(colors.size() > 0) {
                        finalColors.push_back(colors[std::clamp<int>(i + 0, 0, maxColorIndex)]);
                        finalColors.push_back(colors[std::clamp<int>(i + 1, 0, maxColorIndex)]);
                        finalColors.push_back(colors[std::clamp<int>(i + 2, 0, maxColorIndex)]);
                    }

                    finalVertices.push_back(this->vertices[i + 0]);
                    finalVertices.push_back(this->vertices[i + 2]);
                    finalVertices.push_back(this->vertices[i + 3]);

                    for(size_t t = 0; t < this->texcoords.size(); t++) {
                        finalTexcoords[t].push_back(this->texcoords[t][i + 0]);
                        finalTexcoords[t].push_back(this->texcoords[t][i + 2]);
                        finalTexcoords[t].push_back(this->texcoords[t][i + 3]);
                    }

                    if(colors.size() > 0) {
                        finalColors.push_back(colors[std::clamp<int>(i + 0, 0, maxColorIndex)]);
                        finalColors.push_back(colors[std::clamp<int>(i + 2, 0, maxColorIndex)]);
                        finalColors.push_back(colors[std::clamp<int>(i + 3, 0, maxColorIndex)]);
                    }
                }
            }
        } else if(this->primitive == Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN) {
            finalVertices.clear();
            for(auto& finalTexcoord : finalTexcoords) {
                finalTexcoord.clear();
            }
            finalColors.clear();
            this->convertedPrimitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES;

            if(this->vertices.size() > 2) {
                for(size_t i = 2; i < this->vertices.size(); i++) {
                    finalVertices.push_back(this->vertices[0]);

                    finalVertices.push_back(this->vertices[i]);
                    finalVertices.push_back(this->vertices[i - 1]);

                    for(size_t t = 0; t < this->texcoords.size(); t++) {
                        finalTexcoords[t].push_back(this->texcoords[t][0]);
                        finalTexcoords[t].push_back(this->texcoords[t][i]);
                        finalTexcoords[t].push_back(this->texcoords[t][i - 1]);
                    }

                    if(colors.size() > 0) {
                        finalColors.push_back(colors[std::clamp<int>(0, 0, maxColorIndex)]);
                        finalColors.push_back(colors[std::clamp<int>(i, 0, maxColorIndex)]);
                        finalColors.push_back(colors[std::clamp<int>(i - 1, 0, maxColorIndex)]);
                    }
                }
            }
        }

        // build directx vertices
        {
            this->convertedVertices.resize(finalVertices.size());

            this->iNumVertices =
                this->convertedVertices.size();  // NOTE: overwrite this->iNumVertices for potential conversions

            const bool hasColors = (finalColors.size() > 0);
            const bool hasTexCoords0 =
                (finalTexcoords.size() > 0 && finalTexcoords[0].size() == this->convertedVertices.size());

            for(size_t i = 0; i < finalVertices.size(); i++) {
                this->convertedVertices[i].pos.x = finalVertices[i].x;
                this->convertedVertices[i].pos.y = finalVertices[i].y;
                this->convertedVertices[i].pos.z = finalVertices[i].z;

                if(hasColors)
                    this->convertedVertices[i].col = finalColors[std::clamp<size_t>(i, 0, maxColorIndex)];
                else {
                    this->convertedVertices[i].col.x = 1.0f;
                    this->convertedVertices[i].col.y = 1.0f;
                    this->convertedVertices[i].col.z = 1.0f;
                    this->convertedVertices[i].col.w = 1.0f;
                }

                // TODO: multitexturing
                if(hasTexCoords0) this->convertedVertices[i].tex = finalTexcoords[0][i];
            }
        }
    }

    // create buffer
    const D3D11_USAGE usage = (D3D11_USAGE)usageToDirectX(this->usage);
    D3D11_BUFFER_DESC bufferDesc{};
    {
        bufferDesc.Usage = usage;
        bufferDesc.ByteWidth = sizeof(DirectX11Interface::SimpleVertex) * this->convertedVertices.size();
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = (bufferDesc.Usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0);
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;
    }
    D3D11_SUBRESOURCE_DATA dataForImmutable{};
    {
        dataForImmutable.pSysMem = &this->convertedVertices[0];
        dataForImmutable.SysMemPitch = 0;       // (unused for vertices)
        dataForImmutable.SysMemSlicePitch = 0;  // (unused for vertices)
    }
    if(FAILED(device->CreateBuffer(&bufferDesc,
                                   (bufferDesc.Usage == D3D11_USAGE_IMMUTABLE ? &dataForImmutable : nullptr),
                                   &this->vertexBuffer)))  // NOTE: immutable is uploaded to gpu right here
    {
        debugLog("DirectX Error: Couldn't CreateBuffer({})", (int)this->convertedVertices.size());
        return;
    }

    // upload everything to gpu
    if(usage == D3D11_USAGE_DEFAULT) {
        D3D11_BOX box;
        {
            box.left = sizeof(DirectX11Interface::SimpleVertex) * 0;
            box.right = box.left + (sizeof(DirectX11Interface::SimpleVertex) * this->convertedVertices.size());
            box.top = 0;
            box.bottom = 1;
            box.front = 0;
            box.back = 1;
        }
        context->UpdateSubresource(this->vertexBuffer, 0, &box, &this->convertedVertices[0], 0, 0);
    } else if(usage == D3D11_USAGE_DYNAMIC) {
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        if(SUCCEEDED(context->Map(this->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
            memcpy(mappedResource.pData, &this->convertedVertices[0],
                   sizeof(DirectX11Interface::SimpleVertex) * this->convertedVertices.size());
            context->Unmap(this->vertexBuffer, 0);
        } else {
            debugLog("DirectX Error: Couldn't Map({}) vertexbuffer", (int)this->convertedVertices.size());
            return;
        }
    }

    // free memory
    if(!this->bKeepInSystemMemory) {
        this->clear();
        this->convertedVertices.clear();
    }

    this->bReady = true;
}

void DirectX11VertexArrayObject::initAsync() { this->bAsyncReady = true; }

void DirectX11VertexArrayObject::destroy() {
    VertexArrayObject::destroy();

    if(this->vertexBuffer != nullptr) {
        this->vertexBuffer->Release();
        this->vertexBuffer = nullptr;
    }

    this->convertedVertices.clear();
}

void DirectX11VertexArrayObject::draw() {
    if(!this->bReady) {
        debugLog("WARNING: called, but was not ready!");
        return;
    }

    const int start = std::clamp<int>(this->iDrawRangeFromIndex > -1
                                          ? this->iDrawRangeFromIndex
                                          : nearestMultipleUp((int)(this->iNumVertices * this->fDrawPercentFromPercent),
                                                              this->iDrawPercentNearestMultiple),
                                      0, this->iNumVertices);
    const int end = std::clamp<int>(this->iDrawRangeToIndex > -1
                                        ? this->iDrawRangeToIndex
                                        : nearestMultipleDown((int)(this->iNumVertices * this->fDrawPercentToPercent),
                                                              this->iDrawPercentNearestMultiple),
                                    0, this->iNumVertices);

    if(start > end || std::abs(end - start) == 0) return;

    auto* context = static_cast<DirectX11Interface*>(g.get())->getDeviceContext();

    // draw it
    {
        const UINT stride = sizeof(DirectX11Interface::SimpleVertex);
        const UINT offset = 0;

        context->IASetVertexBuffers(0, 1, &this->vertexBuffer, &stride, &offset);
        context->IASetPrimitiveTopology((D3D_PRIMITIVE_TOPOLOGY)primitiveToDirectX(this->convertedPrimitive));
        context->Draw(end - start, start);
    }
}

int DirectX11VertexArrayObject::primitiveToDirectX(Graphics::PRIMITIVE primitive) {
    switch(primitive) {
        case Graphics::PRIMITIVE::PRIMITIVE_LINES:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case Graphics::PRIMITIVE::PRIMITIVE_LINE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_FAN:  // NOTE: not available! -------------------
            return D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case Graphics::PRIMITIVE::PRIMITIVE_TRIANGLE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case Graphics::PRIMITIVE::PRIMITIVE_QUADS:  // NOTE: not available! -------------------
            return D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }

    return D3D_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

int DirectX11VertexArrayObject::usageToDirectX(Graphics::USAGE_TYPE usage) {
    switch(usage) {
        case Graphics::USAGE_TYPE::USAGE_STATIC:
            return D3D11_USAGE_IMMUTABLE;
        // NOTE: this fallthrough is intentional.
        // no performance benefits found so far with DYNAMIC, since D3D11_MAP_WRITE_NO_OVERWRITE has very limited use cases
        case Graphics::USAGE_TYPE::USAGE_DYNAMIC:
        case Graphics::USAGE_TYPE::USAGE_STREAM:
            return D3D11_USAGE_DEFAULT;
    }

    return D3D11_USAGE_IMMUTABLE;
}

#endif
