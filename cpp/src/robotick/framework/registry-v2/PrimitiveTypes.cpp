// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/registry-v2/TypeDescriptor.h"
#include "robotick/framework/registry-v2/TypeMacros.h"
#include "robotick/framework/registry-v2/TypeRegistry.h"

#include <cstdio>
#include <ctype.h>

namespace robotick
{
	template <typename T> bool scan_value(const char* str, const char* format, T* out)
	{
		return sscanf(str, format, out) == 1;
	}

	template <typename T> bool print_value(char* out, size_t size, const char* format, const T& value)
	{
		return snprintf(out, size, format, value) >= 0;
	}

	// register int: =====

	static bool int_to_string(const void* data, char* out, size_t size)
	{
		return print_value(out, size, "%d", *reinterpret_cast<const int*>(data));
	}

	static bool int_from_string(const char* str, void* out)
	{
		return scan_value(str, "%d", reinterpret_cast<int*>(out));
	}

	ROBOTICK_REGISTER_PRIMITIVE(int, int_to_string, int_from_string);

	// register float: =====

	static bool float_to_string(const void* data, char* out, size_t size)
	{
		return print_value(out, size, "%f", *reinterpret_cast<const float*>(data));
	}

	static bool float_from_string(const char* str, void* out)
	{
		return scan_value(str, "%f", reinterpret_cast<float*>(out));
	}

	ROBOTICK_REGISTER_PRIMITIVE(float, float_to_string, float_from_string);

	// register double: =====

	static bool double_to_string(const void* data, char* out, size_t size)
	{
		return print_value(out, size, "%lf", *reinterpret_cast<const double*>(data));
	}

	static bool double_from_string(const char* str, void* out)
	{
		return scan_value(str, "%lf", reinterpret_cast<double*>(out));
	}

	ROBOTICK_REGISTER_PRIMITIVE(double, double_to_string, double_from_string);

	// register bool: =====

	inline bool str_eq_case_insensitive(const char* a, const char* b)
	{
		while (*a && *b)
		{
			if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b)))
				return false;
			++a;
			++b;
		}
		return *a == *b;
	}

	static bool bool_to_string(const void* data, char* out, size_t size)
	{
		const bool value = *reinterpret_cast<const bool*>(data);
		const char* str = value ? "true" : "false";
		return snprintf(out, size, "%s", str) >= 0;
	}

	static bool bool_from_string(const char* str, void* out)
	{
		if (str_eq_case_insensitive(str, "true"))
		{
			*reinterpret_cast<bool*>(out) = true;
			return true;
		}
		if (str_eq_case_insensitive(str, "false"))
		{
			*reinterpret_cast<bool*>(out) = false;
			return true;
		}

		int i = 0;
		if (sscanf(str, "%d", &i) == 1)
		{
			*reinterpret_cast<bool*>(out) = (i != 0);
			return true;
		}

		return false;
	}

	ROBOTICK_REGISTER_PRIMITIVE(bool, bool_to_string, bool_from_string);

	// register FixedString<N>: =====

	template <size_t N> static bool fixed_string_to_string(const void* data, char* out, size_t size)
	{
		using FS = FixedString<N>;
		const auto* str = reinterpret_cast<const FS*>(data);
		return snprintf(out, size, "%s", str->c_str()) >= 0;
	}

	template <size_t N> static bool fixed_string_from_string(const char* str, void* out)
	{
		using FS = FixedString<N>;
		auto* result = reinterpret_cast<FS*>(out);
		*result = FS(str);
		return true;
	}

	template <size_t N> static constexpr TypeDescriptor make_fixed_string_desc(const char* name)
	{
		using FS = FixedString<N>;
		return {name, TypeId(name), sizeof(FS), alignof(FS), TypeDescriptor::TypeCategory::Primitive, {}, &fixed_string_to_string<N>,
			&fixed_string_from_string<N>};
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

} // namespace robotick
