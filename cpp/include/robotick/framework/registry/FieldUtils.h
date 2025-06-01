// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <any>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace robotick
{
	template <typename T> T& any_cast_ref(std::any& val)
	{
		if (!val.has_value())
			ROBOTICK_ERROR("Missing config value");
		return std::any_cast<T&>(val);
	}

	template <typename T> struct type_identity
	{
		using type = T;
	};

	template <typename Func> inline bool dispatch_fixed_string(const TypeId& type, Func&& fn)
	{
		if (type == get_type_id<FixedString8>())
			return fn(type_identity<FixedString8>{});
		if (type == get_type_id<FixedString16>())
			return fn(type_identity<FixedString16>{});
		if (type == get_type_id<FixedString32>())
			return fn(type_identity<FixedString32>{});
		if (type == get_type_id<FixedString64>())
			return fn(type_identity<FixedString64>{});
		if (type == get_type_id<FixedString128>())
			return fn(type_identity<FixedString128>{});
		if (type == get_type_id<FixedString256>())
			return fn(type_identity<FixedString256>{});
		if (type == get_type_id<FixedString512>())
			return fn(type_identity<FixedString512>{});
		if (type == get_type_id<FixedString1024>())
			return fn(type_identity<FixedString1024>{});
		return false;
	}

	inline void apply_struct_fields(void* struct_ptr, const StructRegistryEntry& info, const std::map<std::string, std::any>& config)
	{
		for (const auto& field : info.fields)
		{
			auto it = config.find(field.name);
			if (it == config.end())
				continue;

			ROBOTICK_ASSERT(field.offset_within_struct != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

			void* field_ptr = static_cast<uint8_t*>(struct_ptr) + field.offset_within_struct;

			if (field.type == get_type_id<std::string>())
			{
				*static_cast<std::string*>(field_ptr) = std::any_cast<std::string>(it->second);
			}
			else if (field.type == get_type_id<double>())
			{
				*static_cast<double*>(field_ptr) = std::any_cast<double>(it->second);
			}
			else if (field.type == get_type_id<int>())
			{
				*static_cast<int*>(field_ptr) = std::any_cast<int>(it->second);
			}
			else if (dispatch_fixed_string(field.type,
						 [&](auto T) -> bool
						 {
							 using FS = typename decltype(T)::type;
							 if (it->second.type() == typeid(std::string))
							 {
								 *static_cast<FS*>(field_ptr) = std::any_cast<std::string>(it->second).c_str();
							 }
							 else if (it->second.type() == typeid(const char*))
							 {
								 *static_cast<FS*>(field_ptr) = std::any_cast<const char*>(it->second);
							 }
							 else
							 {
								 ROBOTICK_ERROR("Invalid type for FixedString field: %s", field.name.c_str());
							 }
							 return true;
						 }))
			{
				continue;
			}
			else
			{
				ROBOTICK_ERROR("Unsupported config type: %s for field: %s", get_registered_type_name_from_id(field.type), field.name.c_str());
			}
		}
	}

} // namespace robotick
