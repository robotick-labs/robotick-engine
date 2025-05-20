#include "robotick/framework/registry/WorkloadRegistry.h"

#include <map>     // for std::map
#include <string>  // for std::string

namespace robotick
{
    std::map<std::string, const WorkloadRegistryEntry*>& WorkloadRegistry::registry()
    {
        static std::map<std::string, const WorkloadRegistryEntry*> instance;
        return instance;
    }

    void WorkloadRegistry::register_entry(const WorkloadRegistryEntry& entry)
    {
        registry()[entry.name] = &entry;
    }

    const WorkloadRegistryEntry* WorkloadRegistry::find(const std::string& name) const
    {
        auto it = registry().find(name);
        return it != registry().end() ? it->second : nullptr;
    }
}  // namespace robotick
