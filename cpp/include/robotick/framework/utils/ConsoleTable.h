// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

namespace robotick
{

	struct ConsoleTableRow
	{
		std::vector<std::string> columns;
	};

	/// Prints a general-purpose table with fixed-width columns and optional ANSI/Unicode formatting
	void print_console_table(const std::string& title, const std::vector<std::string>& headers, const std::vector<size_t>& widths,
		const std::vector<ConsoleTableRow>& rows, bool pretty_print, bool enable_unicode);

} // namespace robotick
