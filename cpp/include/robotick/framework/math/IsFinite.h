// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <cmath>

namespace robotick
{
	inline bool isfinite(float value)
	{
		return robotick::std_approved::isfinite(value);
	}
	inline bool isfinite(double value)
	{
		return robotick::std_approved::isfinite(value);
	}

} // namespace robotick
