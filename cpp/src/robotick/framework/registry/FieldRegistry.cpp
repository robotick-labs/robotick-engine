// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/data/WorkloadsBuffer.h"

namespace robotick
{
	FieldRegistry& FieldRegistry::get()
	{
		static FieldRegistry instance;
		return instance;
	}

	const StructRegistryEntry* FieldRegistry::register_struct(
		const std::string& name, size_t size, const std::type_index& type, size_t offset, std::vector<FieldInfo> fields)
	{
		std::lock_guard<std::mutex> lock(mutex);

		auto& entry = entries[name];

		entry.name = name;
		entry.size = size;

		if (entry.offset_within_workload == OFFSET_UNBOUND)
		{
			entry.offset_within_workload = offset;
		}
		// ^- one of the registrations doesn't know the offset - TODO - create a ticket to address the disparity - 2 sets of registrations is a sign
		// of a wrong pattern somewhere

		entry.type = type;

		// first valid registration populates fields:
		if (entry.fields.empty() && !fields.empty())
		{
			entry.fields = std::move(fields);

			// Populate field_from_name map
			for (auto& field : entry.fields)
			{
				entry.field_from_name.emplace(field.name, &field);
			}
		}
		// else: retain existing field definitions
		// (first registration call may actually come from Workload registration code, since static execution is
		// hard to predict.  So in that case we allow that to register an empty struct, and then populate the same
		// object with fields when "real" registration happens)

		return &entry;
	}

	const StructRegistryEntry* FieldRegistry::get_struct(const std::string& name) const
	{
		std::lock_guard<std::mutex> lock(mutex);

		auto it = entries.find(name);
		return it != entries.end() ? &it->second : nullptr;
	}

	uint8_t* FieldInfo::get_data_ptr(
		WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_info) const
	{
		ROBOTICK_ASSERT(
			instance.offset_in_workloads_buffer != OFFSET_UNBOUND && "Workload object instance offset should have been correctly set by now");
		ROBOTICK_ASSERT(struct_info.offset_within_workload != OFFSET_UNBOUND && "struct offset should have been correctly set by now");
		ROBOTICK_ASSERT(this->offset_within_struct != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

		uint8_t* base_ptr = workloads_buffer.raw_ptr();
		uint8_t* instance_ptr = base_ptr + instance.offset_in_workloads_buffer;
		uint8_t* struct_ptr = instance_ptr + struct_info.offset_within_workload;
		return struct_ptr + this->offset_within_struct;
	}

} // namespace robotick
