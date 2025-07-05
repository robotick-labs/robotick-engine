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

		bool has_field(const char* key) const;
		const FieldDescriptor* find_field(const char* key) const;

		void* get_field_ptr(Blackboard* blackboard, const char* key) const;
		const void* get_field_ptr(const Blackboard* blackboard, const char* key) const;
	};

	class Blackboard
	{
	  public: // common accessors (part of API)
		Blackboard() = default;

		Blackboard(const Blackboard&) = delete;
		Blackboard& operator=(const Blackboard&) = delete;

		void initialize_fields(const HeapVector<FieldDescriptor>& fields);
		void initialize_fields(const ArrayView<FieldDescriptor>& fields);

		template <typename T> void set(const std::string& key, const T& value);
		template <typename T> T get(const std::string& key) const;

		static const StructDescriptor* resolve_descriptor(const void* instance);

	  public: // non-API accessors - used for setup and lower-level querying of Blackboard
		void bind(size_t datablock_offset);

		size_t get_datablock_offset() const;

		const StructDescriptor& get_struct_descriptor() const { return info.struct_descriptor; };
		const BlackboardInfo& get_info() const { return info; };

	  private:
		BlackboardInfo info;
	};
} // namespace robotick