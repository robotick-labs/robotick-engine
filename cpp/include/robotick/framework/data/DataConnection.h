// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include <cassert>
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

		void copy_data() const
		{
			assert(source_ptr != nullptr && "DataConnectionInfo: source_ptr is null");
			assert(dest_ptr != nullptr && "DataConnectionInfo: dest_ptr is null");
			assert(size > 0 && "DataConnectionInfo: size must be greater than 0");
			assert(source_ptr != dest_ptr && "DataConnectionInfo: source_ptr and dest_ptr must not alias");

			std::memcpy(dest_ptr, source_ptr, size);
		}
	};

	struct ParsedFieldPath
	{
		FixedString64 workload_name;
		FixedString64 section_name; // workload_name/section_name/field_path[0](/field_path[1])
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
