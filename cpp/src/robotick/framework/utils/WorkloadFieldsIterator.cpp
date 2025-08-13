// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include "robotick/api.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"

namespace robotick
{

	void WorkloadFieldsIterator::for_each_workload(const Engine& engine, std::function<void(const WorkloadInstanceInfo&)> callback)
	{
		for (const WorkloadInstanceInfo& instance : engine.get_all_instance_info())
		{
			callback(instance);
		}
	}

	void WorkloadFieldsIterator::for_each_field_in_workload(const Engine& engine,
		const WorkloadInstanceInfo& instance,
		WorkloadsBuffer* workloads_override,
		std::function<void(const WorkloadFieldView&)> callback)
	{
		auto& workloads_buffer = workloads_override ? *workloads_override : engine.get_workloads_buffer();

		const TypeDescriptor* workload_type = instance.type;
		ROBOTICK_ASSERT(workload_type != nullptr);

		const WorkloadDescriptor* workload_desc = workload_type->get_workload_desc();
		ROBOTICK_ASSERT(workload_desc != nullptr);

		for_each_field_in_struct(instance, workload_desc->config_desc, workload_desc->config_offset, workloads_buffer, callback);
		for_each_field_in_struct(instance, workload_desc->inputs_desc, workload_desc->inputs_offset, workloads_buffer, callback);
		for_each_field_in_struct(instance, workload_desc->outputs_desc, workload_desc->outputs_offset, workloads_buffer, callback);
	}

	void WorkloadFieldsIterator::for_each_field_in_struct(const WorkloadInstanceInfo& instance,
		const TypeDescriptor* struct_type,
		const size_t struct_offset,
		WorkloadsBuffer& workloads_buffer,
		std::function<void(const WorkloadFieldView&)> callback)
	{
		if (!struct_type)
			return;

		const StructDescriptor* struct_desc = struct_type->get_struct_desc();
		ROBOTICK_ASSERT(struct_desc);

		for (const FieldDescriptor& field_desc : struct_desc->fields)
		{
			void* base_ptr = field_desc.get_data_ptr(workloads_buffer, instance, *struct_type, struct_offset);
			if (!base_ptr)
				continue;

			const TypeDescriptor* field_type_desc = field_desc.find_type_descriptor();
			if (!field_type_desc)
				continue;

			const StructDescriptor* sub_struct_desc = field_type_desc->get_struct_desc();
			if (!sub_struct_desc)
			{
				if (const DynamicStructDescriptor* dyn_desc = field_type_desc->get_dynamic_struct_desc())
				{
					sub_struct_desc = dyn_desc->get_struct_descriptor(base_ptr);
				}
			}

			if (sub_struct_desc)
			{
				for (const FieldDescriptor& sub_field : sub_struct_desc->fields)
				{
					void* sub_ptr = sub_field.get_data_ptr(base_ptr);
					WorkloadFieldView view{&instance, struct_type, &field_desc, &sub_field, sub_ptr};
					callback(view);
				}
			}
			else
			{
				WorkloadFieldView view{&instance, struct_type, &field_desc, nullptr, base_ptr};
				callback(view);
			}
		}
	}

} // namespace robotick
