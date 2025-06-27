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
		const TypeDescriptor** existing_type = types_by_id.find(type.id);
		if (existing_type != nullptr)
		{
			ROBOTICK_FATAL_EXIT("TypeRegistry::register_type() - cannot have multiple types with same id '%u' (names new vs existing: '%s'|'%s' )",
				(unsigned int)type.id.value,
#ifdef ROBOTICK_DEBUG_TYPEID_NAMES
				type.id.name, (*existing_type)->id.name);
#else
				type.name.c_str(), (*existing_type)->name.c_str());
#endif // #ifdef ROBOTICK_DEBUG_TYPEID_NAMES
		}

		types.push_back(&type);
		types_by_id.insert(type.id, &type);
	}

	const TypeDescriptor* TypeRegistry::find_by_id(const TypeId& id)
	{
		const TypeDescriptor** found_type = types_by_id.find(id);
		return found_type ? *found_type : nullptr;
	}

	const TypeDescriptor* TypeRegistry::find_by_name(const char* name)
	{
		const TypeId query_id(name);
		return find_by_id(query_id);
	}

	size_t TypeRegistry::get_registered_count()
	{
		return types.size();
	}

} // namespace robotick
