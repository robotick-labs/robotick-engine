// Copyright Robotick Labs
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
		std::string config;
		std::string inputs;
		std::string outputs;
		float tick_duration_ms = 0.0; // actual work time
		float tick_delta_ms = 0.0;	  // time since last tick call
		float goal_interval_ms = 0.0; // expected interval

		ConsoleTelemetryRow() = default;
		ConsoleTelemetryRow(std::string type,
			std::string name,
			std::string config,
			std::string inputs,
			std::string outputs,
			float tick_duration_ms,
			float tick_delta_ms,
			float goal_interval_ms)
			: type(std::move(type))
			, name(std::move(name))
			, config(std::move(config))
			, inputs(std::move(inputs))
			, outputs(std::move(outputs))
			, tick_duration_ms(tick_duration_ms)
			, tick_delta_ms(tick_delta_ms)
			, goal_interval_ms(goal_interval_ms)
		{
		}
	};

	/// Prints telemetry info using the generic table infrastructure
	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print, bool enable_unicode);

} // namespace robotick
