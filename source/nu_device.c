/*
 * Nunki (simple rendering engine)
 * Copyright (C) 2015 Canio Massimo Tristano <massimo.tristano@gmail.com>.
 * For licensing info see LICENSE.
 */

#include "nu_device.h"
#include "nu_base.h"
#include "nu_libs.h"
#include "thirdparty/gl3w.h"

#ifdef _WIN32
#include "nu_device_context_win32.inl"
#endif

#define MAX_NUM_CONSTANT_BUFFERS 16

/*-------------------------------------------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------------------------------------*/
typedef struct {
	uint location;
	uint stride;
	NuVertexAttributeType type;
	uint dimension;
} VertexLayoutAttribute;

typedef struct {
	bool instanced;
	uint stride;
	uint numAttributes;
	VertexLayoutAttribute attributes[NU_VERTEX_LAYOUT_MAX_STREAM_ATTRIBUTES];
} VertexLayoutStream;

typedef struct NuVertexLayoutImpl {
	uint numAttributes;
	uint numStreams;
	VertexLayoutStream streams[NU_VERTEX_LAYOUT_MAX_STREAMS];
} VertexLayout;

typedef struct {
	VertexLayout const *layout;
	GLuint programId;
	uint   numConstantBuffers;
	uint   numSamplers;
} Technique;

typedef struct NuBufferImpl {
	GLuint        id;
	NuBufferType  type;
	uint          size;
	NuBufferUsage usage;
	uint          mapped : 1;
} Buffer;

typedef struct {
	GLuint              boundBuffers[3];
	NuRect2i            viewport;
	const Technique*    technique;
	const VertexLayout* vertexLayout;
	bool                vertexLayoutIsDirty;
	uint                numActiveAttributes;
	NuBlendState        blendState;
	NuBufferView        constantBuffers[MAX_NUM_CONSTANT_BUFFERS];
	NuBufferView        vertexBuffers[NU_VERTEX_LAYOUT_MAX_STREAMS];
} State;

typedef struct NuContextImpl {
	NGlContext nglContext;
	State state;
	
} Context;

static struct {
	bool              initialized;
	NuAllocator       allocator;
	NGlContextManager nglContextManager;
	Technique*        techniques;
	State             state;
	State*            currentState;
} gDevice;

/*-------------------------------------------------------------------------------------------------
 * Globals
 *-----------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------
 * Static functions
 *-----------------------------------------------------------------------------------------------*/

static inline void BindGlobalContext(void)
{
	nGlContextManagerMakeCurrent(&gDevice.nglContextManager);
	gDevice.currentState = &gDevice.state;
}

static inline void BindContext(Context* context)
{
	nGlContextMakeCurrent(&context->nglContext);
	gDevice.currentState = &context->state;
}

#define EnforceInitialized() nEnforce(gDevice.initialized, "Device uninitialized");

static inline NuAllocator* GetDeviceAllocator(NuAllocator* allocator)
{
	return allocator ? allocator : &gDevice.allocator;
}

static inline GLenum BufferTypeToGl(NuBufferType type)
{
	return (GLenum[]) { GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_UNIFORM_BUFFER }[type];
}

static inline GLenum BufferUsageToGl(NuBufferUsage usage)
{
	return (GLenum[]) { GL_STATIC_DRAW, GL_DYNAMIC_DRAW, GL_STREAM_DRAW }[usage];
}

static inline uint GetAttributeTypeSize(NuVertexAttributeType type)
{
	switch (type) {
		case NU_VAT_INT8: case NU_VAT_UINT8: case NU_VAT_SNORM8: case NU_VAT_UNORM8: return 1;
		case NU_VAT_INT16: case NU_VAT_UINT16: case NU_VAT_SNORM16: case NU_VAT_UNORM16: return 2;
		case NU_VAT_INT32: case NU_VAT_UINT32: case NU_VAT_SNORM32: case NU_VAT_UNORM32:  return 4;
		case NU_VAT_FLOAT: return 4;
	}
	nDebugBreak();
	return 0;
}

static inline void UnpackVertexLayoutAttributeTypeToGl(NuVertexAttributeType type, GLenum *gl_type, bool *normalized, bool *integer)
{
	switch (type) {
		case NU_VAT_INT8:  *gl_type = GL_BYTE; *normalized = false; *integer = true; return;
		case NU_VAT_UINT8: *gl_type = GL_UNSIGNED_BYTE; *normalized = false; *integer = true; return;
		case NU_VAT_SNORM8: *gl_type = GL_BYTE; *normalized = true; *integer = false; return;
		case NU_VAT_UNORM8: *gl_type = GL_UNSIGNED_BYTE; *normalized = true; *integer = false; return;

		case NU_VAT_INT16:  *gl_type = GL_SHORT; *normalized = false; *integer = true; return;
		case NU_VAT_UINT16: *gl_type = GL_UNSIGNED_SHORT; *normalized = false; *integer = true; return;
		case NU_VAT_SNORM16: *gl_type = GL_SHORT; *normalized = true; *integer = false; return;
		case NU_VAT_UNORM16: *gl_type = GL_UNSIGNED_SHORT; *normalized = true; *integer = false; return;

		case NU_VAT_INT32:  *gl_type = GL_INT; *normalized = false; *integer = true; return;
		case NU_VAT_UINT32: *gl_type = GL_UNSIGNED_INT; *normalized = false; *integer = true; return;
		case NU_VAT_SNORM32: *gl_type = GL_INT; *normalized = true; *integer = false; return;
		case NU_VAT_UNORM32: *gl_type = GL_UNSIGNED_INT; *normalized = true; *integer = false; return;

		case NU_VAT_FLOAT: *gl_type = GL_FLOAT; *normalized = false; *integer = false; return;
	}
}

static inline GLenum PrimitiveTypeToGl(NuPrimitiveType primitive)
{
	return (GLenum[]) { GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_TRIANGLE_FAN, GL_TRIANGLE_STRIP, GL_TRIANGLES }[primitive];
}

static void __stdcall debugCallbackGL(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void* user_data)
{
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			nDebugError("OpenGL error: %s", message);
			break;

		case GL_DEBUG_SEVERITY_MEDIUM:
			nDebugWarning("OpenGL warning: %s", message);
			break;

		default:
			nDebugInfo("OpenGL info: %s", message);
			break;
	}
}

static void BindBuffer(const Buffer* buffer)
{
	if (gDevice.currentState->boundBuffers[buffer->type] != buffer->id)
	{
		gDevice.currentState->boundBuffers[buffer->type] = buffer->id;
		glBindBuffer(BufferTypeToGl(buffer->type), buffer->id);
	}
}

static void UnbindBuffer(const Buffer *buffer)
{
	if (gDevice.currentState->boundBuffers[buffer->type] != buffer->id)
	{
		glBindBuffer(BufferTypeToGl(buffer->type), 0);
		gDevice.currentState->boundBuffers[buffer->type] = 0;
	}
}

static inline Technique const* DeviceGetTechnique(NuTechnique techniqueIndex)
{
	nEnforce(techniqueIndex > 0, "Null technique provided.");
	nEnforce(techniqueIndex - 1 < nArrayLen(gDevice.techniques), "Invalid technique provided.");
	return &gDevice.techniques[techniqueIndex - 1];
}

/*-------------------------------------------------------------------------------------------------
 * API
 *-----------------------------------------------------------------------------------------------*/
NuResult nInitDevice(NuAllocator* allocator, void* dummyWindowHandle)
{
	nEnforce(!gDevice.initialized, "Device already initialized.");
	gDevice.initialized = true;
	gDevice.allocator = *allocator;

	NuResult result = nInitGlContextManager(&gDevice.nglContextManager, dummyWindowHandle);
	if (result) {
		nZero(&gDevice);
		return result;
	}

	BindGlobalContext();

	/* setup GL debug callback */
#ifdef _DEBUG
	if (glDebugMessageCallback) {
		//nu_debug_info("OpenGL debugging supported and enabled.");
		glDebugMessageCallback(debugCallbackGL, NULL);
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}
#endif

	return NU_SUCCESS;
}

void nDeinitDevice(void)
{
	if (!gDevice.initialized) return;
	nDeinitGlContextManager(&gDevice.nglContextManager);
	nZero(&gDevice);
}

NuResult nuCreateVertexLayout(NuVertexLayoutDesc* const desc, NuAllocator *allocator, NuVertexLayout* pLayout)
{
	EnforceInitialized();
	nEnforce(desc, "Null info provided.");
	nEnforce(desc->numStreams <= NU_VERTEX_LAYOUT_MAX_STREAMS, "Number of streams in provided 'desc' higher than maximum NU_VERTEX_LAYOUT_MAX_STREAMS.");

	*pLayout = NULL;

	VertexLayout* layout = nNew(VertexLayout, GetDeviceAllocator(allocator));
	if (!layout) return NU_FAILURE;

	layout->numStreams = desc->numStreams;
	layout->numAttributes = desc->numAttributes;

	for (uint i = 0, n = desc->numAttributes; i < n; ++i) {
		NuVertexAttributeDesc const* attrDesc = &desc->attributes[i];

		nEnforce(attrDesc->stream < desc->numStreams, "Attribute %d specified an invalid stream index (%d).", i, attrDesc->stream);
		VertexLayoutStream *stream = &layout->streams[attrDesc->stream];
		stream->instanced = desc->streams[i].instanceData;

		nEnforce(stream->numAttributes < NU_VERTEX_LAYOUT_MAX_STREAM_ATTRIBUTES, "Number of attributes in stream %d greater than NU_VERTEX_LAYOUT_MAX_STREAM_ATTRIBUTES.", i);
		VertexLayoutAttribute *attribute = &stream->attributes[stream->numAttributes++];

		attribute->location = i;
		attribute->stride = stream->stride;
		attribute->type = attrDesc->type;
		attribute->dimension = attrDesc->dimension;

		stream->stride += GetAttributeTypeSize(attrDesc->type) * attrDesc->dimension;
	}

	*pLayout = layout;
	return NU_SUCCESS;
}

void nuDestroyVertexLayout(NuVertexLayout vlayout, NuAllocator *allocator)
{
	EnforceInitialized();
	nFree(vlayout, GetDeviceAllocator(allocator));
}

static GLuint CreateShader(GLenum type, const char *source, char* message_buffer, uint message_buffer_size)
{
	/* create the shader and compile it */
	GLuint shader_id = glCreateShader(type);
	glShaderSource(shader_id, 1, &source, NULL);
	glCompileShader(shader_id);

	/* check results */
	GLint result;
	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);

	if (result == GL_FALSE) {
		GLsizei size;
		glGetShaderInfoLog(shader_id, message_buffer_size, &size, message_buffer);
		return 0;
	}

	return shader_id;
}

NuTechniqueCreateResult nuCreateTechnique(NuTechniqueCreateInfo const *info, NuTechnique *pTechnique)
{
	EnforceInitialized();
	BindGlobalContext();

	nEnforce(info, "Null info provided.");
	nEnforce(info->vertexShaderSource, "A vertex shader must have been provided.");
	nEnforce(info->fragmentShaderSource, "A fragment shader must have been provided.");
	nEnforce(info->layout, "A layout must have been provided.");

	GLuint vsId = CreateShader(GL_VERTEX_SHADER, info->vertexShaderSource, info->errorMessageBuffer, info->errorMessageBufferSize);
	if (!vsId) return NU_TECHNIQUE_CREATE_ERROR_INVALID_VERTEX_SHADER;


	GLuint gsId = 0;
	if (info->geometryShaderSource) {
		gsId = CreateShader(GL_GEOMETRY_SHADER, info->geometryShaderSource, info->errorMessageBuffer, info->errorMessageBufferSize);
		glDeleteShader(vsId);
		if (!gsId) return NU_TECHNIQUE_CREATE_ERROR_INVALID_GEOMETRY_SHADER;
	}

	GLuint fsId = CreateShader(GL_FRAGMENT_SHADER, info->fragmentShaderSource, info->errorMessageBuffer, info->errorMessageBufferSize);
	if (!fsId) {
		glDeleteShader(vsId);
		glDeleteShader(gsId);
		return NU_TECHNIQUE_CREATE_ERROR_INVALID_FRAGMENT_SHADER;
	}

	/* create program and link it */
	GLuint programId = glCreateProgram();
	glAttachShader(programId, vsId);
	if (gsId) glAttachShader(programId, gsId);
	glAttachShader(programId, fsId);
	glDeleteShader(vsId);
	if (gsId) glDeleteShader(gsId);
	glDeleteShader(fsId);
	glLinkProgram(programId);

	/* check result */
	int result;
	glGetProgramiv(programId, GL_LINK_STATUS, &result);

	if (result == GL_FALSE) {
		GLsizei written;
		glGetProgramInfoLog(programId, info->errorMessageBufferSize, &written, info->errorMessageBuffer);
		glDeleteProgram(programId);
		return NU_TECHNIQUE_CREATE_ERROR_LINK_FAILED;
	}

	glUseProgram(programId);

	Technique *technique = nArrayPush(&gDevice.techniques, &gDevice.allocator, Technique);
	*technique = (Technique) {
		.layout = info->layout,
		.programId = programId,
	};

	/* register constant buffers */
	if (info->constantBuffers) {
		for (uint i = 0; info->constantBuffers[i]; ++i) {
			const char *cbufferName = info->constantBuffers[i];
			auto index = glGetUniformBlockIndex(technique->programId, cbufferName);
			if (index == GL_INVALID_INDEX) {
				nDebugWarning("Undefined shader uniform block '%s', maybe it's unused?", cbufferName);
				++technique->numConstantBuffers; /* bump it up anyway, as this slot will be considered in use. */
			} else {
				glUniformBlockBinding(technique->programId, index, technique->numConstantBuffers++);
			}
		}
	}

	/* register samplers */
	if (info->samplers) {
		for (uint i = 0; info->samplers[i]; ++i) {
			const char* samplerName = info->samplers[i];
			auto index = glGetUniformLocation(technique->programId, samplerName);
			if (index == GL_INVALID_INDEX) {
				nDebugWarning("Undefined uniform sampler '%s', maybe it's unused?", samplerName);
				++technique->numSamplers;
			} else {
				glUniform1i(index, technique->numSamplers++);
			}
		}
	}

	*pTechnique = technique - gDevice.techniques + 1;

	return NU_SUCCESS;
}

NuResult nuCreateBuffer(NuBufferCreateInfo const* info, NuAllocator* allocator, NuBuffer *pBuffer)
{
	EnforceInitialized();
	nEnforce(info, "Null info provided.");
	nEnforce(!*pBuffer, "Buffer has already been created.");

	*pBuffer = NULL;

	GLuint id;
	glGenBuffers(1, &id);
	if (!id) {
		nDebugError("Could not create GPU device object.");
		return NU_FAILURE;
	}

	Buffer *buffer = nNew(Buffer, GetDeviceAllocator(allocator));
	buffer->id = id;
	buffer->type = info->type;
	buffer->usage = info->usage;
	buffer->size = 0;

	nuBufferUpdate((NuBuffer) { buffer }, 0, info->initialData, info->initialSize);

	*pBuffer = buffer;
	return NU_SUCCESS;
}

void nuDestroyBuffer(NuBuffer buffer, NuAllocator* allocator)
{
	EnforceInitialized();
	if (!buffer) return;

	glDeleteBuffers(1, &buffer->id);
	nFree(buffer, GetDeviceAllocator(allocator));
}

void nuBufferUpdate(NuBuffer buffer, uint offset, const void* data, uint size)
{
	EnforceInitialized();
	nEnforce(buffer, "Null buffer provided.");

	BindBuffer(buffer);
	uint newSize = max_uint(buffer->size, offset + size);

	if (buffer->size < newSize) {
		if (offset == 0) {
			glBufferData(BufferTypeToGl(buffer->type), size, data, BufferUsageToGl(buffer->usage));
		} else {
			glBufferData(BufferTypeToGl(buffer->type), newSize, NULL, BufferUsageToGl(buffer->usage));
			glBufferSubData(BufferTypeToGl(buffer->type), offset, size, data);
		}
	} else {
		glBufferSubData(BufferTypeToGl(buffer->type), offset, size, data);
	}

	buffer->size = newSize;
}

NuResult nuCreateContext(NuContextCreateInfo const* info, NuAllocator* allocator, NuContext* outContext)
{
	EnforceInitialized();
	*outContext = NULL;
	Context* context = nNew(Context, GetDeviceAllocator(allocator));
	if (!context) {
		return NU_ERROR_OUT_OF_MEMORY;
	}

	NuResult result = nInitGlContext(&gDevice.nglContextManager, info->windowHandle, &context->nglContext);
	if (result) {
		nDebugError("Could not initialize OpenGL context for specified window.");
		return result;
	}

	/* initialize opengl context */
	BindContext(context);
	glEnable(GL_BLEND);
	
	/* create default VAO */
	GLuint vao;
	glGenVertexArrays(1, &vao);
	nAssert(vao == 1);
	glBindVertexArray(vao);


	*outContext = context;
	return NU_SUCCESS;
}

void nuDestroyContext(NuContext context, NuAllocator* allocator)
{
	EnforceInitialized();
	nDeinitGlContext(&gDevice.nglContextManager, &context->nglContext);
	nFree(context, GetDeviceAllocator(allocator));
}

void nuDeviceClear(NuContext context, NuClearFlags flags, float* color4, float depth, uint stencil)
{
	EnforceInitialized();
	BindContext(context);
	nEnforce(flags != 0, "Some flag must have been set.");

	GLenum glflags = 0;
	if (flags & NU_CLEAR_COLOR) {
		glClearColor(color4[0], color4[1], color4[2], color4[3]);
		glflags |= GL_COLOR_BUFFER_BIT;
	}

	if (flags & NU_CLEAR_DEPTH) {
		glClearDepthf(depth);
		glflags |= GL_COLOR_BUFFER_BIT;
	}

	if (flags & NU_CLEAR_STENCIL) {
		glClearStencil(stencil);
		glflags |= GL_COLOR_BUFFER_BIT;
	}

	glClear(glflags);
}

void nuDeviceSetViewport(NuContext context, NuRect2i rect)
{
	EnforceInitialized();
	BindContext(context);
	if (memcmp(&rect, &gDevice.currentState->viewport, sizeof rect)) {
		gDevice.currentState->viewport = rect;
		glViewport(rect.position.x, rect.position.y, rect.size.width, rect.size.height);
	}
}

void nuDeviceSetTechnique(NuContext context, NuTechnique const techniqueIndex)
{
	EnforceInitialized();
	BindContext(context);
	Technique const *technique = DeviceGetTechnique(techniqueIndex);
	
	if (technique && gDevice.currentState->technique != technique) {
		gDevice.currentState->technique = technique;
		glUseProgram(technique->programId);
	}

	if (gDevice.currentState->vertexLayout != technique->layout) {
		/* force vertex buffer pointers to be set again */
		gDevice.currentState->vertexLayoutIsDirty = true;
		gDevice.currentState->vertexLayout = technique->layout;

		/* activate or deactivate vertex attrib pointers */
		const uint numActiveAttributes = technique->layout->numAttributes;
		if (gDevice.currentState->numActiveAttributes != numActiveAttributes) {
			uint min = min_uint(gDevice.currentState->numActiveAttributes, numActiveAttributes);
			uint max = max_uint(gDevice.currentState->numActiveAttributes, numActiveAttributes);
			for (uint i = min; i < numActiveAttributes; ++i) {
				glEnableVertexAttribArray(i);
			}
			for (uint i = numActiveAttributes; i < max; ++i) {
				glDisableVertexAttribArray(i);
			}
		}
	}
}

void nuDeviceSetBlendState(NuContext context, NuBlendState const* blendState)
{
	static const GLenum nglBlendFuncs[] = {
		GL_ZERO,
		GL_ONE,
		GL_SRC_COLOR,
		GL_ONE_MINUS_SRC_COLOR,
		GL_DST_COLOR,
		GL_ONE_MINUS_DST_COLOR,
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA,
		GL_ONE_MINUS_DST_ALPHA,
		GL_CONSTANT_COLOR,
		GL_ONE_MINUS_CONSTANT_COLOR,
		GL_CONSTANT_ALPHA,
		GL_ONE_MINUS_CONSTANT_ALPHA,
		GL_SRC_ALPHA_SATURATE,
		GL_SRC1_COLOR,
		GL_ONE_MINUS_SRC1_COLOR,
		GL_SRC1_ALPHA,
		GL_ONE_MINUS_SRC1_ALPHA,
	};

	static const GLenum nglBlendEquats[] = {
		GL_FUNC_ADD,
		GL_FUNC_SUBTRACT,
		GL_FUNC_REVERSE_SUBTRACT,
		GL_MIN,
		GL_MAX,
	};

	EnforceInitialized();
	BindContext(context);

	if (memcmp(&gDevice.currentState->blendState, blendState, sizeof *blendState)) {
		gDevice.currentState->blendState = *blendState;
		glBlendEquationSeparate(nglBlendEquats[blendState->rgbOp], nglBlendEquats[blendState->alphaOp]);
		glBlendFuncSeparate(nglBlendFuncs[blendState->srcRgbFactor], nglBlendFuncs[blendState->dstRgbFactor], nglBlendFuncs[blendState->srcAlphaFactor], nglBlendFuncs[blendState->dstAlphaFactor]);
	}
}

void nuDeviceSetVertexBuffers(NuContext context, uint base, uint count, NuBufferView const *views)
{
	EnforceInitialized();
	BindContext(context);
	State* currentState = gDevice.currentState;

	nEnforce(currentState->technique && currentState->technique->layout, "A technique with a valid input layout must be set before setting vertex buffers.");

	for (uint i = 0; i < count; ++i) {
		uint streamId = i + base;
		const NuBufferView* view = &views[i];
		const Buffer* buffer = view->buffer;

		nEnforce(streamId < currentState->technique->layout->numStreams, "Stream too large for currently bound technique input layout.");

		/* check cache first */
		if (!currentState->vertexLayoutIsDirty &&
			(currentState->vertexBuffers[streamId].buffer == buffer && currentState->vertexBuffers[streamId].offset == view->offset)) {
			continue;
		}

		currentState->vertexBuffers[streamId] = *view;

		const VertexLayout *layout = currentState->technique->layout;
		const VertexLayoutStream *stream = &layout->streams[streamId];

		BindBuffer(buffer);

		for (uint i = 0; i < stream->numAttributes; ++i) {
			const VertexLayoutAttribute *attribute = &stream->attributes[i];

			GLenum gl_type;
			bool normalized;
			bool is_integer;
			UnpackVertexLayoutAttributeTypeToGl(attribute->type, &gl_type, &normalized, &is_integer);

			const void *offset_ptr = (void*)(uintptr_t)(view->offset + attribute->stride);

			if (is_integer) {
				glVertexAttribIPointer(attribute->location, attribute->dimension, gl_type, stream->stride, offset_ptr);
			} else {
				glVertexAttribPointer(attribute->location, attribute->dimension, gl_type, normalized, stream->stride, offset_ptr);
			}

			glVertexAttribDivisor(attribute->location, stream->instanced != 0);
		}
	}

	currentState->vertexLayoutIsDirty = false;
}

void nuDeviceSetConstantBuffers(NuContext context, uint base, uint count, NuBufferView const *views)
{
	EnforceInitialized();
	BindContext(context);

	for (uint i = 0; i < count; ++i) {
		uint index = i + base;
		const NuBufferView* view = &views[index];
		NuBufferView* cache = &gDevice.currentState->constantBuffers[index];
		uint size = view->size ? view->size : view->buffer->size;
		if (cache->buffer != view->buffer || cache->offset != view->offset || cache->size != size) {
			glBindBufferRange(GL_UNIFORM_BUFFER, index, view->buffer->id, view->offset, size);
			*cache = *view;
			cache->size = size;
		}
	}
}

void nuDeviceDrawArrays(NuContext context, NuPrimitiveType primitive, uint firstVertex, uint numVertices, uint numInstances)
{
	EnforceInitialized();
	BindContext(context);

	nEnforce(gDevice.currentState->technique, "No valid technique bound to the device.");
	nEnforce(gDevice.currentState->vertexLayout, "No valid vertex layout bound to the device.");

	if (gDevice.currentState->vertexLayoutIsDirty) {
		nuDeviceSetVertexBuffers(context, 0, gDevice.currentState->numActiveAttributes, gDevice.currentState->vertexBuffers);
	}

	glDrawArraysInstanced(PrimitiveTypeToGl(primitive), firstVertex, numVertices, numInstances);
}

void nuDeviceDrawIndexed(NuContext context, NuPrimitiveType primitive, NuIndexBufferView indexBuffer, uint firstVertex, uint numIndices, uint numInstances, uint baseVertex)
{
	EnforceInitialized();
	BindContext(context);

	nEnforce(gDevice.currentState->technique, "No valid technique bound to the device.");
	nEnforce(gDevice.currentState->vertexLayout, "No valid vertex layout bound to the device.");

	if (gDevice.currentState->vertexLayoutIsDirty) {
		nuDeviceSetVertexBuffers(context, 0, gDevice.currentState->numActiveAttributes, gDevice.currentState->vertexBuffers);
	}

	BindBuffer(indexBuffer.bufferView.buffer);

	GLenum glIndexType = (GLenum[]) { GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT }[indexBuffer.indexType];
	glDrawElementsInstancedBaseVertex(PrimitiveTypeToGl(primitive), numIndices, glIndexType, (const void*)(uintptr_t)indexBuffer.bufferView.offset, numInstances, baseVertex);
}

void nuDeviceSwapBuffers(NuContext context)
{
	nGlContextSwapBuffers(&context->nglContext);
}
