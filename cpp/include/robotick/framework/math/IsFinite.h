// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <math.h>

namespace robotick
{
	inline bool isfinite(float value)
	{
		return ::isfinite(value);
	}
	inline bool isfinite(double value)
	{
		return ::isfinite(value);
	}

} // namespace robotick
