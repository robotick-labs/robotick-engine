// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"

namespace robotick
{
	static bool fixed_vector_to_string(const void*, char*, size_t)
	{
		return false; // not supported
	}

	static bool fixed_vector_from_string(const char*, void*)
	{
		return false; // not supported
	}

	ROBOTICK_REGISTER_PRIMITIVE(FixedVector1k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector2k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector4k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector8k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector16k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector32k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector64k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector128k, fixed_vector_to_string, fixed_vector_from_string);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector256k, fixed_vector_to_string, fixed_vector_from_string);

} // namespace robotick