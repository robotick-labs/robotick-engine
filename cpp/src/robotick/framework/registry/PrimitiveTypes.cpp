// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/strings/FixedString.h"

#include <cstdio>
#include <ctype.h>

namespace robotick
{

	ROBOTICK_REGISTER_PRIMITIVE(int);
	ROBOTICK_REGISTER_PRIMITIVE(uint8_t);
	ROBOTICK_REGISTER_PRIMITIVE(uint16_t);
	ROBOTICK_REGISTER_PRIMITIVE(uint32_t);
	ROBOTICK_REGISTER_PRIMITIVE(uint64_t);
	ROBOTICK_REGISTER_PRIMITIVE(float);
	ROBOTICK_REGISTER_PRIMITIVE(double);
	ROBOTICK_REGISTER_PRIMITIVE(bool);

	// register FixedString<N>: =====

	template <size_t N> static constexpr TypeDescriptor make_fixed_string_desc(const char* name)
	{
		using FS = FixedString<N>;
		return {name, TypeId(name), sizeof(FS), alignof(FS), TypeCategory::Primitive, {}, "text/plain"};
	}

#define REGISTER_FIXED_STRING(N)                                                                                                                     \
	static constexpr TypeDescriptor s_fixed_string_##N##_desc = make_fixed_string_desc<N>("FixedString" #N);                                         \
	static const AutoRegisterType s_register_fixed_string_##N(s_fixed_string_##N##_desc);

	REGISTER_FIXED_STRING(8)
	REGISTER_FIXED_STRING(16)
	REGISTER_FIXED_STRING(32)
	REGISTER_FIXED_STRING(64)
	REGISTER_FIXED_STRING(128)
	REGISTER_FIXED_STRING(256)
	REGISTER_FIXED_STRING(512)
	REGISTER_FIXED_STRING(1024)

#undef REGISTER_FIXED_STRING

	extern "C" void robotick_force_register_primitives()
	{
		// This function exists solely to force this TU to be retained
	}

} // namespace robotick
