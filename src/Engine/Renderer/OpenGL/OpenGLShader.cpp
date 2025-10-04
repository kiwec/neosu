// Copyright (c) 2016, PG, All rights reserved.
#include "OpenGLShader.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include "OpenGLHeaders.h"
#include "OpenGLStateCache.h"

#include "ConVar.h"
#include "Engine.h"
#include "Logging.h"

#include <fstream>

OpenGLShader::OpenGLShader(std::string vertexShader, std::string fragmentShader, bool source)
    : Shader(), sVsh(std::move(vertexShader)), sFsh(std::move(fragmentShader)), bSource(source) {
    this->iProgram = 0;
    this->iVertexShader = 0;
    this->iFragmentShader = 0;

    this->iProgramBackup = 0;
}

void OpenGLShader::init() { this->bReady = this->compile(this->sVsh, this->sFsh, this->bSource); }

void OpenGLShader::initAsync() { this->bAsyncReady = true; }

void OpenGLShader::destroy() {
    if(this->iProgram != 0) glDeleteObjectARB(this->iProgram);
    if(this->iFragmentShader != 0) glDeleteObjectARB(this->iFragmentShader);
    if(this->iVertexShader != 0) glDeleteObjectARB(this->iVertexShader);

    this->iProgram = 0;
    this->iFragmentShader = 0;
    this->iVertexShader = 0;

    this->iProgramBackup = 0;

    this->uniformLocationCache.clear();
}

void OpenGLShader::enable() {
    if(!this->bReady) return;

    int currentProgram = OpenGLStateCache::getCurrentProgram();
    if(currentProgram == this->iProgram) return;  // already active

    this->iProgramBackup = currentProgram;
    glUseProgramObjectARB(this->iProgram);
    OpenGLStateCache::setCurrentProgram(this->iProgram);
}

void OpenGLShader::disable() {
    if(!this->bReady) return;

    glUseProgramObjectARB(this->iProgramBackup);

    // update cache
    OpenGLStateCache::setCurrentProgram(this->iProgramBackup);
}

void OpenGLShader::setUniform1f(std::string_view name, float value) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform1fARB(id, value);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform1fv(std::string_view name, int count, const float *const values) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform1fvARB(id, count, values);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform1i(std::string_view name, int value) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform1iARB(id, value);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform2f(std::string_view name, float value1, float value2) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform2fARB(id, value1, value2);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform2fv(std::string_view name, int count, const float *const vectors) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform2fv(id, count, (const float *const)&vectors[0]);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform3f(std::string_view name, float x, float y, float z) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform3fARB(id, x, y, z);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform3fv(std::string_view name, int count, const float *const vectors) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform3fv(id, count, (const float *const)&vectors[0]);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniform4f(std::string_view name, float x, float y, float z, float w) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform4fARB(id, x, y, z, w);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniformMatrix4fv(std::string_view name, const Matrix4 &matrix) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniformMatrix4fv(id, 1, GL_FALSE, matrix.get());
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

void OpenGLShader::setUniformMatrix4fv(std::string_view name, const float *const v) {
    if(!this->bReady) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniformMatrix4fv(id, 1, GL_FALSE, v);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLShader Warning: Can't find uniform {:s}", name);
}

int OpenGLShader::getAttribLocation(std::string_view name) {
    if(!this->bReady || name.empty()) return -1;

    return glGetAttribLocation(this->iProgram, name.data());
}

int OpenGLShader::getAndCacheUniformLocation(std::string_view name) {
    if(!this->bReady || name.empty()) return -1;

    const auto cachedValue = this->uniformLocationCache.find(name);
    const bool cached = (cachedValue != this->uniformLocationCache.end());

    const int id = (cached ? cachedValue->second : glGetUniformLocationARB(this->iProgram, name.data()));
    if(!cached && id != -1) this->uniformLocationCache[std::string{name}] = id;

    return id;
}

bool OpenGLShader::compile(const std::string &vertexShader, const std::string &fragmentShader, bool source) {
    // load & compile shaders
    debugLog("Compiling {:s} ...", (source ? "vertex source" : vertexShader));
    this->iVertexShader = source ? createShaderFromString(vertexShader, GL_VERTEX_SHADER_ARB)
                                 : createShaderFromFile(vertexShader, GL_VERTEX_SHADER_ARB);
    debugLog("Compiling {:s} ...", (source ? "fragment source" : fragmentShader));
    this->iFragmentShader = source ? createShaderFromString(fragmentShader, GL_FRAGMENT_SHADER_ARB)
                                   : createShaderFromFile(fragmentShader, GL_FRAGMENT_SHADER_ARB);

    if(this->iVertexShader == 0 || this->iFragmentShader == 0) {
        engine->showMessageError("OpenGLShader Error", "Couldn't createShader()");
        return false;
    }

    // create program
    this->iProgram = glCreateProgramObjectARB();
    if(this->iProgram == 0) {
        engine->showMessageError("OpenGLShader Error", "Couldn't glCreateProgramObjectARB()");
        return false;
    }

    // attach
    glAttachObjectARB(this->iProgram, this->iVertexShader);
    glAttachObjectARB(this->iProgram, this->iFragmentShader);

    // link
    glLinkProgramARB(this->iProgram);

    int returnValue = GL_TRUE;
    glGetObjectParameterivARB(this->iProgram, GL_OBJECT_LINK_STATUS_ARB, &returnValue);
    if(returnValue == GL_FALSE) {
        engine->showMessageError("OpenGLShader Error", "Couldn't glLinkProgramARB()");
        return false;
    }

    // validate
    glValidateProgramARB(this->iProgram);
    returnValue = GL_TRUE;
    glGetObjectParameterivARB(this->iProgram, GL_OBJECT_VALIDATE_STATUS_ARB, &returnValue);
    if(returnValue == GL_FALSE) {
        engine->showMessageError("OpenGLShader Error", "Couldn't glValidateProgramARB()");
        return false;
    }

    return true;
}

int OpenGLShader::createShaderFromString(const std::string &shaderSource, int shaderType) {
    const GLhandleARB shader = glCreateShaderObjectARB(shaderType);

    if(shader == 0) {
        engine->showMessageError("OpenGLShader Error", "Couldn't glCreateShaderObjectARB()");
        return 0;
    }

    // compile shader
    const char *shaderSourceChar = shaderSource.c_str();
    glShaderSourceARB(shader, 1, &shaderSourceChar, nullptr);
    glCompileShaderARB(shader);

    int returnValue = GL_TRUE;
    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &returnValue);

    if(returnValue == GL_FALSE) {
        debugLog("------------------OpenGLShader Compile Error------------------");

        glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &returnValue);

        if(returnValue > 0) {
            char *errorLog = new char[returnValue];
            glGetInfoLogARB(shader, returnValue, &returnValue, errorLog);
            debugLog("{}", errorLog);
            delete[] errorLog;
        }

        debugLog("--------------------------------------------------------------");

        engine->showMessageError("OpenGLShader Error", "Couldn't glShaderSourceARB() or glCompileShaderARB()");
        return 0;
    }

    return static_cast<int>(shader);
}

int OpenGLShader::createShaderFromFile(const std::string &fileName, int shaderType) {
    // load file
    std::ifstream inFile(fileName);
    if(!inFile) {
        engine->showMessageError("OpenGLShader Error", fileName.c_str());
        return 0;
    }
    std::string line;
    std::string shaderSource;
    // int linecount = 0;
    while(inFile.good()) {
        std::getline(inFile, line);
        shaderSource += line + "\n\0";
        // linecount++;
    }
    shaderSource += "\n\0";
    inFile.close();

    return createShaderFromString(shaderSource.c_str(), shaderType);
}

#endif
