// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/WorkloadFieldsIterator.h"

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

namespace robotick
{

	void WorkloadFieldsIterator::for_each_workload_field_impl(
		const Engine& engine, WorkloadsBuffer* workloads_override, std::function<void(const WorkloadFieldView&)> callback)
	{
		auto& workloads_buffer = workloads_override ? *workloads_override : engine.get_workloads_buffer();

		for (const WorkloadInstanceInfo& instance : engine.get_all_instance_info())
		{
			const WorkloadRegistryEntry* type = instance.type;

			auto walk_struct = [&](const StructRegistryEntry* struct_info)
			{
				if (!struct_info)
					return;

				for (const FieldInfo& field : struct_info->fields)
				{
					void* base_ptr = field.get_data_ptr(workloads_buffer, instance, *struct_info);

					if (base_ptr == nullptr)
					{
						continue; // nothing to report for an absent field
					}

					if (field.type == typeid(Blackboard))
					{
						Blackboard& blackboard = *static_cast<Blackboard*>(base_ptr);

						for (const BlackboardFieldInfo& blackboard_field : blackboard.get_schema())
						{
							WorkloadFieldView view;
							view.instance = &instance;
							view.struct_info = struct_info;
							view.field = &field;
							view.subfield = &blackboard_field;
							view.field_ptr = blackboard_field.get_data_ptr(blackboard);
							callback(view);
						}
					}
					else
					{
						WorkloadFieldView view;
						view.instance = &instance;
						view.struct_info = struct_info;
						view.field = &field;
						view.subfield = nullptr;
						view.field_ptr = base_ptr;
						callback(view);
					}
				}
			};

			walk_struct(type->config_struct);
			walk_struct(type->input_struct);
			walk_struct(type->output_struct);
		}
	}

} // namespace robotick
