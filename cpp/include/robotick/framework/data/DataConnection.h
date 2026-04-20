// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/containers/Map.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringView.h"
#include "robotick/framework/utility/Pair.h"
#include "robotick/framework/utils/TypeId.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace robotick
{
	class Engine;
	struct WorkloadInstanceInfo;

	struct DataConnectionSeed;
	struct FieldDescriptor;
	struct TypeDescriptor;
	class WorkloadsBuffer;

	struct DataConnectionInputHandle
	{
		uint16_t handle_id = 0;
		FixedString512 path;
		void* dest_ptr = nullptr;
		size_t size = 0;
		TypeId type;
		AtomicValue<uint8_t> enabled{1};

		bool is_enabled() const noexcept { return enabled.load(std_approved::memory_order_acquire) != 0; }
		void set_enabled(bool value) noexcept
		{
			enabled.store(value ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0), std_approved::memory_order_release);
		}

		void do_copy_data(const void* src_ptr) const noexcept
		{
			ROBOTICK_ASSERT(src_ptr != nullptr && dest_ptr != nullptr && size > 0);
			ROBOTICK_ASSERT(src_ptr != dest_ptr && "Source and destination pointers are the same - this should have been caught in fixup");
			if (!is_enabled())
			{
				return;
			}

			::memcpy(dest_ptr, src_ptr, size);
		}

		void do_copy_data_partial(const void* src_ptr, size_t offset, size_t bytes) const noexcept
		{
			ROBOTICK_ASSERT(src_ptr != nullptr && dest_ptr != nullptr);
			ROBOTICK_ASSERT(offset <= size);
			ROBOTICK_ASSERT(bytes <= size - offset);
			if (!is_enabled() || bytes == 0)
			{
				return;
			}

			::memcpy(static_cast<uint8_t*>(dest_ptr) + offset, src_ptr, bytes);
		}

		void do_copy_data_partial_unchecked(const void* src_ptr, size_t offset, size_t bytes) const noexcept
		{
			ROBOTICK_ASSERT(src_ptr != nullptr && dest_ptr != nullptr);
			ROBOTICK_ASSERT(offset <= size);
			ROBOTICK_ASSERT(bytes <= size - offset);
			if (bytes == 0)
			{
				return;
			}

			::memcpy(static_cast<uint8_t*>(dest_ptr) + offset, src_ptr, bytes);
		}
	};

	struct DataConnectionInfo
	{
		const DataConnectionSeed* seed = nullptr;
		const void* source_ptr = nullptr;
		void* dest_ptr = nullptr;
		DataConnectionInputHandle* dest_handle = nullptr;
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
			if (dest_handle)
			{
				dest_handle->do_copy_data(source_ptr);
				return;
			}

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
			const bool fatalExitIfNotFound = true);

		/// @brief Given a dot-separated field path (e.g. "MyWorkload.outputs.x"), returns the raw pointer, size in bytes, and field-descriptor
		static FieldInfo find_field_info(const Engine& engine, const char* path);
	};

} // namespace robotick
