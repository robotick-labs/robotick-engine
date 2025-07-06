// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/WorkloadFieldsIterator.h"

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
		const WorkloadRegistryEntry* type = instance.type;

		auto walk_struct = [&](const StructRegistryEntry* struct_info)
		{
			if (!struct_info)
				return;

			for (const FieldInfo& field : struct_info->fields)
			{
				void* base_ptr = field.get_data_ptr(workloads_buffer, instance, *struct_info);
				if (!base_ptr)
					continue;

				if (field.type == GET_TYPE_ID(Blackboard))
				{
					Blackboard& blackboard = *static_cast<Blackboard*>(base_ptr);
					const StructDescriptor& blackboard_struct_desc = blackboard.get_struct_descriptor();
					for (const FieldDescriptor& blackboard_field : blackboard_struct_desc.fields)
					{
						WorkloadFieldView view{&instance, struct_info, &field, &blackboard_field, blackboard_field.get_data_ptr(blackboard)};
						callback(view);
					}
				}
				else
				{
					WorkloadFieldView view{&instance, struct_info, &field, nullptr, base_ptr};
					callback(view);
				}
			}
		};

		walk_struct(type->config_struct);
		walk_struct(type->input_struct);
		walk_struct(type->output_struct);
	}

} // namespace robotick
