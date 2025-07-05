// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/registry/TypeMacros.h"

namespace robotick
{
	const StructDescriptor* Blackboard::resolve_descriptor(const void* instance)
	{
		const Blackboard* blackboard = static_cast<const Blackboard*>(instance);
		return blackboard ? &(blackboard->get_struct_descriptor()) : nullptr;
	}

	ROBOTICK_REGISTER_DYNAMIC_STRUCT(Blackboard, Blackboard::resolve_descriptor)

	void Blackboard::initialize_fields(const HeapVector<FieldDescriptor>& fields)
	{
		// TODO - remove the const-cast - should work fine as const (StructDescriptor may need some adjusting)
		info.struct_descriptor.fields.use(const_cast<FieldDescriptor*>(fields.data()), fields.size());
	}

	void Blackboard::initialize_fields(const ArrayView<FieldDescriptor>& fields)
	{
		info.struct_descriptor.fields = fields;
	}

} // namespace robotick