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
		entries[entry.name] = &entry;
	}

	const WorkloadRegistryEntry* WorkloadRegistry::find(const std::string& name) const
	{
		auto it = entries.find(name);
		return it != entries.end() ? it->second : nullptr;
	}

} // namespace robotick
