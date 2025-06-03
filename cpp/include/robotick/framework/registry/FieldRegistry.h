// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstddef>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace robotick
{
	class WorkloadsBuffer;
	struct WorkloadInstanceInfo;
	struct StructRegistryEntry;

	struct FieldInfo
	{
		std::string name;
		size_t offset_within_struct = OFFSET_UNBOUND;
		TypeId type = TypeId::invalid();
		size_t size = 0;

		uint8_t* get_data_ptr(WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_info) const;

		template <typename T>
		inline T& get_data(WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_info) const
		{
			uint8_t* ptr = get_data_ptr(workloads_buffer, instance, struct_info);
			if (!ptr)
			{
				ROBOTICK_FATAL_EXIT("FieldInfo::get<T>() null pointer access for field '%s'", name.c_str());
			}

			return *static_cast<T*>((void*)ptr);
		}
	};

	struct StructRegistryEntry
	{
		std::string name;
		size_t size = 0;
		TypeId type = TypeId::invalid();
		size_t offset_within_workload = OFFSET_UNBOUND;
		std::vector<FieldInfo> fields;
		std::map<std::string, FieldInfo*> field_from_name;
	};

	class FieldRegistry
	{
	  public:
		static FieldRegistry& get();

		const StructRegistryEntry* register_struct(const std::string& name, size_t size, TypeId type, size_t offset, std::vector<FieldInfo> fields);

		const StructRegistryEntry* get_struct(const std::string& name) const;

	  private:
		std::unordered_map<std::string, StructRegistryEntry> entries;
		mutable std::mutex mutex;
	};

} // namespace robotick

// ——————————————————————————————————————————————————————————————————
// Field registration macros (non-templated)
// ——————————————————————————————————————————————————————————————————

#define ROBOTICK_BEGIN_FIELDS(StructType)                                                                                                            \
	static_assert(std::is_standard_layout<StructType>::value, "Structs must be standard layout");                                                    \
	[[maybe_unused]] static const robotick::StructRegistryEntry* s_robotick_fields_##StructType = robotick::FieldRegistry::get().register_struct(                     \
		#StructType, sizeof(StructType), GET_TYPE_ID(StructType), robotick::OFFSET_UNBOUND, {

#define ROBOTICK_FIELD(StructType, FieldType, FieldName)                                                                                             \
	{#FieldName, offsetof(StructType, FieldName), GET_TYPE_ID(FieldType), sizeof(decltype(StructType::FieldName))},

#define ROBOTICK_END_FIELDS()                                                                                                                        \
	});                                                                                                                                              \
	/**/
