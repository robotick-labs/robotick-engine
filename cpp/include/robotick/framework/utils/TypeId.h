// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace robotick
{

	// Lightweight type-safe identifier for types, replacing std::type_index
	struct TypeId
	{
		constexpr explicit TypeId(uint32_t v = 0) : value(v) {}

		constexpr bool operator==(TypeId other) const { return value == other.value; }
		constexpr bool operator!=(TypeId other) const { return value != other.value; }
		constexpr operator uint32_t() const { return value; }

		static constexpr TypeId invalid() { return TypeId{0}; }
		constexpr bool is_valid() const { return value != 0; }

		uint32_t value;
	};

	// Default fallback: unregistered type returns 0
	template <typename T> constexpr TypeId get_type_id()
	{
		return TypeId{0};
	}

// Helper macro to register a type with a fixed numeric ID
// (In future, replace with constexpr hash if desired)
#define ROBOTICK_DEFINE_TYPE_ID(Type, Id)                                                                                                            \
	namespace robotick                                                                                                                               \
	{                                                                                                                                                \
		template <> constexpr TypeId get_type_id<Type>()                                                                                             \
		{                                                                                                                                            \
			return TypeId{Id};                                                                                                                       \
		}                                                                                                                                            \
	}

	// Optional: also register stringified name alongside ID
	template <typename T> constexpr const char* get_registered_type_name()
	{
		return "<unknown>";
	}

#define ROBOTICK_DEFINE_TYPENAME(Type, Name)                                                                                                         \
	namespace robotick                                                                                                                               \
	{                                                                                                                                                \
		template <> constexpr const char* get_registered_type_name<Type>()                                                                           \
		{                                                                                                                                            \
			return Name;                                                                                                                             \
		}                                                                                                                                            \
	}

// Combined macro for convenience
#define ROBOTICK_REGISTER_TYPE(Type, Name, Id)                                                                                                       \
	ROBOTICK_DEFINE_TYPENAME(Type, Name)                                                                                                             \
	ROBOTICK_DEFINE_TYPE_ID(Type, Id)

} // namespace robotick
