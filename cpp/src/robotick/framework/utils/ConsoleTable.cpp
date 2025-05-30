// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/utils/ConsoleTable.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace robotick
{
	namespace
	{
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
	} // namespace

	void print_console_table(const std::string& title, const std::vector<std::string>& headers, const std::vector<size_t>& widths,
		const std::vector<ConsoleTableRow>& rows, bool pretty_print, bool enable_unicode)
	{
		constexpr const char* GREEN = "\033[32m";
		constexpr const char* YELLOW = "\033[33m";
		constexpr const char* RED = "\033[31m";
		constexpr const char* RESET = "\033[0m";

		const auto& chars = get_box_chars(enable_unicode);
		std::ostringstream oss;
		oss << "\033[2J\033[H"; // clear + home
		oss << "\n=== " << title << " ===\n\n";

		if (!pretty_print)
		{
			for (auto& h : headers)
				oss << h << "\t";
			oss << "\n";
			for (auto& row : rows)
			{
				for (auto& col : row.columns)
					oss << col << "\t";
				oss << "\n";
			}
			std::cout << oss.str() << std::flush;
			return;
		}

		// top border
		oss << chars.TL;
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << chars.H;
			oss << (i + 1 == widths.size() ? chars.TR : chars.TMID);
		}
		oss << "\n";

		// header row (bold)
		oss << chars.V << "\033[1m";
		for (size_t i = 0; i < headers.size(); ++i)
		{
			oss << std::setw(widths[i]) << std::left << headers[i] << chars.V << (i + 1 < headers.size() ? std::string() + "\033[1m" : std::string());
		}
		oss << "\033[0m\n";

		// header separator
		oss << chars.LMID;
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << chars.H;
			oss << (i + 1 == widths.size() ? chars.RMID : chars.CENTER);
		}
		oss << "\n";

		// data rows
		for (auto& row : rows)
		{
			// wrap each column to its width
			std::vector<std::vector<std::string>> wrapped;
			for (size_t i = 0; i < row.columns.size(); ++i)
				wrapped.push_back(wrap(row.columns[i], widths[i]));

			size_t max_lines = 0;
			for (auto& col : wrapped)
				max_lines = std::max(max_lines, col.size());

			for (size_t line = 0; line < max_lines; ++line)
			{
				oss << chars.V;
				for (size_t col = 0; col < wrapped.size(); ++col)
				{
					std::string cell = (line < wrapped[col].size() ? wrapped[col][line] : "");

					// if it's a "%"-column, apply color *around* the setw
					if (pretty_print && headers[col].find('%') != std::string::npos)
					{
						// extract numeric prefix
						double val = 0.0;
						try
						{
							val = std::stod(cell);
						}
						catch (...)
						{
						}

						const char* color = (val <= 105.0) ? GREEN : (val < 110.0) ? YELLOW : RED;

						oss << color << std::setw(widths[col]) << std::left << cell << RESET;
					}
					else
					{
						oss << std::setw(widths[col]) << std::left << cell;
					}
					oss << chars.V;
				}
				oss << "\n";
			}
		}

		// bottom border
		oss << chars.BL;
		for (size_t i = 0; i < widths.size(); ++i)
		{
			for (size_t j = 0; j < widths[i]; ++j)
				oss << chars.H;
			oss << (i + 1 == widths.size() ? chars.BR : chars.BMID);
		}
		oss << "\n";

		std::cout << oss.str() << std::flush;
	}
} // namespace robotick
