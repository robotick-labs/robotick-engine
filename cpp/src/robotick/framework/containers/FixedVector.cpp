// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"

namespace robotick
{
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector1k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector2k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector4k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector8k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector16k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector32k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector64k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector128k, uint8_t)
	ROBOTICK_REGISTER_FIXED_VECTOR(FixedVector256k, uint8_t)

	extern "C" void robotick_force_register_fixed_vector_types()
	{
		// This function exists solely to force this TU to be retained
	}

} // namespace robotick
