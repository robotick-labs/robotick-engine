// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

namespace robotick
{

	struct DataConnectionSeed
	{
		std::string source_path;
		std::string dest_path;
	};

	struct DataConnectionInfo
	{
		const void* source_ptr;
		void* dest_ptr;
		size_t size;
		std::type_index type;
	};

	struct ParsedFieldPath
	{
		FixedString64 workload_name;
		FixedString64 section_name; // input/output/config
		std::vector<FixedString64> field_path;
	};

	class FieldPathParseError : public std::runtime_error
	{
	  public:
		explicit FieldPathParseError(const std::string& msg);
	};

	ParsedFieldPath parse_field_path(const std::string& raw);
	bool is_valid_section(const std::string& s);

	class WorkloadInstanceInfo;
	class DataConnectionResolver
	{
	  public:
		static std::vector<DataConnectionInfo> resolve(
			const std::vector<DataConnectionSeed>& seeds, const std::vector<WorkloadInstanceInfo>& instances);
	};

} // namespace robotick
