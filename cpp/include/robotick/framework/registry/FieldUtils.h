// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"
#include "robotick/framework/common/FixedString.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/utils/TypeId.h"

#include <any>
#include <charconv> // for std::from_chars
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
			ROBOTICK_FATAL_EXIT("Missing config value");
		return std::any_cast<T&>(val);
	}

	template <typename T> struct type_identity
	{
		using type = T;
	};

	template <typename Func> inline bool dispatch_fixed_string(const TypeId& type, Func&& fn)
	{
		if (type == GET_TYPE_ID(FixedString8))
			return fn(type_identity<FixedString8>{});
		if (type == GET_TYPE_ID(FixedString16))
			return fn(type_identity<FixedString16>{});
		if (type == GET_TYPE_ID(FixedString32))
			return fn(type_identity<FixedString32>{});
		if (type == GET_TYPE_ID(FixedString64))
			return fn(type_identity<FixedString64>{});
		if (type == GET_TYPE_ID(FixedString128))
			return fn(type_identity<FixedString128>{});
		if (type == GET_TYPE_ID(FixedString256))
			return fn(type_identity<FixedString256>{});
		if (type == GET_TYPE_ID(FixedString512))
			return fn(type_identity<FixedString512>{});
		if (type == GET_TYPE_ID(FixedString1024))
			return fn(type_identity<FixedString1024>{});
		return false;
	}

	inline void apply_struct_fields(void* struct_ptr, const StructRegistryEntry& info, const std::map<std::string, std::string>& config)
	{
		for (const auto& field : info.fields)
		{
			auto it = config.find(field.name);
			if (it == config.end())
				continue;

			ROBOTICK_ASSERT(field.offset_within_struct != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

			void* field_ptr = static_cast<uint8_t*>(struct_ptr) + field.offset_within_struct;
			const std::string& value = it->second;

			if (field.type == GET_TYPE_ID(std::string))
			{
				*static_cast<std::string*>(field_ptr) = value;
			}
			else if (field.type == GET_TYPE_ID(double))
			{
				double parsed = 0.0;
				auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
				if (result.ec != std::errc() || result.ptr != value.data() + value.size())
				{
					ROBOTICK_FATAL_EXIT("Failed to parse double config value for field: %s (value: %s)", field.name.c_str(), value.c_str());
				}
				*static_cast<double*>(field_ptr) = parsed;
			}
			else if (field.type == GET_TYPE_ID(int))
			{
				int parsed = 0;
				auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
				if (result.ec != std::errc() || result.ptr != value.data() + value.size())
				{
					ROBOTICK_FATAL_EXIT("Failed to parse int config value for field: %s (value: %s)", field.name.c_str(), value.c_str());
				}
				*static_cast<int*>(field_ptr) = parsed;
			}
			else if (dispatch_fixed_string(field.type,
						 [&](auto T) -> bool
						 {
							 using FS = typename decltype(T)::type;
							 *static_cast<FS*>(field_ptr) = value.c_str();
							 return true;
						 }))
			{
				continue;
			}
			else
			{
				ROBOTICK_FATAL_EXIT("Unsupported config type for field: %s", field.name.c_str());
			}
		}
	}

} // namespace robotick
