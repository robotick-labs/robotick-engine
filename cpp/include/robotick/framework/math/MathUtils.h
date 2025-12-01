// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/utility/InitializerList.h"

#include "robotick/framework/math/Clamp.h"
#include "robotick/framework/math/Trig.h"

#include <cfloat>
#include <cmath>

namespace robotick
{
	static constexpr float kPi = 3.14159265358979323846f;
	static constexpr float kHalfPi = 0.5f * kPi;
	static constexpr float kTwoPi = 2.0f * kPi;
	static constexpr float kDegToRad = kPi / 180.0f;
	static constexpr float kRadToDeg = 180.0f / kPi;

	inline float deg_to_rad(float degrees)
	{
		return degrees * kDegToRad;
	}

	inline float rad_to_deg(float radians)
	{
		return radians * kRadToDeg;
	}

	template <typename T> inline T max(robotick::initializer_list<T> values)
	{
		T result = *values.begin();
		for (T v : values)
			if (v > result)
				result = v;
		return result;
	}

	template <typename T> inline T max(const T& lhs, const T& rhs)
	{
		return (lhs > rhs) ? lhs : rhs;
	}

	template <typename T> inline T min(const T& lhs, const T& rhs)
	{
		return (lhs < rhs) ? lhs : rhs;
	}

	template <typename T> inline T min(robotick::initializer_list<T> values)
	{
		T result = *values.begin();
		for (T v : values)
			if (v < result)
				result = v;
		return result;
	}

	template <typename T> inline T clamp01(T v)
	{
		return robotick::clamp(v, T(0), T(1));
	}

	inline float lerp(float a, float b, float t)
	{
		t = clamp01(t);
		return a + (b - a) * t;
	}
} // namespace robotick
