// Copyright Robotick
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/common/Map.h"
#include "robotick/framework/common/Pair.h"
#include "robotick/framework/common/StringView.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

namespace robotick
{
	class Engine;
	class WorkloadInstanceInfo;

	struct DataConnectionSeed;
	struct FieldDescriptor;
	struct TypeDescriptor;
	struct WorkloadsBuffer;

	struct DataConnectionInfo
	{
		const DataConnectionSeed* seed = nullptr;
		const void* source_ptr = nullptr;
		void* dest_ptr = nullptr;
		const WorkloadInstanceInfo* source_workload = nullptr;
		const WorkloadInstanceInfo* dest_workload = nullptr;
		size_t size = 0;
		TypeId type;

		enum class ExpectedHandler
		{
			Unassigned,
			SequencedGroupWorkload,
			Engine,
			DelegateToParent // set by a child-group if it wants parent-group (or Engine) to handle this update for them
		};
		ExpectedHandler expected_handler = ExpectedHandler::Unassigned;

		void do_data_copy() const noexcept
		{
			ROBOTICK_ASSERT(source_ptr != nullptr && dest_ptr != nullptr && size > 0);
			static_assert(std::is_trivially_copyable_v<std::byte>, "do_data_copy() assumes trivially-copyable payloads");
			ROBOTICK_ASSERT(source_ptr != dest_ptr && "Source and destination pointers are the same - this should have been caught in fixup");

			// If aliasing is possible, use std::memmove instead.
			std::memcpy(dest_ptr, source_ptr, size);
		}
	};

	class FieldPathParseError : public std::runtime_error
	{
	  public:
		explicit FieldPathParseError(const std::string& msg);
	};

	using FieldConfigEntry = Pair<FixedString64, FixedString64>;

	class DataConnectionUtils
	{
	  public:
		/// @brief Creates and resolves data connections between workload instances based on the provided connection seeds.
		static void create(HeapVector<DataConnectionInfo>& out_connections,
			WorkloadsBuffer& workloads_buffer,
			const ArrayView<const DataConnectionSeed*>& seeds,
			const Map<const char*, WorkloadInstanceInfo*>& instances);

		/// @brief Applies a set of field configuration overrides to a given struct by matching and writing string-based field values.
		static void apply_struct_field_values(
			void* struct_ptr, const TypeDescriptor& struct_type_desc, const ArrayView<const FieldConfigEntry>& field_config_entries)
		{
			const StructDescriptor* struct_desc = struct_type_desc.get_struct_desc();
			if (!struct_desc)
			{
				ROBOTICK_FATAL_EXIT("Struct with no struct desc");
			}

			for (const FieldConfigEntry& field_config_entry : field_config_entries)
			{
				const FieldDescriptor* found_field = struct_desc->find_field(field_config_entry.first.c_str());
				if (!found_field)
					continue;

				ROBOTICK_ASSERT_MSG(found_field->offset != OFFSET_UNBOUND, "Field offset should have been correctly set by now");

				void* field_ptr = static_cast<uint8_t*>(struct_ptr) + found_field->offset;
				const FixedString64& value = field_config_entry.second;

				const TypeDescriptor* field_type_desc = found_field->find_type_descriptor();
				if (!field_type_desc)
				{
					ROBOTICK_FATAL_EXIT("Field offset should have been correctly set by now");
				}

				if (!field_type_desc->from_string(value.c_str(), field_ptr))
				{
					ROBOTICK_WARNING("Unable to parse value-string '%s' for field: %s", value.c_str(), found_field->name.c_str());
				}
			}
		}

		/// @brief Given a dot-separated field path (e.g. "MyWorkload.outputs.x"), returns the raw pointer, size in bytes, and field-descriptor
		static std::tuple<void*, size_t, const FieldDescriptor*> find_field_info(const Engine& engine, const std::string& path)
		{
			(void)engine;
			(void)path;
			ROBOTICK_FATAL_EXIT("Not implemented!");
			return std::tuple<void*, size_t, const FieldDescriptor*>();
		}
	};

} // namespace robotick
