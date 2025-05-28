// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/data/Buffer.h"
#include "robotick/framework/data/FixedString.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

namespace robotick
{

	struct DataConnectionSeed
	{
		std::string source_path; // e.g. "A.output.temp"
		std::string dest_path;	 // e.g. "B.input.goal"
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

	class DataConnectionResolver
	{
	  public:
		static std::vector<DataConnectionInfo> resolve(const std::vector<DataConnectionSeed>& seeds, const BlackboardsBuffer& blackboards);
	};

} // namespace robotick
