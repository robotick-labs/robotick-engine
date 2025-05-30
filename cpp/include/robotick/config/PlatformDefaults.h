// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/config/PlatformDefines.h"

namespace robotick
{
	// DEFAULT_MAX_BLACKBOARDS_BYTES
	//
	// During engine setup, we construct all workloads into a single WorkloadsBuffer.
	// However, we can only determine blackboard memory requirements *after*
	// constructing and pre-loading the workloads (since that's when PythonWorkloads
	// or other dynamic workloads reveal their schema).
	//
	// To solve this "chicken-and-egg" problem, we pre-reserve an estimated maximum
	// number of bytes for all blackboards, appended after the workloads.
	// This lets us compute and bind blackboards in-place after preload, without
	// reallocating or copying buffers — a simple, robust fix for all platforms.
	//
	// If this default is too small, a runtime exception will be thrown. You can
	// override it with a config option per deployment target.

#if defined(ROBOTICK_PLATFORM_DESKTOP)
	constexpr size_t DEFAULT_MAX_BLACKBOARDS_BYTES = 128 * 1024; // 128 KB
#elif defined(ROBOTICK_PLATFORM_MOBILE)
	constexpr size_t DEFAULT_MAX_BLACKBOARDS_BYTES = 64 * 1024; // 64 KB
#elif defined(ROBOTICK_PLATFORM_EMBEDDED)
	constexpr size_t DEFAULT_MAX_BLACKBOARDS_BYTES = 8 * 1024; // 8 KB
#else
	constexpr size_t DEFAULT_MAX_BLACKBOARDS_BYTES = 16 * 1024; // Fallback for unknown
#endif

} // namespace robotick
