// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0
#include "robotick/framework/registry/FieldRegistry.h"

namespace robotick
{
	FieldRegistry& FieldRegistry::get()
	{
		static FieldRegistry instance;
		return instance;
	}
	const StructRegistryEntry* FieldRegistry::register_struct(
		const std::string& name, size_t size, const std::type_index& type, size_t offset, std::vector<FieldInfo> fields)
	{
		std::lock_guard<std::mutex> lock(mutex);

		auto& entry = entries[name];

		entry.name = name;
		entry.size = size;
		entry.offset = std::max(offset, entry.offset); // one of the registrations doesn't know the offset - TODO - create a ticket to address the
													   // disparity - 2 sets of registrations is a sign of a wrong pattern somewhere
		entry.type = type;

		if (entry.fields.empty() && !fields.empty())
		{
			entry.fields = std::move(fields); // first valid registration populates fields
		}
		// else: retain existing field definitions
		// (first registration call may actually come from Workload registration code, since static execution is
		// hard to predict.  So in that case we allow that to register an empty struct, and then populate the same
		// object with fields when "real" registration happens)

		return &entry;
	}

	const StructRegistryEntry* FieldRegistry::get_struct(const std::string& name) const
	{
		std::lock_guard<std::mutex> lock(mutex);

		auto it = entries.find(name);
		return it != entries.end() ? &it->second : nullptr;
	}
} // namespace robotick
