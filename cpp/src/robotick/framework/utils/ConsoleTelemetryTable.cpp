// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/ConsoleTelemetryTable.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace robotick
{
	namespace
	{
		const size_t width_type = 24;
		const size_t width_name = 24;
		const size_t width_inputs = 40;
		const size_t width_outputs = 40;
		const size_t width_tick = 12;
		const size_t width_goal = 12;
		const size_t width_percent = 8;

		std::vector<std::string> wrap(const std::string& s, size_t width)
		{
			std::vector<std::string> out;
			for (size_t i = 0; i < s.size(); i += width)
				out.push_back(s.substr(i, width));
			return out;
		}

		std::string colored_percent(const std::string& val, double percent, bool first, bool pretty)
		{
			if (!first || !pretty)
				return val;

			const char* color = (percent < 80.0) ? "\033[32m" : (percent <= 100.0 ? "\033[33m" : "\033[31m");
			std::ostringstream oss;
			oss << color << std::setw(width_percent) << val << "\033[0m";
			return oss.str();
		}
	} // namespace

	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print)
	{
		const std::vector<size_t> widths = {width_type, width_name, width_inputs, width_outputs, width_tick, width_goal, width_percent};

		const std::vector<std::string> headers = {"Type", "Name", "Inputs", "Outputs", "Tick/ms", "Goal/ms", "%"};

		std::ostringstream oss;
		oss << "\033[2J\033[H"; // clear + home

		if (!pretty_print)
		{
			oss << "=== Robotick Console Telemetry ===\n\n";
			for (const auto& h : headers)
				oss << h << "\t";
			oss << "\n";
			for (const auto& row : rows)
			{
				oss << row.type << "\t" << row.name << "\t" << row.inputs << "\t" << row.outputs << "\t" << std::fixed << std::setprecision(2)
					<< row.tick_ms << "\t" << std::fixed << std::setprecision(2) << row.goal_ms << "\t" << std::fixed << std::setprecision(1)
					<< row.percent << "%\n";
			}
			std::cout << oss.str() << std::flush;
			return;
		}

		oss << "\n=== Robotick Console Telemetry ===\n\n";

		// Top border
		oss << "┌";
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << "─";
			oss << (i == widths.size() - 1 ? "┐\n" : "┬");
		}

		// Header
		oss << "│\033[1m";
		for (size_t i = 0; i < headers.size(); ++i)
		{
			oss << std::setw(widths[i]) << std::left << headers[i];
			oss << (i == headers.size() - 1 ? "\033[0m│\n" : "\033[0m│\033[1m");
		}

		// Separator
		oss << "├";
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << "─";
			oss << (i == widths.size() - 1 ? "┤\n" : "┼");
		}

		// Rows
		for (const auto& row : rows)
		{
			auto type_lines = wrap(row.type, width_type);
			auto name_lines = wrap(row.name, width_name);
			auto input_lines = wrap(row.inputs, width_inputs);
			auto output_lines = wrap(row.outputs, width_outputs);

			std::ostringstream tick, goal, percent;
			tick << std::fixed << std::setprecision(2) << row.tick_ms;
			goal << std::fixed << std::setprecision(2) << row.goal_ms;
			percent << std::fixed << std::setprecision(1) << row.percent << "%";

			auto tick_lines = wrap(tick.str(), width_tick);
			auto goal_lines = wrap(goal.str(), width_goal);
			auto percent_lines = wrap(percent.str(), width_percent);

			size_t max_lines = std::max({type_lines.size(), name_lines.size(), input_lines.size(), output_lines.size(), tick_lines.size(),
				goal_lines.size(), percent_lines.size()});

			for (size_t i = 0; i < max_lines; ++i)
			{
				auto get = [i](const std::vector<std::string>& v)
				{
					return i < v.size() ? v[i] : "";
				};

				oss << "│" << std::setw(width_type) << std::left << get(type_lines) << "│" << std::setw(width_name) << get(name_lines) << "│"
					<< std::setw(width_inputs) << get(input_lines) << "│" << std::setw(width_outputs) << get(output_lines) << "│"
					<< std::setw(width_tick) << get(tick_lines) << "│" << std::setw(width_goal) << get(goal_lines) << "│"
					<< colored_percent(get(percent_lines), row.percent, i == 0, pretty_print) << "│\n";
			}
		}

		// Bottom border
		oss << "└";
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << "─";
			oss << (i == widths.size() - 1 ? "┘\n" : "┴");
		}

		std::cout << oss.str() << std::flush;
	}
} // namespace robotick
