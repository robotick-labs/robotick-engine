// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeRegistry.h"

#include "robotick/api_base.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"

namespace robotick
{
	namespace
	{
		Thread::ThreadId s_registration_thread = 0;
		bool s_registration_thread_initialized = false;
		AtomicFlag s_registry_sealed{false};
	} // namespace

	TypeRegistry& TypeRegistry::get()
	{
		static TypeRegistry instance;
		return instance;
	}

	void TypeRegistry::seal()
	{
		s_registry_sealed.set(true);
	}

	bool TypeRegistry::is_sealed() const
	{
		return s_registry_sealed.is_set();
	}

	void TypeRegistry::register_type(const TypeDescriptor& type)
	{
		ROBOTICK_ASSERT_MSG(!s_registry_sealed.is_set(),
			"TypeRegistry::register_type() - registry has been sealed and cannot accept new types (ensure all registrations happen during startup)");

		if (!s_registration_thread_initialized)
		{
			s_registration_thread = Thread::get_current_thread_id();
			s_registration_thread_initialized = true;
		}

		ROBOTICK_ASSERT_MSG(Thread::get_current_thread_id() == s_registration_thread,
			"TypeRegistry::register_type() - registration must occur on the same thread that performed initial registration");

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
