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
	class WorkloadsBuffer;

	struct DataConnectionInfo;
	struct TickInfo;
	struct WorkloadInstanceInfo;

	struct FieldDescriptor
	{
		StringView name;
		TypeId type_id;
		size_t offset_within_struct = OFFSET_UNBOUND; // from start of host-struct (TODO - make this explicit in naming - ditto for parents)

		const TypeDescriptor* find_type_descriptor() const;

		void* get_data_ptr(void* container_ptr) const;

		void* get_data_ptr(WorkloadsBuffer& workloads_buffer,
			const WorkloadInstanceInfo& instance,
			const TypeDescriptor& struct_type,
			const size_t struct_offset) const;

		template <typename T> inline T& get_data(void* container_ptr) const
		{
			void* ptr = get_data_ptr(container_ptr);
			if (!ptr)
			{
				ROBOTICK_FATAL_EXIT("FieldInfo::get<T>() null pointer access for field '%s'", name.c_str());
			}

			return *static_cast<T*>((void*)ptr);
		}

		template <typename T>
		inline T& get_data(WorkloadsBuffer& workloads_buffer,
			const WorkloadInstanceInfo& instance,
			const TypeDescriptor& struct_type,
			const size_t struct_offset) const
		{
			void* ptr = get_data_ptr(workloads_buffer, instance, struct_type, struct_offset);
			if (!ptr)
			{
				ROBOTICK_FATAL_EXIT("FieldInfo::get<T>() null pointer access for field '%s'", name.c_str());
			}

			return *static_cast<T*>(ptr);
		}
	};

	struct StructDescriptor
	{
		ArrayView<FieldDescriptor> fields;

		const FieldDescriptor* find_field(const char* field_name) const;
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

		size_t config_offset = OFFSET_UNBOUND;	// from start of host-workload
		size_t inputs_offset = OFFSET_UNBOUND;	// ditto
		size_t outputs_offset = OFFSET_UNBOUND; // ditto

		// function pointers
		void (*construct_fn)(void*) = nullptr;
		void (*destruct_fn)(void*) = nullptr;

		void (*set_children_fn)(void*, const HeapVector<const WorkloadInstanceInfo*>&, HeapVector<DataConnectionInfo>&) = nullptr;
		void (*set_engine_fn)(void*, const Engine&) = nullptr;
		void (*pre_load_fn)(void*) = nullptr;
		void (*load_fn)(void*) = nullptr;
		void (*setup_fn)(void*) = nullptr;
		void (*start_fn)(void*, double) = nullptr;
		void (*tick_fn)(void*, const TickInfo&) = nullptr;
		void (*stop_fn)(void*) = nullptr;
	};

	enum class TypeCategory
	{
		Primitive,
		Struct,
		DynamicStruct,
		Workload
	};

	struct TypeDescriptor
	{
		StringView name;  // e.g. "int"
		TypeId id;		  // Unique ID per type
		size_t size;	  // Size in bytes
		size_t alignment; // Alignment in bytes

		TypeCategory type_category;

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

		// --- misc helpers: ---
		const WorkloadDescriptor* get_workload_desc() const
		{
			return (type_category == TypeCategory::Workload ? type_category_desc.workload_desc : nullptr);
		}

		const StructDescriptor* get_struct_desc() const { return (type_category == TypeCategory::Struct ? type_category_desc.struct_desc : nullptr); }

		const DynamicStructDescriptor* get_dynamic_struct_desc() const
		{
			return (type_category == TypeCategory::DynamicStruct ? type_category_desc.dynamic_struct_desc : nullptr);
		}

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

	extern const TypeDescriptor s_type_desc_void;

} // namespace robotick
