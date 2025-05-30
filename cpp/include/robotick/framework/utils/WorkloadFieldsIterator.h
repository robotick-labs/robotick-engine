// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <functional>
#include <typeindex>

namespace robotick
{
	// Forward declarations
	struct WorkloadInstanceInfo;
	struct StructRegistryEntry;
	struct FieldInfo;
	struct BlackboardFieldInfo;
	class Engine;
	class WorkloadsBuffer;

	// View describing a single field in a workload, for use in iterator results (not to be confused with FieldInfo)
	struct WorkloadFieldView
	{
		const WorkloadInstanceInfo* instance = nullptr;
		const StructRegistryEntry* struct_info = nullptr;
		const FieldInfo* field = nullptr;
		const BlackboardFieldInfo* subfield = nullptr;
		void* field_ptr = nullptr;
	};

	struct WorkloadFieldsIterator
	{
		static inline void for_each_workload_field(
			const Engine& engine, const WorkloadsBuffer* workloads_override, std::function<void(const WorkloadFieldView&)> callback)
		{
			for_each_workload_field_impl(engine, workloads_override, std::move(callback));
		}

		static inline void for_each_workload_field(const Engine& engine, std::function<void(const WorkloadFieldView&)> callback)
		{
			for_each_workload_field_impl(engine, nullptr, std::move(callback));
		}

	  private:
		static void for_each_workload_field_impl(
			const Engine& engine, const WorkloadsBuffer* workloads_override, std::function<void(const WorkloadFieldView&)> callback);
	};

} // namespace robotick
