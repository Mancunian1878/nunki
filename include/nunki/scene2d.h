/*
 * Nunki (simple rendering engine)
 * Copyright (C) 2015 Canio Massimo Tristano <massimo.tristano@gmail.com>.
 * For licensing info see LICENSE.
 */

#pragma once

#include "./base.h"
#include "./device.h"
#include "./font.h"

NU_HANDLE(NuScene2D);

/**
 * Write the #documentation.
 */
#define NU_IMMEDIATE_SCENE2D NULL

typedef struct
{
	NuContext context;
	NuRect2i  viewport;
} Nu2dBeginImmediateInfo;

typedef struct
{
	const NuBlendState* blendState;
} Nu2dQuadsSolidBeginInfo;

typedef struct
{
	const NuBlendState* blendState;
	const NuTexture     texture;
	const NuSampler     sampler;
} Nu2dQuadsTexturedBeginInfo;

/**
 * Write the #documentation.
 */
NUNKI_API void nu2dImmediateBegin(Nu2dBeginImmediateInfo const* info);

/**
 * Write the #documentation.
 */
NUNKI_API void nu2dImmediateEnd(void);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nuCreateScene2D(NuAllocator* allocator, NuScene2D* scene);

/**
 * Write the #documentation.
 */
NUNKI_API void nuDestroyScene2D(NuScene2D scene, NuAllocator* allocator);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dReset(NuScene2D scene, NuRect2i viewport);

/**
 * Write the #documentation.
 */
NUNKI_API void nu2dPresent(NuScene2D scene, NuContext context);


/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dBeginQuadsSolid(NuScene2D scene, const Nu2dQuadsSolidBeginInfo* info);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dQuadSolid(NuScene2D scene, NuRect2 rect, uint32_t color);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dQuadSolidEx(NuScene2D scene, NuRect2 rect, uint32_t topLeftColor, uint32_t topRightColor, uint32_t bottomLeftColor, uint32_t bottomRightColor);


/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dBeginQuadsTextured(NuScene2D scene, const Nu2dQuadsTexturedBeginInfo* info);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dQuadTextured(NuScene2D scene, NuRect2 rect, uint32_t color, NuRect2 uvRect, uint textureIndex);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dQuadTexturedEx(NuScene2D scene, NuRect2 rect, uint32_t topLeftColor, uint32_t topRightColor, uint32_t bottomLeftColor, uint32_t bottomRightColor, NuRect2 uvRect, uint textureIndex);


/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dBeginText(NuScene2D scene, NuFont font);

/**
 * Write the #documentation.
 */
NUNKI_API NuResult nu2dText(NuScene2D scene, const char* text, NuPoint2i position, NuTextStyle const* styles, uint initialStyleIndex);
