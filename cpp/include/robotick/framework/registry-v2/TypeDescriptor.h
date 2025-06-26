// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include "robotick/framework/common/ArrayView.h"
#include "robotick/framework/common/StringView.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"

#include <stddef.h>
#include <stdint.h>

namespace robotick
{
	struct TypeDescriptor;

	class Engine;
	struct DataConnectionInfo;
	struct TickInfo;
	struct WorkloadInstanceInfo;

	struct FieldDescriptor
	{
		StringView name;
		TypeId type_id;
		size_t offset = OFFSET_UNBOUND; // from start of host-struct

		const TypeDescriptor* find_type_descriptor() const;
	};

	struct StructDescriptor
	{
		ArrayView<FieldDescriptor> fields;
	};

	struct DynamicStructDescriptor
	{
		/// @brief Takes a void* to an instance (e.g. a blackboard), returns a StructDescriptor view of available fields
		const StructDescriptor* (*resolve_fn)(const void* instance);
	};

	struct WorkloadDescriptor
	{
		// data types and offsets
		const TypeDescriptor* config_desc = nullptr;
		const TypeDescriptor* inputs_desc = nullptr;
		const TypeDescriptor* outputs_desc = nullptr;

		size_t config_offset = SIZE_MAX; // from start of host-workload
		size_t input_offset = SIZE_MAX;	 // ditto
		size_t output_offset = SIZE_MAX; // ditto

		// function pointers
		void (*construct_fn)(void*) = nullptr;
		void (*destruct_fn)(void*) = nullptr;

		void (*set_children_fn)(void*, const std::vector<const WorkloadInstanceInfo*>&, std::vector<DataConnectionInfo*>&) = nullptr;
		void (*set_engine_fn)(void*, const Engine&) = nullptr;
		void (*pre_load_fn)(void*) = nullptr;
		void (*load_fn)(void*) = nullptr;
		void (*setup_fn)(void*) = nullptr;
		void (*start_fn)(void*, double) = nullptr;
		void (*tick_fn)(void*, const TickInfo&) = nullptr;
		void (*stop_fn)(void*) = nullptr;
	};

	struct TypeDescriptor
	{
		StringView name;  // e.g. "int"
		TypeId id;		  // Unique ID per type
		size_t size;	  // Size in bytes
		size_t alignment; // Alignment in bytes

		enum class TypeCategory
		{
			Primitive,
			Struct,
			DynamicStruct,
			Workload
		} type_category;

		union TypeCategoryDesc
		{
			const StructDescriptor* struct_desc;
			const WorkloadDescriptor* workload_desc;
			const DynamicStructDescriptor* dynamic_struct_desc;
		};

		TypeCategoryDesc type_category_desc{};

		// Converts data to string form, writing to buffer. Null-terminated.
		// Returns true if successful.
		bool (*to_string)(const void* data, char* out_buffer, size_t buffer_size);

		// Parses string and stores result in out_data.
		bool (*from_string)(const char* str, void* out_data);

		// --- templated string helpers: ---

		template <typename TData, typename TString> inline bool to_string_typed(const TData& value, TString& output) const
		{
			return to_string(&value, output.str(), output.capacity());
		}

		template <typename TData, typename TString> inline bool from_string_typed(const TString& input, TData& out_value) const
		{
			return from_string(input.c_str(), &out_value);
		}
	};

} // namespace robotick
