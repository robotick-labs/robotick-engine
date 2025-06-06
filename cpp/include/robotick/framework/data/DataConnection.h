// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

namespace robotick
{

	class WorkloadInstanceInfo;
	struct WorkloadsBuffer;

	struct DataConnectionSeed
	{
		std::string source_field_path;
		std::string dest_field_path;
	};

	struct DataConnectionInfo
	{
		const DataConnectionSeed seed; // intentional copy of the original seed for safety
		const void* source_ptr = nullptr;
		void* dest_ptr = nullptr;
		const WorkloadInstanceInfo* source_workload = nullptr;
		const WorkloadInstanceInfo* dest_workload = nullptr;
		size_t size = 0;
		std::type_index type;

		enum class ExpectedHandler
		{
			Unassigned,
			SequencedGroupWorkload,
			ParentGroupOrEngine // set by a child-group if it wants parent-group (or Engine) to handle this update for them
		};
		ExpectedHandler expected_handler = ExpectedHandler::Unassigned;

		void do_data_copy() const noexcept
		{
			assert(source_ptr != nullptr && dest_ptr != nullptr && size > 0);
			static_assert(std::is_trivially_copyable_v<std::byte>, "do_data_copy() assumes trivially-copyable payloads");
			assert(source_ptr != dest_ptr && "Source and destination pointers are the same - this should have been caught in fixup");

			// If aliasing is possible, use std::memmove instead.
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

	class DataConnectionsFactory
	{
	  public:
		static std::vector<DataConnectionInfo> create(
			WorkloadsBuffer& workloads_buffer, const std::vector<DataConnectionSeed>& seeds, const std::vector<WorkloadInstanceInfo>& instances);
	};

} // namespace robotick
