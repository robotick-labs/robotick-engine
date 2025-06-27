// Copyright Robotick Labs
#pragma once

#include <cstddef>
#include <cstdint>

namespace robotick
{
	/**
	 * @brief Computes the 32-bit FNV-1a hash of a null-terminated C-string.
	 *
	 * @param str Null-terminated input string.
	 * @return 32-bit hash value.
	 */
	constexpr uint32_t hash_string(const char* str)
	{
		uint32_t hash = 2166136261u; // FNV offset basis
		if (!str)
			return hash;

		while (*str)
		{
			hash ^= static_cast<unsigned char>(*str++);
			hash *= 16777619u; // FNV prime
		}
		return hash;
	}
} // namespace robotick
