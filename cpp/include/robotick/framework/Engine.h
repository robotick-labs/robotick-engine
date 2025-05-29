// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadInstanceInfo.h"
#include "robotick/framework/api.h"

#include <atomic>
#include <memory>
#include <string>

namespace robotick
{
	class WorkloadsBuffer;
	class BlackboardsBuffer;
	struct DataConnectionInfo;
	struct WorkloadRegistryEntry;
	struct TelemetryCollector;
	struct WorkloadFieldsIterator;

	namespace test
	{
		struct EngineInspector;
	}

	class ROBOTICK_API Engine
	{
		friend struct robotick::test::EngineInspector;
		friend struct robotick::TelemetryCollector;
		friend struct robotick::WorkloadFieldsIterator;

	  public:
		Engine();
		~Engine();

		void load(const Model& model);

		// The stop_flag must outlive this call. Do not pass temporaries.
		void run(const std::atomic<bool>& stop_after_next_tick_flag);

		void run(const std::atomic<bool>&&) = delete; // cause compile-error if a temporary is used

	  protected:
		const WorkloadInstanceInfo* get_root_instance_info() const;
		const WorkloadInstanceInfo& get_instance_info(size_t index) const;
		const std::vector<WorkloadInstanceInfo>& get_all_instance_info() const;
		const std::vector<DataConnectionInfo>& get_all_data_connections() const;

		const WorkloadsBuffer& get_workloads_buffer_readonly() const;
		const BlackboardsBuffer& get_blackboards_buffer_readonly() const;

	  private:
		struct State;
		std::unique_ptr<State> state;
	};
} // namespace robotick
