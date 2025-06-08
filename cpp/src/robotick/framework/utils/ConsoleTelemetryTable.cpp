// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/ConsoleTelemetryTable.h"
#include "robotick/framework/utils/ConsoleTable.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace robotick
{
	// Helper: Wrap a string, adding ellipses only on continuation lines
	std::vector<std::string> wrap_by_space_with_ellipses(const std::string& input, size_t max_width)
	{
		std::vector<std::string> lines;
		std::istringstream iss(input);
		std::string word;
		std::string current_line;
		bool first_line = true;

		while (iss >> word)
		{
			// Handle long words with ellipsis wrapping
			if (word.size() > max_width)
			{
				if (!current_line.empty())
				{
					lines.push_back(current_line);
					current_line.clear();
				}

				size_t start = 0;
				while (start < word.size())
				{
					size_t take = first_line ? max_width : max_width - 3;
					std::string part = word.substr(start, take);
					if (!first_line)
						part = "..." + part;
					lines.push_back(part);
					start += take;
					first_line = false;
				}
				first_line = true;
				continue;
			}

			// Normal wrapping
			if (current_line.empty())
			{
				current_line = word;
			}
			else if (current_line.size() + 1 + word.size() <= max_width)
			{
				current_line += " " + word;
			}
			else
			{
				lines.push_back(current_line);
				current_line = word;
			}
		}
		if (!current_line.empty())
			lines.push_back(current_line);

		return lines;
	}

	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print, bool enable_unicode)
	{
		const std::vector<std::string> headers = {
			"Type", "Name", "Config", "Inputs", "Outputs", "dT (ms)", "Goal (ms)", "dT %", "Time (ms)", "Time %"};

		const std::vector<size_t> widths = {32, 16, 32, 32, 32, 10, 10, 8, 10, 8};

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

			std::vector<std::vector<std::string>> wrapped_columns = {
				wrap_by_space_with_ellipses(r.type, widths[0]),
				wrap_by_space_with_ellipses(r.name, widths[1]),
				wrap_by_space_with_ellipses(r.config, widths[2]),
				wrap_by_space_with_ellipses(r.inputs, widths[3]),
				wrap_by_space_with_ellipses(r.outputs, widths[4]),
				wrap_by_space_with_ellipses(dt.str(), widths[5]),
				wrap_by_space_with_ellipses(goal.str(), widths[6]),
				wrap_by_space_with_ellipses(dpct.str(), widths[7]),
				wrap_by_space_with_ellipses(td.str(), widths[8]),
				wrap_by_space_with_ellipses(tpct.str(), widths[9]),
			};

			size_t max_lines = 0;
			for (const auto& col : wrapped_columns)
				max_lines = std::max(max_lines, col.size());

			for (size_t i = 0; i < max_lines; ++i)
			{
				std::vector<std::string> line;
				for (size_t col = 0; col < wrapped_columns.size(); ++col)
				{
					line.push_back(i < wrapped_columns[col].size() ? wrapped_columns[col][i] : "");
				}
				table_rows.push_back({line});
			}
		}

		print_console_table("Robotick Console Telemetry", headers, widths, table_rows, pretty_print, enable_unicode);
	}
} // namespace robotick
