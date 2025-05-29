// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Buffer.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/ConsoleTelemetryTable.h"

#include <cassert> // assert
#include <random>  // std::random_device, std::mt19937, std::uniform_real_distribution
#include <sstream> // std::ostringstream
#include <string>  // std::string, std::to_string
#include <vector>  // std::vector

namespace robotick
{
	struct ConsoleTelemetryConfig
	{
		bool enable_pretty_print = true;
		bool enable_unicode = true;
		bool enable_demo = false;
	};

	ROBOTICK_BEGIN_FIELDS(ConsoleTelemetryConfig)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, enable_pretty_print)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, enable_unicode)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, enable_demo)
	ROBOTICK_END_FIELDS()

	struct ConsoleTelemetryWorkload
	{
		ConsoleTelemetryConfig config;

		const Engine* engine = nullptr;

		static std::string depth_prefix(size_t depth, const std::string& name)
		{
			if (depth == 0)
				return name;

			std::ostringstream oss;
			oss << "|";
			for (size_t i = 1; i < depth; ++i)
				oss << "  ";
			oss << "--" << name;
			return oss.str();
		}

		static void collect_console_telemetry_row(
			ConsoleTelemetryRow& instance_row, size_t current_depth, const WorkloadInstanceInfo& instance_info, const Engine& engine)
		{
			(void)engine;

			instance_row.type = depth_prefix(current_depth, instance_info.type->name);
			instance_row.name = instance_info.unique_name;
			instance_row.inputs = "?";
			instance_row.outputs = "?";

			// actual work time
			instance_row.tick_duration_ms = instance_info.mutable_stats.last_tick_duration * 1000.0;

			// time since last tick call
			instance_row.tick_delta_ms = instance_info.mutable_stats.last_time_delta * 1000.0;

			// expected interval (as specified in model seed data)
			instance_row.goal_interval_ms = instance_info.tick_rate_hz > 0.0 ? 1000.0 / instance_info.tick_rate_hz : -1.0;
		}

		static std::vector<ConsoleTelemetryRow> collect_console_telemetry_rows(const Engine& engine)
		{
			// obtain clone of workloads and bb's buffers for non-aliased use below...
			const WorkloadsBuffer& workloads_buffer = engine.get_workloads_buffer_readonly();
			(void)workloads_buffer;
			const BlackboardsBuffer& blackboards_buffer = engine.get_blackboards_buffer_readonly();
			(void)blackboards_buffer;
			// TODO - remove "source" concept from these and others - we need to eliminate global-singletons

			std::vector<ConsoleTelemetryRow> console_rows;
			console_rows.reserve(engine.get_all_instance_info().size());
			// ^- we should expect to visit all instances in the below code so size accordingly

			const WorkloadInstanceInfo* root_instance_info = engine.get_root_instance_info();
			assert(root_instance_info != nullptr && "Engine must have a root instance before ticking workloads");

			auto visit_all = [&](const WorkloadInstanceInfo* instance_info, size_t current_depth, const auto& self) -> void
			{
				assert(instance_info != nullptr && "Engine should have no null instance-info's");

				// add console-row for this instance:
				ConsoleTelemetryRow& instance_row = console_rows.emplace_back();
				collect_console_telemetry_row(instance_row, current_depth, *instance_info, engine);

				for (const WorkloadInstanceInfo* child_instance_info : instance_info->children)
				{
					self(child_instance_info, current_depth + 1, self);
				}
			};

			visit_all(root_instance_info, 0, visit_all);

			return console_rows;
		}

		static std::vector<ConsoleTelemetryRow> collect_console_telemetry_rows_demo(const Engine&)
		{
			std::vector<ConsoleTelemetryRow> rows;

			// Random distributions for generating demo telemetry data:

			static std::random_device rd;
			static std::mt19937 gen(rd());
			static std::uniform_real_distribution<> tick_dist(0.1, 5.0);
			static std::uniform_real_distribution<> goal_dist(1.0, 5.0);
			static std::uniform_real_distribution<> val_dist(0.0, 100.0);

			for (int i = 0; i < 3; ++i)
			{
				double tick_ms = tick_dist(gen);
				double goal_ms = goal_dist(gen);
				double percent = (tick_ms / goal_ms) * 100.0;

				std::ostringstream input_oss;
				std::ostringstream output_oss;
				input_oss << "input_" << i << "=" << val_dist(gen);
				output_oss << "output_" << i << "=" << val_dist(gen);

				rows.push_back(ConsoleTelemetryRow{
					"DummyType" + std::to_string(i), "Workload" + std::to_string(i), input_oss.str(), output_oss.str(), tick_ms, goal_ms, percent});
			}

			return rows;
		}

		void set_engine(const Engine& engine_in) { engine = &engine_in; }

		void tick(double)
		{
			// Note - when using ConsoleTelemetryWorkload it is strongly recommended to run it at 3-5Hz max.
			// This avoids overwhelming stdout and dominating frame time, even without pretty printing.
			// To help mitigate this, printing is built as a single string and flushed once per tick.

			assert(engine != nullptr && "ConsoleTelemetryWorkload - engine should never be null during tick");
			// ^- we should never reach the point of ticking without set_engine having been called.
			//  - we also assume that any given workload-instance can only ever be part of a single
			//    engine, and that the lifespan of the engine is longer than the workloads that are
			//    owned and created/destroyed by the engine.

			std::vector<ConsoleTelemetryRow> rows =
				config.enable_demo ? collect_console_telemetry_rows_demo(*engine) : collect_console_telemetry_rows(*engine);

			print_console_telemetry_table(rows, config.enable_pretty_print, config.enable_unicode);
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(ConsoleTelemetryWorkload);

} // namespace robotick
