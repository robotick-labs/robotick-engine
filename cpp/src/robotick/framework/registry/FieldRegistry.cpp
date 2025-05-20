#include "robotick/framework/registry/FieldRegistry.h"

namespace robotick
{

	static std::unordered_map<std::string, StructRegistryEntry> struct_registry;

	const StructRegistryEntry *register_struct(const std::string &name, size_t struct_size,
											   std::vector<FieldInfo> fields)
	{
		auto &entry = struct_registry[name];
		entry.name = name;
		entry.size = struct_size;
		entry.fields = std::move(fields);
		return &entry;
	}

	const StructRegistryEntry *get_struct(const std::string &name)
	{
		auto it = struct_registry.find(name);
		if (it != struct_registry.end())
			return &it->second;
		return nullptr;
	}

} // namespace robotick
