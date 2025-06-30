#pragma once

#include <cstddef>
#include <limits>

namespace robotick
{
	static constexpr size_t OFFSET_UNBOUND = std::numeric_limits<size_t>::max();

	constexpr float kFloatEpsilon = 1e-6f;
} // namespace robotick