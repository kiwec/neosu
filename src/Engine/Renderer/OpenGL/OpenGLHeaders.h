#pragma once
// Copyright (c) 2015, PG, All rights reserved.
#ifndef OPENGLHEADERS_H
#define OPENGLHEADERS_H

#ifndef __EMSCRIPTEN__
#include "glad/glad.h"
#else
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3platform.h>
#endif

#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX			0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX		0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX	0x9049

#define VBO_FREE_MEMORY_ATI								0x87FB
#define TEXTURE_FREE_MEMORY_ATI							0x87FC
#define RENDERBUFFER_FREE_MEMORY_ATI					0x87FD

#ifdef MCENGINE_FEATURE_OPENGL
	#include "OpenGLInterface.h"
	#include "OpenGLVertexArrayObject.h"
	#include "OpenGLShader.h"

	using BackendGLInterface = OpenGLInterface;
	using BackendGLVAO = OpenGLVertexArrayObject;
	using BackendGLShader = OpenGLShader;
#elif defined(MCENGINE_FEATURE_GLES32)
	#include "OpenGLES32Interface.h"
	#include "OpenGLES32VertexArrayObject.h"
	#include "OpenGLES32Shader.h"

	using BackendGLInterface = OpenGLES32Interface;
	using BackendGLVAO = OpenGLES32VertexArrayObject;
	using BackendGLShader = OpenGLES32Shader;
#endif

#endif
