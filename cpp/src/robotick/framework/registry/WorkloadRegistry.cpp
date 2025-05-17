#include "robotick/framework/registry/WorkloadRegistry.h"

#include <map>	  // for std::map
#include <string> // for std::string

namespace robotick
{

	std::map<std::string, const WorkloadRegistryEntry *> &get_mutable_registry()
	{
		static std::map<std::string, const WorkloadRegistryEntry *> registry;
		return registry;
	}

	void register_workload_entry(const WorkloadRegistryEntry &entry)
	{
		get_mutable_registry()[entry.name] = &entry;
	}

	const WorkloadRegistryEntry *get_workload_registry_entry(const std::string &name)
	{
		auto &map = get_mutable_registry();
		auto it = map.find(name);
		return it != map.end() ? it->second : nullptr;
	}

} // namespace robotick
