//================ Copyright (c) 2025, WH, All rights reserved. =================//
//
// Purpose:		OpenGLES 3.2 GLSL implementation of Shader
//
// $NoKeywords: $gles32shader
//===============================================================================//

#pragma once
#ifndef OPENGLES32SHADER_H
#define OPENGLES32SHADER_H

#include "config.h"

#ifdef MCENGINE_FEATURE_GLES32

#include "Shader.h"
#include "templates.h"

class OpenGLES32Shader final : public Shader {
    NOCOPY_NOMOVE(OpenGLES32Shader)
   public:
    OpenGLES32Shader(const std::string &shader, bool source);
    OpenGLES32Shader(const std::string &vertexShader, const std::string &fragmentShader, bool source);  // DEPRECATED
    ~OpenGLES32Shader() override { destroy(); }

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

    int getAttribLocation(std::string_view name);

    // ILLEGAL:
    bool isActive();

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    bool compile(const std::string &vertexShader, const std::string &fragmentShader, bool source);
    int createShaderFromString(std::string shaderSource, int shaderType);
    int createShaderFromFile(const std::string &fileName, int shaderType);
    int getAndCacheUniformLocation(std::string_view name);

    std::string m_sVsh, m_sFsh;

    int m_iVertexShader;
    int m_iFragmentShader;
    int m_iProgram;

    int m_iProgramBackup;

    sv_unordered_map<int> m_uniformLocationCache;
};

#endif

#endif
