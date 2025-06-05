#pragma once

#include <functional>
#include <typeindex>

namespace robotick
{
	struct WorkloadInstanceInfo;
	struct StructRegistryEntry;
	struct FieldInfo;
	struct BlackboardFieldInfo;
	class Engine;
	class WorkloadsBuffer;

	struct WorkloadFieldView
	{
		const WorkloadInstanceInfo* workload_info = nullptr;
		const StructRegistryEntry* struct_info = nullptr;
		const FieldInfo* field_info = nullptr;
		const BlackboardFieldInfo* subfield_info = nullptr;
		void* field_ptr = nullptr;
	};

	struct WorkloadFieldsIterator
	{
		static void for_each_workload(const Engine& engine, std::function<void(const WorkloadInstanceInfo&)> callback);

		static void for_each_field_in_workload(const Engine& engine, const WorkloadInstanceInfo& instance, WorkloadsBuffer* workloads_override,
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
