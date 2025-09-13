// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeRegistry.h"

#include "robotick/api_base.h"
#include "robotick/framework/registry/TypeDescriptor.h"

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
#ifdef ROBOTICK_DEBUG_TYPEID_NAMES
			ROBOTICK_FATAL_EXIT("TypeRegistry::register_type() - cannot have multiple types with same id '%u' (names new vs existing: '%s'|'%s' )",
				(unsigned int)type.id.value,
				type.id.name,
				(*existing_type)->id.name);
#else
			ROBOTICK_FATAL_EXIT("TypeRegistry::register_type() - cannot have multiple types with same id '%u' (names new vs existing: '%s'|'%s' )",
				(unsigned int)type.id.value,
				type.name.c_str(),
				(*existing_type)->name.c_str());
#endif
		}

		types.push_back(&type);
		types_by_id.insert(type.id, &type);
	}

	const TypeDescriptor* TypeRegistry::find_by_id(const TypeId& id)
	{
		ROBOTICK_ASSERT(types_by_id.size() == types.size());

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
