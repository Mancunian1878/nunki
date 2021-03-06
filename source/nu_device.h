/*
 * Nunki (simple rendering engine)
 * Copyright (C) 2015 Canio Massimo Tristano <massimo.tristano@gmail.com>.
 * For licensing info see LICENSE.
 */

#pragma once

#include "nunki/device.h"

/**
 * Write the #documentation.
 */
NuResult nInitDevice(NuAllocator* allocator, void* windowHandle);

/**
 * Write the #documentation.
 */
void nDeinitDevice(void);
