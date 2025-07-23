// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"

namespace robotick
{
	template <typename T> static bool fixed_vector_to_string(const void* data, char* out_buffer, size_t buffer_size)
	{
		const T* vec = static_cast<const T*>(data);
		if (!vec || !out_buffer || buffer_size < 32) // min size safety
			return false;

		// Print format: <FixedVector[capacity](size)>
		const size_t capacity_kb = vec->capacity() / 1024;
		const size_t used_kb = vec->size() / 1024;

		// Format string into out_buffer
		const int written = snprintf(out_buffer, buffer_size, "<FixedVector%zuk(%zuk)>", capacity_kb, used_kb);
		return written > 0 && static_cast<size_t>(written) < buffer_size;
	}

	static bool fixed_vector_from_string(const char*, void*)
	{
		return false; // not supported
	}

	ROBOTICK_REGISTER_PRIMITIVE(FixedVector1k, fixed_vector_to_string<FixedVector1k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector2k, fixed_vector_to_string<FixedVector2k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector4k, fixed_vector_to_string<FixedVector4k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector8k, fixed_vector_to_string<FixedVector8k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector16k, fixed_vector_to_string<FixedVector16k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector32k, fixed_vector_to_string<FixedVector32k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector64k, fixed_vector_to_string<FixedVector64k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector128k, fixed_vector_to_string<FixedVector128k>, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector256k, fixed_vector_to_string<FixedVector256k>, fixed_vector_from_string);

	extern "C" void robotick_force_register_fixed_vector_types()
	{
		// This function exists solely to force this TU to be retained
	}

} // namespace robotick
