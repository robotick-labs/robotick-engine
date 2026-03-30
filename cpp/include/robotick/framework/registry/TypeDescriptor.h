// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include "robotick/framework/containers/ArrayView.h"
#include "robotick/framework/containers/HeapVector.h"
#include "robotick/framework/strings/StringView.h"
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

	struct EnumValue
	{
		StringView name;
		uint64_t value = 0;
	};

	struct EnumDescriptor
	{
		ArrayView<EnumValue> values;
		size_t underlying_size = 0;
		bool is_signed = true;
		bool is_flags = false;
	};

	struct FieldDescriptor
	{
		StringView name;
		TypeId type_id;
		size_t offset_within_container = OFFSET_UNBOUND;
		size_t element_count = 1; // number of times our type repeats in each field (e.g. array[element_count])

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
				ROBOTICK_FATAL_EXIT("FieldDescriptor::get_data<T>() null pointer access for field '%s'", name.c_str());
			}

			return *static_cast<T*>(ptr);
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
				ROBOTICK_FATAL_EXIT("FieldDescriptor::get_data<T>() null pointer access for field '%s'", name.c_str());
			}

			return *static_cast<T*>(ptr);
		}
	};

	struct StructDescriptor
	{
		ArrayView<FieldDescriptor> fields;

		const FieldDescriptor* find_field(const char* field_name) const;
	};

	struct DynamicStructStoragePlan
	{
		size_t size_bytes = 0;
		size_t alignment = 1;
	};

	struct DynamicStructDescriptor
	{
		/// @brief Takes a void* to an instance (e.g. a blackboard), returns a StructDescriptor view of available fields
		const StructDescriptor* (*resolve_fn)(const void* instance);
		bool (*plan_storage_fn)(const void* instance, DynamicStructStoragePlan& out_plan);
		bool (*bind_storage_fn)(
			void* instance, const WorkloadsBuffer& workloads_buffer, size_t storage_offset_in_workloads_buffer, size_t storage_size_bytes);

		const StructDescriptor* get_struct_descriptor(const void* instance) const
		{
			ROBOTICK_ASSERT_MSG(resolve_fn != nullptr, "DynamicStructDescriptor::get_struct_descriptor() requires resolve_fn");
			return resolve_fn(instance);
		}

		bool has_storage_callbacks() const
		{
			const bool has_plan = (plan_storage_fn != nullptr);
			const bool has_bind = (bind_storage_fn != nullptr);
			ROBOTICK_ASSERT_MSG(has_plan == has_bind,
				"Dynamic struct storage callbacks must be both present or both absent (plan=%d bind=%d)",
				static_cast<int>(has_plan),
				static_cast<int>(has_bind));
			return has_plan;
		}

		bool plan_storage(const void* instance, DynamicStructStoragePlan& out_plan) const
		{
			ROBOTICK_ASSERT_MSG(plan_storage_fn != nullptr, "DynamicStructDescriptor::plan_storage() requires plan_storage_fn");
			return plan_storage_fn(instance, out_plan);
		}

		bool bind_storage(
			void* instance, const WorkloadsBuffer& workloads_buffer, size_t storage_offset_in_workloads_buffer, size_t storage_size_bytes) const
		{
			ROBOTICK_ASSERT_MSG(bind_storage_fn != nullptr, "DynamicStructDescriptor::bind_storage() requires bind_storage_fn");
			return bind_storage_fn(instance, workloads_buffer, storage_offset_in_workloads_buffer, storage_size_bytes);
		}
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
		void (*start_fn)(void*, float) = nullptr;
		void (*tick_fn)(void*, const TickInfo&) = nullptr;
		void (*stop_fn)(void*) = nullptr;
	};

	enum class TypeCategory
	{
		Primitive,
		Enum,
		Struct,
		DynamicStruct,
		Workload
	};

	// Converts data to string form, writing to buffer. Null-terminated.
	// Returns true if successful.
	using ToStringFn = bool (*)(const void* data, char* out_buffer, size_t buffer_size);

	// Parses string and stores result in out_data.
	// Returns true if successful.
	using FromStringFn = bool (*)(const char* str, void* out_data);

	struct TypeDescriptor
	{
		StringView name;  // e.g. "int"
		TypeId id;		  // Unique ID per type
		size_t size;	  // Size in bytes
		size_t alignment; // Alignment in bytes

		TypeCategory type_category;

		union TypeCategoryDesc
		{
			const void* any_desc; // allows construction from brace-init list pre-C++20
			const EnumDescriptor* enum_desc;
			const StructDescriptor* struct_desc;
			const WorkloadDescriptor* workload_desc;
			const DynamicStructDescriptor* dynamic_struct_desc;
		};

		TypeCategoryDesc type_category_desc{};

		StringView mime_type; // http-style metadata - e.g. "img/png" (optional)

		// --- misc helpers: ---
		const WorkloadDescriptor* get_workload_desc() const
		{
			return (type_category == TypeCategory::Workload ? type_category_desc.workload_desc : nullptr);
		}

		const EnumDescriptor* get_enum_desc() const { return (type_category == TypeCategory::Enum ? type_category_desc.enum_desc : nullptr); }

		const StructDescriptor* get_struct_desc() const { return (type_category == TypeCategory::Struct ? type_category_desc.struct_desc : nullptr); }

		const DynamicStructDescriptor* get_dynamic_struct_desc() const
		{
			return (type_category == TypeCategory::DynamicStruct ? type_category_desc.dynamic_struct_desc : nullptr);
		}

		bool to_string(const void* value, char* output_buffer, size_t output_buffer_size) const;
		bool from_string(const char* input, void* out_value) const;
	};

	extern const TypeDescriptor s_type_desc_void;

} // namespace robotick
