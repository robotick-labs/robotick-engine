// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/WorkloadRegistry.h"

#include <algorithm> // std::max
#include <iomanip>	 // std::setw, std::left, std::setprecision, std::fixed
#include <iostream>	 // std::cout, std::endl
#include <random>	 // std::random_device, std::mt19937, std::uniform_real_distribution
#include <sstream>	 // std::ostringstream
#include <string>	 // std::string, std::to_string
#include <vector>	 // std::vector

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

	std::vector<ConsoleTelemetryRow> collect_console_telemetry_rows()
	{
		std::vector<ConsoleTelemetryRow> rows;

		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_real_distribution<> tick_dist(0.1, 5.0);  // Simulated tick time
		static std::uniform_real_distribution<> goal_dist(1.0, 5.0);  // Goal times
		static std::uniform_real_distribution<> val_dist(0.0, 100.0); // For input/output values

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
		std::vector<std::string> wrap_string(const std::string& str, size_t width)
		{
			std::vector<std::string> lines;
			for (size_t i = 0; i < str.length(); i += width)
				lines.push_back(str.substr(i, width));
			return lines;
		}

		void tick(double)
		{
			// === Column widths ===
			const size_t width_type = 16;
			const size_t width_name = 16;
			const size_t width_inputs = 30;
			const size_t width_outputs = 30;
			const size_t width_tick = 12;
			const size_t width_goal = 12;
			const size_t width_percent = 5;

			std::vector<ConsoleTelemetryRow> telemetry_rows = collect_console_telemetry_rows();

			std::ostringstream oss;

			// Clear screen and move cursor to top-left
			oss << "\033[2J\033[H";

			// Header
			oss << "=== ConsoleTelemetry ===\n";
			oss << std::left << std::setw(width_type) << "Type" << std::setw(width_name) << "Name" << std::setw(width_inputs) << "Inputs"
				<< std::setw(width_outputs) << "Outputs" << std::setw(width_tick) << "Tick/ms" << std::setw(width_goal) << "Goal/ms"
				<< "%\n";

			// Rows
			for (const auto& row : telemetry_rows)
			{
				auto type_lines = wrap_string(row.type, width_type);
				auto name_lines = wrap_string(row.name, width_name);
				auto input_lines = wrap_string(row.inputs, width_inputs);
				auto output_lines = wrap_string(row.outputs, width_outputs);

				std::ostringstream tick_stream, goal_stream, percent_stream;
				tick_stream << std::fixed << std::setprecision(2) << row.tick_ms;
				goal_stream << std::fixed << std::setprecision(2) << row.goal_ms;
				percent_stream << std::fixed << std::setprecision(1) << row.percent << "%";

				auto tick_lines = wrap_string(tick_stream.str(), width_tick);
				auto goal_lines = wrap_string(goal_stream.str(), width_goal);
				auto percent_lines = wrap_string(percent_stream.str(), width_percent);

				size_t max_lines = std::max({type_lines.size(), name_lines.size(), input_lines.size(), output_lines.size(), tick_lines.size(),
					goal_lines.size(), percent_lines.size()});

				for (size_t i = 0; i < max_lines; ++i)
				{
					auto get_line = [](const std::vector<std::string>& v, size_t i) -> std::string
					{
						return i < v.size() ? v[i] : "";
					};

					oss << std::left << std::setw(width_type) << get_line(type_lines, i) << std::setw(width_name) << get_line(name_lines, i)
						<< std::setw(width_inputs) << get_line(input_lines, i) << std::setw(width_outputs) << get_line(output_lines, i)
						<< std::setw(width_tick) << get_line(tick_lines, i) << std::setw(width_goal) << get_line(goal_lines, i)
						<< get_line(percent_lines, i) << "\n";
				}
			}

			std::cout << oss.str() << std::flush;
		}
	};

	ROBOTICK_DEFINE_WORKLOAD(ConsoleTelemetryWorkload);

} // namespace robotick
