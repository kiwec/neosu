// Copyright (c) 2017, PG, All rights reserved.
#include "OpenGLVertexArrayObject.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "Engine.h"
#include "ConVar.h"

#include "SDLGLInterface.h"

OpenGLVertexArrayObject::OpenGLVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage,
                                                 bool keepInSystemMemory)
    : VertexArrayObject(primitive, usage, keepInSystemMemory) {
    this->iVertexBuffer = 0;
    this->iTexcoordBuffer = 0;
    this->iColorBuffer = 0;
    this->iNormalBuffer = 0;

    this->iNumTexcoords = 0;
    this->iNumColors = 0;
    this->iNumNormals = 0;

    this->iVertexArray = 0;
}

void OpenGLVertexArrayObject::init() {
    if(!(this->bAsyncReady.load()) || this->vertices.size() < 2) return;

    // handle partial reloads

    if(this->bReady) {
        // update vertex buffer
        if(this->partialUpdateVertexIndices.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, this->iVertexBuffer);
            for(size_t i = 0; i < this->partialUpdateVertexIndices.size(); i++) {
                const int offsetIndex = this->partialUpdateVertexIndices[i];

                // group by continuous chunks to reduce calls
                int numContinuousIndices = 1;
                while((i + 1) < this->partialUpdateVertexIndices.size()) {
                    if((this->partialUpdateVertexIndices[i + 1] - this->partialUpdateVertexIndices[i]) == 1) {
                        numContinuousIndices++;
                        i++;
                    } else
                        break;
                }

                glBufferSubData(GL_ARRAY_BUFFER, sizeof(vec3) * offsetIndex, sizeof(vec3) * numContinuousIndices,
                                &(this->vertices[offsetIndex]));
            }
            this->partialUpdateVertexIndices.clear();
        }

        // update color buffer
        if(this->partialUpdateColorIndices.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, this->iColorBuffer);
            for(size_t i = 0; i < this->partialUpdateColorIndices.size(); i++) {
                const int offsetIndex = this->partialUpdateColorIndices[i];

                this->colors[offsetIndex] = abgr(this->colors[offsetIndex]);

                // group by continuous chunks to reduce calls
                int numContinuousIndices = 1;
                while((i + 1) < this->partialUpdateColorIndices.size()) {
                    if((this->partialUpdateColorIndices[i + 1] - this->partialUpdateColorIndices[i]) == 1) {
                        numContinuousIndices++;
                        i++;

                        this->colors[this->partialUpdateColorIndices[i]] =
                            abgr(this->colors[this->partialUpdateColorIndices[i]]);
                    } else
                        break;
                }

                glBufferSubData(GL_ARRAY_BUFFER, sizeof(Color) * offsetIndex, sizeof(Color) * numContinuousIndices,
                                &(this->colors[offsetIndex]));
            }
            this->partialUpdateColorIndices.clear();
        }
    }

    if(this->iVertexBuffer != 0 && (!this->bKeepInSystemMemory || this->bReady))
        return;  // only fully load if we are not already loaded

    // handle full loads

    unsigned int vertexAttribArrayIndexCounter = 0;
    if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
        // build and bind vertex array
        glGenVertexArrays(1, &this->iVertexArray);
        glBindVertexArray(this->iVertexArray);
    }

    // build and fill vertex buffer
    {
        glGenBuffers(1, &this->iVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->iVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * this->vertices.size(), &(this->vertices[0]),
                     SDLGLInterface::usageToOpenGLMap[this->usage]);

        if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
            glEnableVertexAttribArray(vertexAttribArrayIndexCounter);
            glVertexAttribPointer(vertexAttribArrayIndexCounter, 3, GL_FLOAT, GL_FALSE, 0, (char*)nullptr);
            vertexAttribArrayIndexCounter++;
        } else {
            // NOTE: this state will persist engine-wide forever
            glEnableClientState(GL_VERTEX_ARRAY);
        }
    }

    // build and fill texcoord buffer
    if(this->texcoords.size() > 0 && this->texcoords[0].size() > 0) {
        this->iNumTexcoords = this->texcoords[0].size();

        glGenBuffers(1, &this->iTexcoordBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->iTexcoordBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * this->texcoords[0].size(), &(this->texcoords[0][0]),
                     SDLGLInterface::usageToOpenGLMap[this->usage]);

        if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
            if(this->iNumTexcoords > 0) {
                glEnableVertexAttribArray(vertexAttribArrayIndexCounter);
                glVertexAttribPointer(vertexAttribArrayIndexCounter, 2, GL_FLOAT, GL_FALSE, 0, (char*)nullptr);
                vertexAttribArrayIndexCounter++;
            }
        }
    }

    // build and fill color buffer
    if(this->colors.size() > 0) {
        this->iNumColors = this->colors.size();

        for(auto& color : this->colors) {
            color = abgr(color);
        }

        glGenBuffers(1, &this->iColorBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->iColorBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Color) * this->colors.size(), &(this->colors[0]),
                     SDLGLInterface::usageToOpenGLMap[this->usage]);

        if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
            if(this->iNumColors > 0) {
                glEnableVertexAttribArray(vertexAttribArrayIndexCounter);
                glVertexAttribPointer(vertexAttribArrayIndexCounter, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, (char*)nullptr);
                vertexAttribArrayIndexCounter++;
            }
        }
    }

    // build and fill normal buffer
    if(this->normals.size() > 0) {
        this->iNumNormals = this->normals.size();

        glGenBuffers(1, &this->iNormalBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, this->iNormalBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * this->normals.size(), &(this->normals[0]),
                     SDLGLInterface::usageToOpenGLMap[this->usage]);

        if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
            if(this->iNumNormals > 0) {
                glEnableVertexAttribArray(vertexAttribArrayIndexCounter);
                glVertexAttribPointer(vertexAttribArrayIndexCounter, 3, GL_FLOAT, GL_FALSE, 0, (char*)nullptr);
                vertexAttribArrayIndexCounter++;
            }
        }
    }

    if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
        glBindVertexArray(0);
    }

    // free memory
    if(!this->bKeepInSystemMemory) clear();

    this->bReady = true;
}

void OpenGLVertexArrayObject::initAsync() { this->bAsyncReady = true; }

void OpenGLVertexArrayObject::destroy() {
    VertexArrayObject::destroy();

    if(this->iVertexBuffer > 0) glDeleteBuffers(1, &this->iVertexBuffer);

    if(this->iTexcoordBuffer > 0) glDeleteBuffers(1, &this->iTexcoordBuffer);

    if(this->iColorBuffer > 0) glDeleteBuffers(1, &this->iColorBuffer);

    if(this->iNormalBuffer > 0) glDeleteBuffers(1, &this->iNormalBuffer);

    if(this->iVertexArray > 0) glDeleteVertexArrays(1, &this->iVertexArray);

    this->iVertexBuffer = 0;
    this->iTexcoordBuffer = 0;
    this->iColorBuffer = 0;
    this->iNormalBuffer = 0;

    this->iVertexArray = 0;
}

void OpenGLVertexArrayObject::draw() {
    if(!this->bReady) {
        debugLog("WARNING: called, but was not ready!\n");
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

    if(cv::r_opengl_legacy_vao_use_vertex_array.getBool()) {
        // set vao
        glBindVertexArray(this->iVertexArray);

        // render it
        glDrawArrays(SDLGLInterface::primitiveToOpenGLMap[this->primitive], start,
                     end - start);  // (everything is already preconfigured inside the vertexArray)
    } else {
        // set vertices
        glBindBuffer(GL_ARRAY_BUFFER, this->iVertexBuffer);
        glVertexPointer(3, GL_FLOAT, 0, (char*)nullptr);  // set vertex pointer to vertex buffer

        // set texture0
        if(this->iNumTexcoords > 0) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glBindBuffer(GL_ARRAY_BUFFER, this->iTexcoordBuffer);
            glTexCoordPointer(2, GL_FLOAT, 0, (char*)nullptr);  // set first texcoord pointer to texcoord buffer
        } else
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        // set colors
        if(this->iNumColors > 0) {
            glEnableClientState(GL_COLOR_ARRAY);
            glBindBuffer(GL_ARRAY_BUFFER, this->iColorBuffer);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, (char*)nullptr);  // set color pointer to color buffer
        } else
            glDisableClientState(GL_COLOR_ARRAY);

        // set normals
        if(this->iNumNormals > 0) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glBindBuffer(GL_ARRAY_BUFFER, this->iNormalBuffer);
            glNormalPointer(GL_FLOAT, 0, (char*)nullptr);  // set normal pointer to normal buffer
        } else
            glDisableClientState(GL_NORMAL_ARRAY);

        // render it
        glDrawArrays(SDLGLInterface::primitiveToOpenGLMap[this->primitive], start, end - start);
    }
}

#endif
