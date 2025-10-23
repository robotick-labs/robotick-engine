// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <float.h> // FLT_EPSILON, DBL_EPSILON
#include <math.h>  // sqrtf, sqrt

namespace robotick
{
	inline float max(float a, float b)
	{
		return a > b ? a : b;
	}

	inline float min(float a, float b)
	{
		return a < b ? a : b;
	}

	inline float clamp(float v, float lo, float hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	inline float clamp01(float v)
	{
		return clamp(v, 0.0f, 1.0f);
	}

	inline float lerp(float a, float b, float t)
	{
		t = clamp01(t);
		return a + (b - a) * t;
	}
} // namespace robotick