/*
 * Nunki (simple rendering engine)
 * Copyright (C) 2015 Canio Massimo Tristano <massimo.tristano@gmail.com>.
 * For licensing info see LICENSE.
 */

#pragma  once

#include "./base.h"

#define NU_VERTEX_LAYOUT_MAX_STREAM_ATTRIBUTES 16
#define NU_VERTEX_LAYOUT_MAX_STREAMS 8

NU_HANDLE(NuContext);
NU_HANDLE(NuVertexLayout);
NU_HANDLE_INDEX(NuTechnique);
NU_HANDLE(NuBuffer);

typedef enum
{
	NU_CLEAR_COLOR = 1,
	NU_CLEAR_DEPTH = 2,
	NU_CLEAR_STENCIL = 4,
} NuClearFlags;

typedef enum
{
	NU_PRIMITIVE_POINTS,
	NU_PRIMITIVE_LINES,
	NU_PRIMITIVE_LINE_LOOP,
	NU_PRIMITIVE_TRIANGLE_FAN,
	NU_PRIMITIVE_TRIANGLE_STRIP,
	NU_PRIMITIVE_TRIANGLES,
} NuPrimitiveType;

typedef enum
{
	NU_VAT_INT8,
	NU_VAT_UINT8,
	NU_VAT_SNORM8,
	NU_VAT_UNORM8,
	NU_VAT_INT16,
	NU_VAT_UINT16,
	NU_VAT_SNORM16,
	NU_VAT_UNORM16,
	NU_VAT_INT32,
	NU_VAT_UINT32,
	NU_VAT_SNORM32,
	NU_VAT_UNORM32,
	NU_VAT_FLOAT
} NuVertexAttributeType;

typedef enum
{
	NU_BUFFER_TYPE_VERTEX,
	NU_BUFFER_TYPE_INDEX,
	NU_BUFFER_TYPE_CONSTANT,
} NuBufferType;

typedef enum
{
	NU_BUFFER_USAGE_IMMUTABLE,
	NU_BUFFER_USAGE_DYNAMIC,
	NU_BUFFER_USAGE_STREAM,
} NuBufferUsage;

typedef enum {
	NU_TECHNIQUE_CREATE_SUCCESS,
	NU_TECHNIQUE_CREATE_ERROR_INVALID_VERTEX_SHADER,
	NU_TECHNIQUE_CREATE_ERROR_INVALID_GEOMETRY_SHADER,
	NU_TECHNIQUE_CREATE_ERROR_INVALID_FRAGMENT_SHADER,
	NU_TECHNIQUE_CREATE_ERROR_LINK_FAILED,
} NuTechniqueCreateResult;

typedef struct {
	NuBuffer buffer;
	uint offset;
	uint size; /* unused for vertex buffers */
} NuBufferView;

typedef enum {
	NU_UNSIGNED_BYTE,
	NU_UNSIGNED_SHORT,
	NU_UNSIGNED_INT,
} NuIndexType;

typedef struct {
	NuBufferView bufferView;
	NuIndexType indexType;
} NuIndexBufferView;

typedef struct {
	bool instanceData;
} NuVertexStreamDesc;

typedef struct
{
	uint stream;
	NuVertexAttributeType type;
	uint dimension;
} NuVertexAttributeDesc;

typedef struct
{
	NuVertexStreamDesc const* streams;
	NuVertexAttributeDesc const* attributes;
	uint numStreams;
	uint numAttributes;
} NuVertexLayoutDesc;

typedef struct {
	char* errorMessageBuffer;
	uint  errorMessageBufferSize;
	NuVertexLayout layout;
	const char *vertexShaderSource;
	const char *geometryShaderSource;
	const char *fragmentShaderSource;
	const char **constantBuffers; /* null or null terminated */
	const char **samplers; /* null or null terminated */
} NuTechniqueCreateInfo;

typedef struct
{
	NuBufferType type;
	NuBufferUsage usage;
	uint initialSize;
	void const *initialData;
} NuBufferCreateInfo;

typedef struct {
	void* windowHandle;
} NuContextCreateInfo;

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nuCreateVertexLayout(NuVertexLayoutDesc* const info, NuAllocator *allocator, NuVertexLayout* pLayout);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDestroyVertexLayout(NuVertexLayout vlayout, NuAllocator *allocator);

/**
 * Write the #documentation.
 */
NUNKI_API NuTechniqueCreateResult nuCreateTechnique(NuTechniqueCreateInfo const *info, NuTechnique *pTechnique);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nuCreateBuffer(NuBufferCreateInfo const* info, NuAllocator* allocator, NuBuffer *buffer);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDestroyBuffer(NuBuffer buffer, NuAllocator* allocator);

/**
 * Write the #documentation.
 */
NUNKI_API void nuBufferUpdate(NuBuffer buffer, uint offset, const void* data, uint size);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nuCreateContext(NuContextCreateInfo const* info, NuAllocator* allocator, NuContext* context);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDestroyContext(NuContext context, NuAllocator* allocator);

/**
 * Write the #documentation.
 */
// NUNKI_API NuViewport nuGetContextViewport(NuContext handle)

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceClear(NuContext context, NuClearFlags flags, float* color4, float depth, uint stencil);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceSetViewport(NuContext context, NuRect2i viewport);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceSetTechnique(NuContext context, NuTechnique const technique);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceSetVertexBuffers(NuContext context, uint base, uint count, NuBufferView const *views);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceSetConstantBuffers(NuContext context, uint base, uint count, NuBufferView const *views);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceDrawArrays(NuContext context, NuPrimitiveType primitive, uint firstVertex, uint numVertices, uint numInstances);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceDrawIndexed(NuContext context, NuPrimitiveType primitive, NuIndexBufferView indexBuffer, uint firstVertex, uint numIndices, uint numInstances, uint baseVertex);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDeviceSwapBuffers(NuContext context);
