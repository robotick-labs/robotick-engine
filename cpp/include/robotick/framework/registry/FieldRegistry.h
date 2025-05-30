// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/utils/Constants.h"
#include "robotick/framework/utils/Typename.h"

#include <cstddef> // for size_t
#include <mutex>
#include <stdexcept> // for std::runtime_error
#include <string>	 // for std::string
#include <type_traits>
#include <typeindex> // for std::type_index
#include <typeinfo>	 // for typeid
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
		size_t offset = OFFSET_UNBOUND;
		std::type_index type = typeid(void);
		size_t size = 0;

		uint8_t* get_data_ptr(WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_info) const;

		template <typename T>
		inline T& get_data(WorkloadsBuffer& workloads_buffer, const WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_info) const
		{
			if (type != std::type_index(typeid(T)))
			{
				throw std::runtime_error("FieldInfo::get<T>() type mismatch for field '" + name + "'");
			}

			uint8_t* ptr = get_data_ptr(workloads_buffer, instance, struct_info);
			if (!ptr)
			{
				throw std::runtime_error("FieldInfo::get<T>() null pointer access for field '" + name + "'");
			}

			return *static_cast<T*>((void*)ptr);
		}
	};

	struct StructRegistryEntry
	{
		std::string name;
		size_t offset = OFFSET_UNBOUND;
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
			const size_t offset = OFFSET_UNBOUND; // we don't know this as we're not registered with the parent workload yet (TODO - address this
												  // strange double-register patterm)

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
