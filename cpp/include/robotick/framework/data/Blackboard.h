// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/utility/TypeTraits.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstdint>

namespace robotick
{
	class Blackboard;

	struct BlackboardInfo
	{
		StructDescriptor struct_descriptor;

		size_t total_datablock_size = 0;

		bool has_field(const char* field_name) const { return find_field(field_name) != nullptr; }
		const FieldDescriptor* find_field(const char* field_name) const { return struct_descriptor.find_field(field_name); }
	};

	class Blackboard
	{
	  public: // common accessors (part of API)
		Blackboard() = default;

		Blackboard(const Blackboard&) = delete;
		Blackboard& operator=(const Blackboard&) = delete;

		void initialize_fields(const HeapVector<FieldDescriptor>& fields);
		void initialize_fields(const ArrayView<FieldDescriptor>& fields);

		static const StructDescriptor* resolve_descriptor(const void* instance);
		static bool plan_storage(const void* instance, DynamicStructStoragePlan& out_plan);
		static bool bind_storage(
			void* instance, const WorkloadsBuffer& workloads_buffer, size_t storage_offset_in_workloads_buffer, size_t storage_size_bytes);

	  public: // field-query methods (by name - slower as requires string-based lookup)
		const FieldDescriptor* find_field(const char* field_name) const;
		void* find_field_data(const char* field_name, const FieldDescriptor*& found_field) const;

		bool has(const char* field_name) const;
		bool set(const char* field_name, void* value, const size_t size);
		void* get(const char* field_name, const size_t size) const;

		template <typename T> bool set(const char* field_name, const T& value)
		{
			static_assert(robotick::is_trivially_copyable_v<T>, "Blackboard::set only supports trivially-copyable types");
			return set(field_name, (void*)&value, sizeof(T));
		}

		template <typename T> T get(const char* field_name) const
		{
			void* found_value = get(field_name, sizeof(T));
			if (found_value)
			{
				return *static_cast<T*>(found_value);
			}

			ROBOTICK_FATAL_EXIT("Blackboard::get called with unknown field-name: '%s'", field_name);
			return T();
		}

	  public: // field-query methods (by pre-cached FieldDescriptor - fast)
		bool set(const FieldDescriptor& field, void* value, size_t size);
		void* get(const FieldDescriptor& field, size_t size) const;

		template <typename T> bool set(const FieldDescriptor& field, const T& value)
		{
			static_assert(robotick::is_trivially_copyable_v<T>, "Blackboard::set only supports trivially-copyable types");
			return set(field, (void*)&value, sizeof(T));
		}

		template <typename T> T get(const FieldDescriptor& field) const
		{
			void* found_value = get(field, sizeof(T));
			if (found_value)
			{
				return *static_cast<T*>(found_value);
			}

			ROBOTICK_FATAL_EXIT("Blackboard::get called with invalid field reference: '%s'", field.name.c_str());
			return T();
		}

	  public: // non-API accessors - used for setup and lower-level querying of Blackboard
		void bind(const WorkloadsBuffer& workloads_buffer, size_t& datablock_offset_in_workloads_buffer);

		const StructDescriptor& get_struct_descriptor() const { return info.struct_descriptor; };
		const BlackboardInfo& get_info() const { return info; };

	  protected:
		void compute_total_datablock_size();
		size_t compute_storage_alignment() const;

	  private:
		BlackboardInfo info;
	};

} // namespace robotick
