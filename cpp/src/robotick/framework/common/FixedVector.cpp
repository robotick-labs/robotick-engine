// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"
#include "robotick/framework/registry/TypeRegistry.h"

namespace robotick
{
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector1k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector2k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector4k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector8k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector16k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector32k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector64k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector128k);
	ROBOTICK_REGISTER_PRIMITIVE(FixedVector256k);

	extern "C" void robotick_force_register_fixed_vector_types()
	{
		// This function exists solely to force this TU to be retained
	}

} // namespace robotick
