// Copyright Robotick Labs
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "robotick/api.h"

#include <memory>
#include <stdexcept>

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

		ROBOTICK_INFO("WorkloadRegistry: registering workload '%s'...", entry.name.c_str());

		if (entries.find(entry.name) != entries.end())
		{
			ROBOTICK_ERROR("WorkloadRegistry: entry with name '%s' already exists.", entry.name.c_str());
		}

		entries[entry.name] = std::make_unique<WorkloadRegistryEntry>(entry);
	}

	const WorkloadRegistryEntry* WorkloadRegistry::find(const char* name) const
	{
		std::scoped_lock lock(mutex);
		auto it_entries = entries.find(FixedString64(name));
		return it_entries != entries.end() ? it_entries->second.get() : nullptr;
	}

} // namespace robotick
