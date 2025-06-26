// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry-v2/TypeDescriptor.h"

#include "robotick/api_base.h"
#include "robotick/framework/registry-v2/TypeRegistry.h"

namespace robotick
{

	const TypeDescriptor* FieldDescriptor::find_type_descriptor() const
	{
		return TypeRegistry::get().find_by_id(type_id);
	}

} // namespace robotick