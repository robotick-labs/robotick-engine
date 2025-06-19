// Copyright Robotick Labs
#pragma once

#include <cstddef>

namespace robotick
{

	inline size_t fnv1a_hash(const char* data, size_t len)
	{
		size_t hash = 2166136261u; // FNV offset basis
		for (size_t i = 0; i < len; ++i)
		{
			hash ^= static_cast<unsigned char>(data[i]);
			hash *= 16777619u; // FNV prime
		}
		return hash;
	}

} // namespace robotick