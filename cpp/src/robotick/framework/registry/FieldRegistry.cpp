// Copyright 2025 Robotick Labs
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "robotick/framework/registry/FieldRegistry.h"

namespace robotick
{

	static std::unordered_map<std::string, StructRegistryEntry> struct_registry;

	const StructRegistryEntry* register_struct(const std::string& name, size_t struct_size, std::vector<FieldInfo> fields)
	{
		auto& entry = struct_registry[name];
		entry.name = name;
		entry.size = struct_size;
		entry.fields = std::move(fields);
		return &entry;
	}

	const StructRegistryEntry* get_struct(const std::string& name)
	{
		auto it = struct_registry.find(name);
		if (it != struct_registry.end())
			return &it->second;
		return nullptr;
	}

} // namespace robotick
