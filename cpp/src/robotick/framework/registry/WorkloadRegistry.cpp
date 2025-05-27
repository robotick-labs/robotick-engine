// Copyright Robotick Labs
#include "robotick/framework/registry/WorkloadRegistry.h"

namespace robotick
{
	WorkloadRegistry& WorkloadRegistry::get()
	{
		static WorkloadRegistry instance;
		return instance;
	}

	void WorkloadRegistry::register_entry(const WorkloadRegistryEntry& entry)
	{
		std::scoped_lock lock(mutex);
		entries[entry.name] = &entry;
	}

	const WorkloadRegistryEntry* WorkloadRegistry::find(const std::string& name) const
	{
		std::scoped_lock lock(mutex);
		auto it_entries = entries.find(name);
		return it_entries != entries.end() ? it_entries->second : nullptr;
	}

} // namespace robotick
