// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/ConsoleTelemetryTable.h"

#include <random>  // std::random_device, std::mt19937, std::uniform_real_distribution
#include <sstream> // std::ostringstream
#include <string>  // std::string, std::to_string
#include <vector>  // std::vector

namespace robotick
{
	struct ConsoleTelemetryConfig
	{
		bool pretty_print = true;
		bool enable_unicode = true;
	};

	ROBOTICK_BEGIN_FIELDS(ConsoleTelemetryConfig)
	ROBOTICK_FIELD(ConsoleTelemetryConfig, pretty_print)
	ROBOTICK_END_FIELDS()

	std::vector<ConsoleTelemetryRow> collect_console_telemetry_rows()
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

	struct ConsoleTelemetryWorkload
	{
		ConsoleTelemetryConfig config;

		void tick(double)
		{
			std::vector<ConsoleTelemetryRow> rows = collect_console_telemetry_rows();
			print_console_telemetry_table(rows, config.pretty_print, config.enable_unicode);
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(ConsoleTelemetryWorkload);

} // namespace robotick
