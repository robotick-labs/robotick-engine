// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstdint>

namespace robotick
{
	class Blackboard;

	struct BlackboardInfo
	{
		StructDescriptor struct_descriptor;

		size_t datablock_offset_from_blackboard = OFFSET_UNBOUND;
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

		const FieldDescriptor* find_field(const char* field_name) const;
		void* find_field_data(const char* field_name, const FieldDescriptor*& found_field) const;

		bool has(const char* field_name) const;
		bool set(const char* field_name, void* value, const size_t size);
		void* get(const char* field_name, const size_t size) const;

		template <typename T> bool set(const char* field_name, const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Blackboard::set only supports trivially-copyable types");
			return set(field_name, (void*)&value, sizeof(T));
		}

		template <typename T> T get(const char* field_name) const
		{
			void* found_value = get(field_name, sizeof(T));
			if (found_value)
			{
				return *static_cast<T*>(found_value);
			}

			return T();
		}

		static const StructDescriptor* resolve_descriptor(const void* instance);

	  public: // non-API accessors - used for setup and lower-level querying of Blackboard
		void bind(size_t datablock_offset) { info.datablock_offset_from_blackboard = datablock_offset; };

		size_t get_datablock_offset() const { return info.datablock_offset_from_blackboard; };

		const StructDescriptor& get_struct_descriptor() const { return info.struct_descriptor; };
		const BlackboardInfo& get_info() const { return info; };

	  private:
		BlackboardInfo info;
	};
} // namespace robotick