// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace robotick
{
	template <typename T> inline T clamp(T value, const T& lo, const T& hi)
	{
		if (value < lo)
			return lo;
		if (value > hi)
			return hi;
		return value;
	}
} // namespace robotick
