// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/utils/Typename.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
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
		size_t offset = 0;
		std::type_index type = typeid(void);
		size_t size = 0;

		uint8_t* get_data_ptr(
			const WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_info) const;
	};

	struct StructRegistryEntry
	{
		std::string name;
		size_t offset = 0;
		std::type_index type = typeid(void);
		size_t size = 0;
		std::vector<FieldInfo> fields;
	};

	class FieldRegistry
	{
	  public:
		static FieldRegistry& get();

		const StructRegistryEntry* register_struct(
			const std::string& name, size_t size, const std::type_index& type, size_t offset, std::vector<FieldInfo> fields);

		const StructRegistryEntry* get_struct(const std::string& name) const;

	  private:
		std::unordered_map<std::string, StructRegistryEntry> entries;
		mutable std::mutex mutex;
	};

	// 🔍 Reflection helper
	template <typename StructType> inline const StructRegistryEntry* get_struct_reflection()
	{
		return FieldRegistry::get().get_struct(get_clean_typename(typeid(StructType)));
	}

	template <typename StructType> struct FieldAutoRegister
	{
		FieldAutoRegister(std::vector<FieldInfo> fields)
		{
			const size_t offset = 0; // we don't know this as we're not registered with the parent workload - that will supply this info later (see
									 // TODO in method we call - pattern feels wrong)

			FieldRegistry::get().register_struct(
				get_clean_typename(typeid(StructType)), sizeof(StructType), typeid(StructType), offset, std::move(fields));
		}
	};

} // namespace robotick

#define ROBOTICK_BEGIN_FIELDS(StructType)                                                                                                            \
	static_assert(std::is_standard_layout<StructType>::value, "Structs must be standard layout");                                                    \
	static robotick::FieldAutoRegister<StructType>                       \
        s_robotick_fields_##StructType({

#define ROBOTICK_FIELD(StructType, FieldName)                                                                                                        \
	{#FieldName, offsetof(StructType, FieldName), typeid(decltype(StructType::FieldName)), sizeof(decltype(StructType::FieldName))},

#define ROBOTICK_END_FIELDS()                                                                                                                        \
	});
