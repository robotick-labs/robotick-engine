// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace robotick
{
	/// @brief Minimal pair implementation (no STL required).
	template <typename TFirst, typename TSecond> struct Pair
	{
		TFirst first;
		TSecond second;

		constexpr Pair() = default;

		constexpr Pair(const TFirst& a, const TSecond& b) : first(a), second(b)
		{
		}

		constexpr bool operator==(const Pair& other) const
		{
			return first == other.first && second == other.second;
		}

		constexpr bool operator!=(const Pair& other) const
		{
			return !(*this == other);
		}
	};
} // namespace robotick
