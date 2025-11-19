#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#ifndef OPENGLSHADER_H
#define OPENGLSHADER_H

#include "config.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "Shader.h"

#include "templates.h"

class OpenGLShader final : public Shader {
    NOCOPY_NOMOVE(OpenGLShader)
   public:
    OpenGLShader(std::string vertexShader, std::string fragmentShader, bool source);
    ~OpenGLShader() override { destroy(); }

    void enable() override;
    void disable() override;

    void setUniform1f(std::string_view name, float value) override;
    void setUniform1fv(std::string_view name, int count, const float *const values) override;
    void setUniform1i(std::string_view name, int value) override;
    void setUniform2f(std::string_view name, float x, float y) override;
    void setUniform2fv(std::string_view name, int count, const float *const vectors) override;
    void setUniform3f(std::string_view name, float x, float y, float z) override;
    void setUniform3fv(std::string_view name, int count, const float *const vectors) override;
    void setUniform4f(std::string_view name, float x, float y, float z, float w) override;
    void setUniformMatrix4fv(std::string_view name, const Matrix4 &matrix) override;
    void setUniformMatrix4fv(std::string_view name, const float *const v) override;

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    bool compile(const std::string &vertexShader, const std::string &fragmentShader, bool source);
    int createShaderFromString(const std::string &shaderSource, int shaderType);
    int createShaderFromFile(const std::string &fileName, int shaderType);

    int getAttribLocation(std::string_view name);
    int getAndCacheUniformLocation(std::string_view name);

    std::string sVsh;
    std::string sFsh;

    bool bSource;
    int iVertexShader;
    int iFragmentShader;
    unsigned int iProgram;

    unsigned int iProgramBackup;

    sv_unordered_map<int> uniformLocationCache;
};

#endif

#endif
