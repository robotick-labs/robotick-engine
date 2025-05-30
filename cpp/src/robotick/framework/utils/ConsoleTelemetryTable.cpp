// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/ConsoleTelemetryTable.h"
#include "robotick/framework/utils/ConsoleTable.h"

#include <iomanip>
#include <sstream>

namespace robotick
{
	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print, bool enable_unicode)
	{
		const std::vector<std::string> headers = {"Type", "Name", "Inputs", "Outputs", "dT (ms)", "Goal (ms)", "dT %", "Time (ms)", "Time %"};

		const std::vector<size_t> widths = {32, 24, 32, 32, 10, 10, 8, 10, 8};

		std::vector<ConsoleTableRow> table_rows;
		table_rows.reserve(rows.size());

		for (auto& r : rows)
		{
			std::ostringstream dt, goal, dpct, td, tpct;
			dt << std::fixed << std::setprecision(2) << r.tick_delta_ms;
			goal << std::fixed << std::setprecision(2) << r.goal_interval_ms;
			dpct << std::fixed << std::setprecision(1) << ((r.goal_interval_ms > 0.0) ? 100.0 * r.tick_delta_ms / r.goal_interval_ms : 0.0) << "%";
			td << std::fixed << std::setprecision(2) << r.tick_duration_ms;
			tpct << std::fixed << std::setprecision(1) << ((r.goal_interval_ms > 0.0) ? 100.0 * r.tick_duration_ms / r.goal_interval_ms : 0.0) << "%";

			table_rows.push_back({{r.type, r.name, r.inputs, r.outputs, dt.str(), goal.str(), dpct.str(), td.str(), tpct.str()}});
		}

		print_console_table("Robotick Console Telemetry", headers, widths, table_rows, pretty_print, enable_unicode);
	}
} // namespace robotick
