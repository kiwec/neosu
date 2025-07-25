//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		empty implementation of Shader
//
// $NoKeywords: $nshader
//===============================================================================//

#ifndef NULLSHADER_H
#define NULLSHADER_H

#include "Shader.h"

class NullShader : public Shader {
   public:
    NullShader(const std::string&  /*vertexShader*/, const std::string&  /*fragmentShader*/, bool  /*source*/) : Shader() { ; }
    ~NullShader() override { this->destroy(); }

    void enable() override { ; }
    void disable() override { ; }

    void setUniform1f(const std::string_view &name, float value) override {
        (void)name;
        (void)value;
    }
    void setUniform1fv(const std::string_view &name, int count, float *values) override {
        (void)name;
        (void)count;
        (void)values;
    }
    void setUniform1i(const std::string_view &name, int value) override {
        (void)name;
        (void)value;
    }
    void setUniform2f(const std::string_view &name, float x, float y) override {
        (void)name;
        (void)x;
        (void)y;
    }
    void setUniform2fv(const std::string_view &name, int count, float *vectors) override {
        (void)name;
        (void)count;
        (void)vectors;
    }
    void setUniform3f(const std::string_view &name, float x, float y, float z) override {
        (void)name;
        (void)x;
        (void)y;
        (void)z;
    }
    void setUniform3fv(const std::string_view &name, int count, float *vectors) override {
        (void)name;
        (void)count;
        (void)vectors;
    }
    void setUniform4f(const std::string_view &name, float x, float y, float z, float w) override {
        (void)name;
        (void)x;
        (void)y;
        (void)z;
        (void)w;
    }
    void setUniformMatrix4fv(const std::string_view &name, Matrix4 &matrix) override {
        (void)name;
        (void)matrix;
    }
    void setUniformMatrix4fv(const std::string_view &name, float *v) override {
        (void)name;
        (void)v;
    }

   private:
    void init() override { this->bReady = true; }
    void initAsync() override { this->bAsyncReady = true; }
    void destroy() override { ; }
};

#endif
