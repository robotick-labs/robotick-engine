// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

namespace robotick
{
	struct ConsoleTelemetryRow
	{
		std::string type;
		std::string name;
		std::string inputs;
		std::string outputs;
		double tick_duration_ms = 0.0; // actual work time
		double tick_delta_ms = 0.0;	   // time since last tick call
		double goal_interval_ms = 0.0; // expected interval

		ConsoleTelemetryRow() = default;
		ConsoleTelemetryRow(std::string type, std::string name, std::string inputs, std::string outputs, double tick_duration_ms,
			double tick_delta_ms, double goal_interval_ms)
			: type(std::move(type)), name(std::move(name)), inputs(std::move(inputs)), outputs(std::move(outputs)),
			  tick_duration_ms(tick_duration_ms), tick_delta_ms(tick_delta_ms), goal_interval_ms(goal_interval_ms)
		{
		}
	};

	/// Prints telemetry info using the generic table infrastructure
	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print, bool enable_unicode);
} // namespace robotick
