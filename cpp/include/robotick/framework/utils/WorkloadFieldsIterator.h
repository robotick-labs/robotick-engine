#pragma once

#include <functional>
#include <typeindex>

namespace robotick
{
	struct FieldDescriptor;
	struct StructDescriptor;
	struct TypeDescriptor;
	struct WorkloadInstanceInfo;
	class Engine;
	class WorkloadsBuffer;

	struct WorkloadFieldView
	{
		const WorkloadInstanceInfo* workload_info = nullptr;
		const TypeDescriptor* struct_info = nullptr;
		const FieldDescriptor* field_info = nullptr;
		const FieldDescriptor* subfield_info = nullptr; // DEPRECATED - remove soon - use "for_each_field_in_struct_field()" instead
		void* field_ptr = nullptr;

		bool is_struct_field() const;
		const StructDescriptor* get_field_struct_desc() const;
	};

	struct WorkloadFieldsIterator
	{
		static void for_each_workload(const Engine& engine, std::function<void(const WorkloadInstanceInfo&)> callback);

		static void for_each_field_in_struct(const WorkloadInstanceInfo& instance,
			const TypeDescriptor* struct_type,
			const size_t struct_offset,
			WorkloadsBuffer& workloads_buffer,
			std::function<void(const WorkloadFieldView&)> callback);

		static void for_each_field_in_struct_field(const WorkloadFieldView& parent_field, std::function<void(const WorkloadFieldView&)> callback);

		static void for_each_field_in_workload(const Engine& engine,
			const WorkloadInstanceInfo& instance,
			WorkloadsBuffer* workloads_override,
			std::function<void(const WorkloadFieldView&)> callback);

		static inline void for_each_workload_field(
			const Engine& engine, WorkloadsBuffer* workloads_override, std::function<void(const WorkloadFieldView&)> callback)
		{
			for_each_workload(engine,
				[&](const WorkloadInstanceInfo& instance)
				{
					for_each_field_in_workload(engine, instance, workloads_override, callback);
				});
		}

		static inline void for_each_workload_field(const Engine& engine, std::function<void(const WorkloadFieldView&)> callback)
		{
			for_each_workload_field(engine, nullptr, std::move(callback));
		}
	};
} // namespace robotick
