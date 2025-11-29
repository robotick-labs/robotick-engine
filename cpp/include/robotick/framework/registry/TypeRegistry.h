// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/containers/List.h"
#include "robotick/framework/containers/Map.h"
#include "robotick/framework/utils/TypeId.h"

#include <stddef.h>

namespace robotick
{
	struct TypeDescriptor;

	using TypeDescriptors = List<const TypeDescriptor*>;

	// NOTE: TypeRegistry is a process-wide singleton intended to be populated exactly once,
	// from a single thread, during program startup (typically via the registration macros).
	// After the executable begins running, the registry is treated as immutable - no runtime
	// registration is allowed. Any attempt to register types outside of the single-threaded
	// startup path is a bug and should trip the corresponding assertions in the implementation.

	class TypeRegistry
	{
	  public:
		static TypeRegistry& get();

		void seal(); // mark the registry as immutable (no further registrations)
		bool is_sealed() const;

		void register_type(const TypeDescriptor& desc);

		const TypeDescriptor* find_by_id(const TypeId& id);
		const TypeDescriptor* find_by_name(const char* name);

		const TypeDescriptors& get_registered_types() const { return types; };
		size_t get_registered_count();

	  private:
		TypeDescriptors types;
		Map<TypeId, const TypeDescriptor*> types_by_id;
	};

} // namespace robotick
