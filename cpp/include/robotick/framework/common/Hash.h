// Copyright Robotick Labs
#pragma once

#include <cstddef>
#include <cstdint>

namespace robotick
{

	constexpr uint32_t fnv1a_hash(const char* data, size_t len)
	{
		uint32_t hash = 2166136261u; // FNV offset basis

		if (!data)
			return hash;

		for (size_t i = 0; i < len; ++i)
		{
			hash ^= static_cast<unsigned char>(data[i]);
			hash *= 16777619u; // FNV prime
		}
		return hash;
	}

} // namespace robotick