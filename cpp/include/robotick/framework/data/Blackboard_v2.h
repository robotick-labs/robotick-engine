// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/registry-v2/TypeDescriptor.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstdint>

namespace robotick
{
	class Blackboard_v2;

	struct BlackboardInfo_v2
	{
		StructDescriptor struct_descriptor;

		size_t datablock_offset_from_blackboard = OFFSET_UNBOUND;

		bool has_field(const char* key) const;
		const FieldDescriptor* find_field(const char* key) const;

		void* get_field_ptr(Blackboard_v2* blackboard, const char* key) const;
		const void* get_field_ptr(const Blackboard_v2* blackboard, const char* key) const;
	};

	class Blackboard_v2
	{
		friend class Engine;
		friend struct BlackboardFieldInfo;
		friend struct BlackboardInfo;
		friend struct BlackboardTestUtils;
		friend struct DataConnectionUtils;
		friend struct DataConnectionsFactory;
		friend struct MqttFieldSync;
		friend struct PythonWorkload;
		friend struct WorkloadFieldsIterator;

	  public:
		Blackboard_v2() = default;

		Blackboard_v2(const Blackboard_v2&) = delete;
		Blackboard_v2& operator=(const Blackboard_v2&) = delete;

		void initialize_fields(const ArrayView<FieldDescriptor>& fields);

		template <typename T> void set(const std::string& key, const T& value);
		template <typename T> T get(const std::string& key) const;

		static const StructDescriptor* resolve_descriptor(const void* instance);

	  protected:
		void bind(size_t datablock_offset);

		size_t get_datablock_offset() const;

		const StructDescriptor& get_struct_descriptor() const { return info.struct_descriptor; };
		const BlackboardInfo_v2& get_info() const { return info; };

	  private:
		BlackboardInfo_v2 info;
	};
} // namespace robotick