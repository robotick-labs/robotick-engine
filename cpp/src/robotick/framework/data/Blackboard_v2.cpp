// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard_v2.h"

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/registry-v2/TypeMacros.h"

namespace robotick
{
	const StructDescriptor* Blackboard_v2::resolve_descriptor(const void* instance)
	{
		const Blackboard_v2* blackboard = static_cast<const Blackboard_v2*>(instance);
		return blackboard ? &(blackboard->get_struct_descriptor()) : nullptr;
	}

	ROBOTICK_REGISTER_DYNAMIC_STRUCT(Blackboard_v2, Blackboard_v2::resolve_descriptor)

	void Blackboard_v2::initialize_fields(const ArrayView<FieldDescriptor>& fields)
	{
		info.struct_descriptor.fields = fields;
	}

} // namespace robotick