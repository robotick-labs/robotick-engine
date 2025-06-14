// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadInstanceInfo.h"

#include <atomic>
#include <memory>
#include <string>

namespace robotick
{
	class AtomicFlag;
	class ConsoleTelemetryCollector;
	class WorkloadsBuffer;
	struct DataConnectionInfo;
	struct MqttFieldSync;
	struct StructRegistryEntry;
	struct WorkloadFieldsIterator;
	struct WorkloadInstanceInfo;
	struct WorkloadRegistryEntry;

	namespace test
	{
		struct EngineInspector;
	}

	class ROBOTICK_API Engine
	{
		friend struct robotick::test::EngineInspector;
		friend struct robotick::ConsoleTelemetryCollector;
		friend struct robotick::MqttFieldSync;
		friend struct robotick::WorkloadFieldsIterator;
		friend struct robotick::WorkloadInstanceInfo;

	  public:
		Engine();
		~Engine();

		void load(const Model& model);

		// The stop_flag must outlive this call. Do not pass temporaries.
		void run(const AtomicFlag& stop_after_next_tick_flag);

		void run(const AtomicFlag&&) = delete; // cause compile-error if a temporary is used

		bool is_running() const;

	  protected:
		const WorkloadInstanceInfo* get_root_instance_info() const;
		const WorkloadInstanceInfo& get_instance_info(size_t index) const;
		const WorkloadInstanceInfo* find_instance_info(const char* unique_name) const;
		const std::vector<WorkloadInstanceInfo>& get_all_instance_info() const;
		const std::vector<DataConnectionInfo>& get_all_data_connections() const;

		WorkloadsBuffer& get_workloads_buffer() const;

	  private:
		void bind_blackboards_in_struct(
			WorkloadInstanceInfo& workload_instance_info, const StructRegistryEntry& struct_entry, size_t& blackboard_storage_offset);
		void bind_blackboards_for_instances(std::vector<WorkloadInstanceInfo>& instances, const size_t blackboards_data_start_offset);

		size_t compute_blackboard_memory_requirements(const std::vector<WorkloadInstanceInfo>& instances);

		void setup_remote_engine_senders(const Model& model);
		void setup_remote_engines_listener();

		void tick_remote_engine_connections();

	  private:
		struct State;
		std::unique_ptr<State> state;
	};
} // namespace robotick
