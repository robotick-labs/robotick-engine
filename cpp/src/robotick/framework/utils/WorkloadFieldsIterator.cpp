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

		auto walk_struct = [&](const TypeDescriptor* struct_type, const size_t struct_offset)
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

				if (field_desc.type_id == GET_TYPE_ID(Blackboard))
				{
					Blackboard& blackboard = *static_cast<Blackboard*>(base_ptr);
					const StructDescriptor& blackboard_struct_desc = blackboard.get_struct_descriptor();
					for (const FieldDescriptor& blackboard_field : blackboard_struct_desc.fields)
					{
						void* blackboard_field_data_ptr = blackboard.get_field_data(blackboard_field);
						WorkloadFieldView view{&instance, struct_type, &field_desc, &blackboard_field, blackboard_field_data_ptr};
						callback(view);
					}
				}
				else
				{
					WorkloadFieldView view{&instance, struct_type, &field_desc, nullptr, base_ptr};
					callback(view);
				}
			}
		};

		walk_struct(workload_desc->config_desc, workload_desc->config_offset);
		walk_struct(workload_desc->inputs_desc, workload_desc->inputs_offset);
		walk_struct(workload_desc->outputs_desc, workload_desc->outputs_offset);
	}

} // namespace robotick
