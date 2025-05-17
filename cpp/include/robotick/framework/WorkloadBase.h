#pragma once

#include "robotick/framework/FixedString.h"

namespace robotick
{
	struct WorkloadBase
	{
		FixedString32 unique_name;
		double tick_rate_hz = 0.0;
	};
} // namespace robotick
