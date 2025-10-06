// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeDescriptor.h"

#include "robotick/api_base.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeRegistry.h"

namespace robotick
{
	const TypeDescriptor s_type_desc_void{
		"void",
		GET_TYPE_ID(void),
		0,
		1,
		TypeCategory::Primitive,
		{},		 // .workload_desc etc. unused for primitives
		nullptr, // .to_string
		nullptr	 // .from_string
	};

	void* FieldDescriptor::get_data_ptr(void* container_ptr) const
	{
		ROBOTICK_ASSERT_MSG(this->offset_within_container != OFFSET_UNBOUND,
			"FieldDescriptor::get_data_ptr() - offset_within_container has not yet been bound (field '%s' with type '%s')",
			this->name.c_str(),
			this->type_id.get_debug_name());
		uint8_t* data_ptr = (uint8_t*)container_ptr + this->offset_within_container;
		return data_ptr;
	}

	void* FieldDescriptor::get_data_ptr(
		WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const TypeDescriptor&, const size_t struct_offset) const
	{
		ROBOTICK_ASSERT(
			instance.offset_in_workloads_buffer != OFFSET_UNBOUND && "Workload object instance offset should have been correctly set by now");
		ROBOTICK_ASSERT(struct_offset != OFFSET_UNBOUND && "struct offset should have been correctly set by now");
		ROBOTICK_ASSERT(this->offset_within_container != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

		uint8_t* base_ptr = workloads_buffer.raw_ptr();
		uint8_t* instance_ptr = base_ptr + instance.offset_in_workloads_buffer;
		uint8_t* struct_ptr = instance_ptr + struct_offset;
		return get_data_ptr(struct_ptr);
	}

	const TypeDescriptor* FieldDescriptor::find_type_descriptor() const
	{
		const TypeDescriptor* field_type = TypeRegistry::get().find_by_id(type_id);
		ROBOTICK_ASSERT_MSG(field_type != nullptr,
			"Unable to find TypeDescriptor '%s' for field '%s' - this shouldn't be possible - perhaps they are being pruned by the linker?",
			this->type_id.get_debug_name(),
			this->name.c_str());
		return field_type;
	}

	const FieldDescriptor* StructDescriptor::find_field(const char* field_name) const
	{
		for (const FieldDescriptor& field : fields)
		{
			if (field.name == field_name)
			{
				return &field;
			}
		}

		return nullptr;
	}

} // namespace robotick