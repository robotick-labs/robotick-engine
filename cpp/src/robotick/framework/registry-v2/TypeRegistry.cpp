// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry-v2/TypeRegistry.h"

#include "robotick/api_base.h"
#include "robotick/framework/registry-v2/TypeDescriptor.h"

#include <string.h> // for strcmp

namespace robotick
{
	TypeRegistry& TypeRegistry::get()
	{
		static TypeRegistry instance;
		return instance;
	}

	void TypeRegistry::register_type(const TypeDescriptor& type)
	{
		// Prevent duplicate ID or name registration
		for (const TypeDescriptor* existing_type : types)
		{
			if (existing_type->name == type.name)
			{
				ROBOTICK_FATAL_EXIT("TypeRegistry::register_type() - cannot have multiple types with same name '%s'", type.name.c_str());
			}
			else if (existing_type->id == type.id)
			{
				ROBOTICK_FATAL_EXIT(
					"TypeRegistry::register_type() - cannot have multiple types with same id '%d' (names new vs existing: '%s'|'%s' )", type.id.value,
#ifdef ROBOTICK_DEBUG_TYPEID_NAMES
					type.id.name, existing_type->id.name);
#else
					type.name, existing_type->name);
#endif // #ifdef ROBOTICK_DEBUG_TYPEID_NAMES
			}
		}

		types.push_back(&type);
	}

	const TypeDescriptor* TypeRegistry::find_by_id(const TypeId& id)
	{
		for (const TypeDescriptor* type : types)
		{
			if (type->id == id)
				return type;
		}
		return nullptr;
	}

	const TypeDescriptor* TypeRegistry::find_by_name(const char* name)
	{
		const TypeId query_id(name);

		for (const TypeDescriptor* type : types)
		{
			if (type->id == query_id)
			{
				ROBOTICK_ASSERT_MSG(type->name == name, "If id matches then so should name as id is created from name");
				return type;
			}
		}
		return nullptr;
	}

	size_t TypeRegistry::get_registered_count()
	{
		return types.size();
	}

} // namespace robotick
