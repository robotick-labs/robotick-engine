// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/Buffer.h"
#include "robotick/framework/registry/FieldRegistry.h"

#include <functional>
#include <typeindex>

namespace robotick
{
	struct WorkloadFieldView
	{
		const WorkloadInstanceInfo* instance = nullptr;
		const StructRegistryEntry* struct_info = nullptr;
		const FieldInfo* field = nullptr;
		const BlackboardField* subfield = nullptr;
		void* field_ptr = nullptr;
	};

	namespace detail
	{
		inline void* resolve_field_ptr(
			const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_entry, const FieldInfo& field, const WorkloadsBuffer& buffer)
		{
			uint8_t* struct_base = instance.ptr + (struct_entry.offset_from(instance));
			return static_cast<void*>(struct_base + field.offset);
		}

		inline void* resolve_blackboard_subfield_ptr(const Blackboard& blackboard, const BlackboardField& subfield, const BlackboardsBuffer& buffer)
		{
			return buffer.raw_ptr() + blackboard.get_offset() + subfield.offset;
		}
	} // namespace detail

	inline void for_each_workload_field(const Engine& engine, const WorkloadsBuffer* workloads_override,
		const BlackboardsBuffer* blackboards_override, std::function<void(const WorkloadFieldView&)> callback)
	{
		const auto& workloads_buffer = workloads_override ? *workloads_override : engine.get_workloads_buffer_readonly();
		const auto& blackboards_buffer = blackboards_override ? *blackboards_override : engine.get_blackboards_buffer_readonly();

		for (const WorkloadInstanceInfo& instance : engine.get_all_instance_info())
		{
			const WorkloadRegistryEntry* type = instance.type;

			auto walk_struct = [&](const StructRegistryEntry* struct_info)
			{
				if (!struct_info)
					return;

				for (const FieldInfo& field : struct_info->fields)
				{
					void* base_ptr = detail::resolve_field_ptr(instance, *struct_info, field, workloads_buffer);

					if (field.type == typeid(Blackboard))
					{
						auto* blackboard = reinterpret_cast<const Blackboard*>(base_ptr);
						if (!blackboard || blackboard->get_schema().empty())
							continue;

						for (const BlackboardField& subfield : blackboard->get_schema())
						{
							WorkloadFieldView view;
							view.instance = &instance;
							view.struct_info = struct_info;
							view.field = &field;
							view.subfield = &subfield;
							view.field_ptr = detail::resolve_blackboard_subfield_ptr(*blackboard, subfield, blackboards_buffer);
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
