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
			if (s.empty())
				return out;

			std::istringstream iss(s);
			std::string word, line;

			while (iss >> word)
			{
				if (line.empty())
					line = word;
				else if (line.length() + 1 + word.length() <= width)
					line += " " + word;
				else
				{
					out.push_back(line);
					line = word;
				}
			}
			if (!line.empty())
				out.push_back(line);
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

		struct BoxChars
		{
			const char* TL;
			const char* TR;
			const char* TMID;
			const char* LMID;
			const char* RMID;
			const char* CENTER;
			const char* BL;
			const char* BR;
			const char* BMID;
			const char* H;
			const char* V;
		};

		static constexpr BoxChars unicode_chars{"┌", "┐", "┬", "├", "┤", "┼", "└", "┘", "┴", "─", "│"};
		static constexpr BoxChars ascii_chars{"+", "+", "+", "+", "+", "+", "+", "+", "+", "-", "|"};

		const BoxChars& get_box_chars(bool unicode)
		{
			return unicode ? unicode_chars : ascii_chars;
		}
	} // namespace

	void print_console_telemetry_table(const std::vector<ConsoleTelemetryRow>& rows, bool pretty_print, bool enable_unicode)
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

		const auto& chars = get_box_chars(enable_unicode);

		// Top border
		oss << chars.TL;
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << chars.H;

			oss << (i == widths.size() - 1 ? chars.TR : chars.TMID);
		}
		oss << "\n";

		// Header row (bold)
		oss << chars.V << "\033[1m";
		for (size_t i = 0; i < headers.size(); ++i)
		{
			oss << std::setw(widths[i]) << std::left << headers[i];
			oss << (i == headers.size() - 1 ? "\033[0m" : "\033[0m" + std::string(chars.V) + "\033[1m");
		}
		oss << chars.V << "\n";

		// Header separator
		oss << chars.LMID;
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << chars.H;

			oss << (i == widths.size() - 1 ? chars.RMID : chars.CENTER);
		}
		oss << "\n";

		// Data rows
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

				oss << chars.V << std::setw(width_type) << std::left << get(type_lines) << chars.V << std::setw(width_name) << get(name_lines)
					<< chars.V << std::setw(width_inputs) << get(input_lines) << chars.V << std::setw(width_outputs) << get(output_lines) << chars.V
					<< std::setw(width_tick) << get(tick_lines) << chars.V << std::setw(width_goal) << get(goal_lines) << chars.V
					<< colored_percent(get(percent_lines), row.percent, i == 0, pretty_print) << chars.V << "\n";
			}
		}

		// Bottom border
		oss << chars.BL;
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << chars.H;
			oss << (i == widths.size() - 1 ? chars.BR : chars.BMID);
		}
		oss << "\n";

		std::cout << oss.str() << std::flush;
	}
} // namespace robotick
