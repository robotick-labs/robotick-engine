// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/utils/TypeId.h"

#include <atomic>
#include <memory>
#include <string>

namespace robotick
{
	class AtomicFlag;
	class Model;
	class WorkloadsBuffer;
	struct DataConnectionInfo;
	struct StructDescriptor;
	struct TickInfo;
	struct WorkloadInstanceInfo;

	class ROBOTICK_API Engine
	{
	  public: // main api accessors
		Engine();
		~Engine();

		void load(const Model& model);

		// The stop_flag must outlive this call. Do not pass temporaries.
		void run(const AtomicFlag& stop_after_next_tick_flag);

		void run(const AtomicFlag&&) = delete; // cause compile-error if a temporary is used

		bool is_running() const;

	  public: // internal public accessors
		const WorkloadInstanceInfo* get_root_instance_info() const;

		const WorkloadInstanceInfo* find_instance_info(const char* unique_name) const;
		void* find_instance(const char* unique_name) const;
		template <typename T> T* find_instance(const char* unique_name) const { return static_cast<T*>(this->find_instance(unique_name)); }
		template <typename T> T& find_instance_ref(const char* unique_name) const
		{
			T* found_instance_ptr = find_instance<T>(unique_name);
			ROBOTICK_ASSERT_MSG(found_instance_ptr != nullptr, "Named instance '%s' was not found", unique_name);
			return *found_instance_ptr;
		}

		const HeapVector<WorkloadInstanceInfo>& get_all_instance_info() const;
		const HeapVector<DataConnectionInfo>& get_all_data_connections() const;

		std::tuple<void*, size_t, TypeId> find_field_info(const char* path) const;

		WorkloadsBuffer& get_workloads_buffer() const;

	  private:
		void bind_blackboards_in_struct(WorkloadInstanceInfo& workload_instance_info,
			const TypeDescriptor& struct_type_desc,
			const size_t struct_offset,
			const size_t blackboard_storage_offset);
		void bind_blackboards_for_instances(HeapVector<WorkloadInstanceInfo>& instances, const size_t blackboards_data_start_offset);

		size_t compute_blackboard_memory_requirements(const HeapVector<WorkloadInstanceInfo>& instances);

		void setup_remote_engine_senders(const Model& model);
		void setup_remote_engines_receiver();

		void tick_remote_engine_connections(const TickInfo& tick_info);

	  private:
		struct State;
		State* state = nullptr;
	};
} // namespace robotick
