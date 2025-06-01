// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace robotick
{
	// Compile-time FNV-1a hash (32-bit)
	constexpr uint32_t fnv1a_32(const char* str, uint32_t hash = 0x811C9DC5)
	{
		return (*str) ? fnv1a_32(str + 1, (hash ^ static_cast<uint32_t>(*str)) * 0x01000193) : hash;
	}

	struct TypeId
	{
		// The only allowed constructor: from string
		constexpr explicit TypeId(const char* type_name)
			: value(fnv1a_32(type_name))
#ifdef ROBOTICK_DEBUG_TYPEID_NAMES
			  ,
			  name(type_name)
#endif
		{
		}

		constexpr bool operator==(TypeId other) const { return value == other.value; }
		constexpr bool operator!=(TypeId other) const { return value != other.value; }
		constexpr operator uint32_t() const { return value; }

		static constexpr TypeId invalid() { return TypeId{"<invalid>"}; }
		constexpr bool is_valid() const { return value != 0; }

		uint32_t value;

#ifdef ROBOTICK_DEBUG_TYPEID_NAMES
		const char* name;
#endif
	};

#define GET_TYPE_ID(Type)                                                                                                                            \
	::robotick::TypeId                                                                                                                               \
	{                                                                                                                                                \
		#Type                                                                                                                                        \
	}

} // namespace robotick
