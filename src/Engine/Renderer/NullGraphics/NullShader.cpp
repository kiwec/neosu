// Copyright (c) 2026, WH, All rights reserved.
#include "NullShader.h"

NullShader::NullShader() : Shader() {}

void NullShader::enable() {}
void NullShader::disable() {}
void NullShader::setUniform1f(std::string_view /*name*/, float /*value*/) {}
void NullShader::setUniform1fv(std::string_view /*name*/, int /*count*/, const float *const /*values*/) {}
void NullShader::setUniform1i(std::string_view /*name*/, int /*value*/) {}
void NullShader::setUniform2f(std::string_view /*name*/, float /*x*/, float /*y*/) {}
void NullShader::setUniform2fv(std::string_view /*name*/, int /*count*/, const float *const /*vectors*/) {}
void NullShader::setUniform3f(std::string_view /*name*/, float /*x*/, float /*y*/, float /*z*/) {}
void NullShader::setUniform3fv(std::string_view /*name*/, int /*count*/, const float *const /*vectors*/) {}
void NullShader::setUniform4f(std::string_view /*name*/, float /*x*/, float /*y*/, float /*z*/, float /*w*/) {}
void NullShader::setUniformMatrix4fv(std::string_view /*name*/, const Matrix4 & /*matrix*/) {}
void NullShader::setUniformMatrix4fv(std::string_view /*name*/, const float *const /*v*/) {}

void NullShader::init() { this->setReady(true); }
void NullShader::initAsync() { this->setAsyncReady(true); }
void NullShader::destroy() {}
