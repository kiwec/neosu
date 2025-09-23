#pragma once
// Copyright (c) 2012, PG, All rights reserved.
#include "Resource.h"

struct Matrix4;

class Shader : public Resource {
   public:
    Shader() : Resource() { ; }
    ~Shader() override { ; }

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual void setUniform1f(std::string_view name, float value) = 0;
    virtual void setUniform1fv(std::string_view name, int count, float *values) = 0;
    virtual void setUniform1i(std::string_view name, int value) = 0;
    virtual void setUniform2f(std::string_view name, float x, float y) = 0;
    virtual void setUniform2fv(std::string_view name, int count, float *vectors) = 0;
    virtual void setUniform3f(std::string_view name, float x, float y, float z) = 0;
    virtual void setUniform3fv(std::string_view name, int count, float *vectors) = 0;
    virtual void setUniform4f(std::string_view name, float x, float y, float z, float w) = 0;
    virtual void setUniformMatrix4fv(std::string_view name, Matrix4 &matrix) = 0;
    virtual void setUniformMatrix4fv(std::string_view name, float *v) = 0;

    // type inspection
    [[nodiscard]] Type getResType() const final { return SHADER; }

    Shader *asShader() final { return this; }
    [[nodiscard]] const Shader *asShader() const final { return this; }

   protected:
    void init() override = 0;
    void initAsync() override = 0;
    void destroy() override = 0;
};
