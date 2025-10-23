// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <float.h> // FLT_EPSILON, DBL_EPSILON
#include <math.h>  // sqrtf, sqrt

namespace robotick
{
	template <typename T> inline T max(T a, T b)
	{
		return a > b ? a : b;
	}

	template <typename T> inline T min(T a, T b)
	{
		return a < b ? a : b;
	}

	template <typename T> inline T clamp(T v, T lo, T hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	template <typename T> inline T clamp01(T v)
	{
		return clamp(v, T(0), T(1));
	}

	inline float lerp(float a, float b, float t)
	{
		t = clamp01(t);
		return a + (b - a) * t;
	}
} // namespace robotick