// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/ConsoleTelemetryTable.h"
#include "robotick/framework/utils/TelemetryCollector.h"

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
			// Note - when using ConsoleTelemetryWorkload it is strongly recommended to run it at 5-10Hz max.
			// This avoids overwhelming stdout and dominating frame time, even without pretty printing.
			// To help mitigate this, printing is built as a single string and flushed once per tick.

			assert(engine != nullptr && "ConsoleTelemetryWorkload - engine should never be null during tick");
			// ^- we should never reach the point of ticking without set_engine having been called.
			//  - we also assume that any given workload-instance can only ever be part of a single
			//    engine, and that the lifespan of the engine is longer than the workloads that are
			//    owned and created/destroyed by the engine.

			auto rows = config.enable_demo ? collect_console_telemetry_rows_demo(*engine) : TelemetryCollector{*engine}.collect_rows();

			print_console_telemetry_table(rows, config.enable_pretty_print, config.enable_unicode);
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(ConsoleTelemetryWorkload);

} // namespace robotick
