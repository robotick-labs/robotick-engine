// Copyright Robotick Labs
#pragma once

#include "robotick/framework/utility/TypeTraits.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

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

	/**
	 * @brief FNV-1a 32-bit streaming hasher. Use for layout identity / checksums.
	 *        Zero heap, platform-stable, safe for structs, types, offsets.
	 */
	struct Hash32
	{
		static constexpr uint32_t offset_basis = 2166136261u;
		static constexpr uint32_t prime = 16777619u;

		uint32_t state = offset_basis;

		void update(const void* data, size_t size)
		{
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; ++i)
			{
				state ^= bytes[i];
				state *= prime;
			}
		}

		template <typename T> void update(const T& value)
		{
			static_assert(!is_pointer_v<T>, "Hash32::update<T> must not be instantiated with pointer types");
			static_assert(is_trivially_copyable_v<T>, "Hash32::update<T> requires trivially copyable types");
			update(&value, sizeof(T));
		}

		void update_cstring(const char* str)
		{
			if (str)
			{
				update(str, ::strlen(str));
			}
		}

		uint32_t final() const { return state; }
	};
} // namespace robotick
