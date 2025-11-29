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
			ROBOTICK_ASSERT(source_ptr != dest_ptr && "Source and destination pointers are the same - this should have been caught in fixup");

			::memcpy(dest_ptr, source_ptr, size);
		}
	};

	struct FieldInfo
	{
		void* ptr = nullptr;
		size_t size = 0;
		const FieldDescriptor* descriptor = nullptr;
	};

	using FieldConfigEntry = Pair<StringView, StringView>;

	class DataConnectionUtils
	{
	  public:
		/// @brief Creates and resolves data connections between workload instances based on the provided connection seeds.
		static void create(HeapVector<DataConnectionInfo>& out_connections,
			WorkloadsBuffer& workloads_buffer,
			const ArrayView<const DataConnectionSeed*>& seeds,
			const Map<const char*, WorkloadInstanceInfo*>& instances);

		/// @brief Applies a set of field configuration overrides to a given struct by matching and writing string-based field values.
		static void apply_struct_field_values(void* struct_ptr,
			const TypeDescriptor& struct_type_desc,
			const ArrayView<const FieldConfigEntry>& field_config_entries,
			const bool warnIfNotFound = true);

		/// @brief Given a dot-separated field path (e.g. "MyWorkload.outputs.x"), returns the raw pointer, size in bytes, and field-descriptor
		static FieldInfo find_field_info(const Engine& engine, const char* path);
	};

} // namespace robotick
