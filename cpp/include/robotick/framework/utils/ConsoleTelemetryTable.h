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
		ConsoleTelemetryRow() = default;
		ConsoleTelemetryRow(std::string type, std::string name, std::string inputs, std::string outputs, double tick_ms = 0.0, double goal_ms = 0.0,
			double percent = 0.0)
			: type(std::move(type)), name(std::move(name)), inputs(std::move(inputs)), outputs(std::move(outputs)), tick_ms(tick_ms),
			  goal_ms(goal_ms), percent(percent)
		{
		}

		std::string type;
		std::string name;
		std::string inputs;
		std::string outputs;
		double tick_ms = 0.0;
		double goal_ms = 0.0;
		double percent = 0.0;
	};

	/// Prints telemetry data in a console table format.
	/// @param rows Vector of telemetry data rows to display
	/// @param pretty_print If true, renders a formatted table with borders and colors;
	///                     if false, outputs simple tab-separated format
	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print, bool enable_unicode);
} // namespace robotick
