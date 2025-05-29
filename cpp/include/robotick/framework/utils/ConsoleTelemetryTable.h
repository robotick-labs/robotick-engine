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
		double tick_ms = 0.0;
		double goal_ms = 0.0;
		double percent = 0.0;
	};

	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print);
} // namespace robotick
