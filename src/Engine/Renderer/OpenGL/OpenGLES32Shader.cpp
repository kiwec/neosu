//================ Copyright (c) 2025, WH, All rights reserved. =================//
//
// Purpose:		OpenGLES32 GLSL implementation of Shader
//
// $NoKeywords: $gles32shader
//===============================================================================//

#include "OpenGLES32Shader.h"

#ifdef MCENGINE_FEATURE_GLES32

#include "Engine.h"
#include "ConVar.h"
#include "Logging.h"

#include "OpenGLHeaders.h"
#include "OpenGLES32Interface.h"
#include "OpenGLStateCache.h"

OpenGLES32Shader::OpenGLES32Shader(const std::string &shader, [[maybe_unused]] bool source) : Shader() {
    SHADER_PARSE_RESULT parsedVertexShader = parseShaderFromString("OpenGLES32Interface::VertexShader", shader);
    SHADER_PARSE_RESULT parsedFragmentShader = parseShaderFromString("OpenGLES32Interface::FragmentShader", shader);

    m_sVsh = parsedVertexShader.source;
    m_sFsh = parsedFragmentShader.source;

    m_iProgram = 0;
    m_iVertexShader = 0;
    m_iFragmentShader = 0;

    m_iProgramBackup = 0;
}

OpenGLES32Shader::OpenGLES32Shader(const std::string &vertexShader, const std::string &fragmentShader,
                                   [[maybe_unused]] bool source)
    : Shader() {
    m_sVsh = vertexShader;
    m_sFsh = fragmentShader;

    m_iProgram = 0;
    m_iVertexShader = 0;
    m_iFragmentShader = 0;

    m_iProgramBackup = 0;
}

void OpenGLES32Shader::init() { this->setReady(this->compile(m_sVsh, m_sFsh, true)); }

void OpenGLES32Shader::initAsync() { this->setAsyncReady(true); }

void OpenGLES32Shader::destroy() {
    auto *gles32 = static_cast<OpenGLES32Interface *>(g.get());
    if(gles32 != nullptr) gles32->unregisterShader(this);

    if(m_iProgram != 0) glDeleteProgram(m_iProgram);
    if(m_iFragmentShader != 0) glDeleteShader(m_iFragmentShader);
    if(m_iVertexShader != 0) glDeleteShader(m_iVertexShader);

    m_iProgram = 0;
    m_iFragmentShader = 0;
    m_iVertexShader = 0;

    m_iProgramBackup = 0;
    m_uniformLocationCache.clear();
}

void OpenGLES32Shader::enable() {
    if(!this->isReady()) return;

    int currentProgram = static_cast<int>(GLStateCache::getCurrentProgram());
    if(currentProgram == m_iProgram)  // already active
        return;

    // use the state cache instead of querying gl directly
    m_iProgramBackup = static_cast<int>(GLStateCache::getCurrentProgram());
    glUseProgram(m_iProgram);

    // update cache
    GLStateCache::setCurrentProgram(m_iProgram);
}

void OpenGLES32Shader::disable() {
    if(!this->isReady()) return;

    glUseProgram(m_iProgramBackup);  // restore

    // update cache
    GLStateCache::setCurrentProgram(m_iProgramBackup);
}

int OpenGLES32Shader::getAndCacheUniformLocation(std::string_view name) {
    if(!this->isReady()) return -1;

    const auto cachedValue = m_uniformLocationCache.find(name);
    const bool cached = (cachedValue != m_uniformLocationCache.end());

    const int id = (cached                      ? cachedValue->second
                    : name[name.size()] == '\0' ? glGetUniformLocation(m_iProgram, name.data())
                                                : -1);
    if(!cached && id != -1) m_uniformLocationCache.emplace(name, id);

    return id;
}

void OpenGLES32Shader::setUniform1f(std::string_view name, float value) {
    if(!this->isReady()) return;

    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform1f(id, value);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform1fv(std::string_view name, int count, const float *const values) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform1fv(id, count, values);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform1i(std::string_view name, int value) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform1i(id, value);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform2f(std::string_view name, float value1, float value2) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform2f(id, value1, value2);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform2fv(std::string_view name, int count, const float *const vectors) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform2fv(id, count, (float *)&vectors[0]);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform3f(std::string_view name, float x, float y, float z) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform3f(id, x, y, z);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform3fv(std::string_view name, int count, const float *const vectors) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform3fv(id, count, (float *)&vectors[0]);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniform4f(std::string_view name, float x, float y, float z, float w) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniform4f(id, x, y, z, w);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniformMatrix4fv(std::string_view name, const Matrix4 &matrix) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniformMatrix4fv(id, 1, GL_FALSE, matrix.get());
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

void OpenGLES32Shader::setUniformMatrix4fv(std::string_view name, const float *const v) {
    if(!this->isReady()) return;
    const int id = getAndCacheUniformLocation(name);
    if(id != -1)
        glUniformMatrix4fv(id, 1, GL_FALSE, v);
    else if(cv::debug_shaders.getBool())
        debugLog("OpenGLES32Shader Warning: Can't find uniform {:s}", name);
}

int OpenGLES32Shader::getAttribLocation(std::string_view name) {
    if(!this->isReady() || name.empty()) return -1;

    return glGetAttribLocation(m_iProgram, name.data());
}

bool OpenGLES32Shader::isActive() {
    int currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    return (this->isReady() && currentProgram == m_iProgram);
}

bool OpenGLES32Shader::compile(const std::string &vertexShader, const std::string &fragmentShader, bool source) {
    // load & compile shaders
    debugLog("Compiling {:s} ...", (source ? "vertex source" : vertexShader));
    m_iVertexShader = source ? createShaderFromString(vertexShader, GL_VERTEX_SHADER)
                             : createShaderFromFile(vertexShader, GL_VERTEX_SHADER);
    debugLog("Compiling {:s} ...", (source ? "fragment source" : fragmentShader));
    m_iFragmentShader = source ? createShaderFromString(fragmentShader, GL_FRAGMENT_SHADER)
                               : createShaderFromFile(fragmentShader, GL_FRAGMENT_SHADER);

    if(m_iVertexShader == 0 || m_iFragmentShader == 0) {
        engine->showMessageError("OpenGLES32Shader Error", "Couldn't createShader()");
        return false;
    }

    // create program
    m_iProgram = glCreateProgram();
    if(m_iProgram == 0) {
        engine->showMessageError("OpenGLES32Shader Error", "Couldn't glCreateProgram()");
        return false;
    }

    // attach
    glAttachShader(m_iProgram, m_iVertexShader);
    glAttachShader(m_iProgram, m_iFragmentShader);

    // link
    glLinkProgram(m_iProgram);

    GLint ret = GL_FALSE;
    glGetProgramiv(m_iProgram, GL_LINK_STATUS, &ret);
    if(ret == GL_FALSE) {
        engine->showMessageError("OpenGLES32Shader Error", "Couldn't glLinkProgram()");
        return false;
    }

    // validate
    ret = GL_FALSE;
    glValidateProgram(m_iProgram);
    glGetProgramiv(m_iProgram, GL_VALIDATE_STATUS, &ret);
    if(ret == GL_FALSE) {
        engine->showMessageError("OpenGLES32Shader Error", "Couldn't glValidateProgram()");
        return false;
    }

    return true;
}

int OpenGLES32Shader::createShaderFromString(std::string shaderSource, int shaderType) {
    const GLint shader = glCreateShader(shaderType);

    if(shader == 0) {
        engine->showMessageError("OpenGLES32Shader Error", "Couldn't glCreateShader()");
        return 0;
    }

    // compile shader
    size_t pos = shaderSource.find("{RUNTIME_VERSION}"sv);
    if(pos != std::string::npos) {
        shaderSource.replace(pos, "{RUNTIME_VERSION}"sv.length(), "100");
    }

    const char *shaderSourceChar = shaderSource.c_str();
    glShaderSource(shader, 1, &shaderSourceChar, nullptr);
    glCompileShader(shader);

    GLint ret = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ret);
    if(ret == GL_FALSE) {
        debugLog("------------------OpenGLES32Shader Compile Error------------------");

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &ret);
        if(ret > 0) {
            char *errorLog = new char[ret];
            {
                glGetShaderInfoLog(shader, ret, &ret, errorLog);
                logRaw("{}", errorLog);
            }
            delete[] errorLog;
        }

        debugLog("-----------------------------------------------------------------");

        engine->showMessageError("OpenGLES32Shader Error", "Couldn't glShaderSource() or glCompileShader()");
        return 0;
    }

    return shader;
}

int OpenGLES32Shader::createShaderFromFile(const std::string &fileName, int shaderType) {
    // load file
    std::ifstream inFile(fileName.c_str());
    if(!inFile) {
        engine->showMessageError("OpenGLES32Shader Error", fileName);
        return 0;
    }
    std::string line;
    std::string shaderSource;
    while(inFile.good()) {
        std::getline(inFile, line);
        shaderSource += line + "\n\0";
    }
    shaderSource += "\n\0";
    inFile.close();

    std::string shaderSourcePtr = std::string(shaderSource.c_str());

    return createShaderFromString(shaderSourcePtr, shaderType);
}

#endif
