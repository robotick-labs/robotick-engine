// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry-v2/TypeMacros.h"

#include "robotick/api_base.h"
#include "robotick/framework/registry-v2/TypeDescriptor.h"
#include "robotick/framework/registry-v2/TypeRegistry.h"

namespace robotick
{
	AutoRegisterType::AutoRegisterType(const TypeDescriptor& desc)
	{
		TypeRegistry::get().register_type(desc);
	}

} // namespace robotick