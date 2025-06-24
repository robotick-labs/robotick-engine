// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/List.h"
#include "robotick/framework/utils/TypeId.h"

#include <stddef.h>

namespace robotick
{
	struct TypeDescriptor;

	using TypeDescriptors = List<const TypeDescriptor*>;

	class TypeRegistry
	{
	  public:
		static TypeRegistry& get();

		void register_type(const TypeDescriptor& desc);

		const TypeDescriptor* find_by_id(const TypeId& id);
		const TypeDescriptor* find_by_name(const char* name);

		const TypeDescriptors& get_registered_types() const { return types; };
		size_t get_registered_count();

	  private:
		TypeDescriptors types;
	};

} // namespace robotick
