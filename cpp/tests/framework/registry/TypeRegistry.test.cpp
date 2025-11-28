// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/registry/TypeRegistry.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	TEST_CASE("Unit/Framework/Registry/TypeRegistryLifecycle")
	{
		auto& registry = TypeRegistry::get();

		SECTION("finds existing primitive type (sanity)")
		{
			const TypeDescriptor* found = registry.find_by_name("int");
			REQUIRE(found != nullptr);
			CHECK(found->size == sizeof(int));
			CHECK(found->alignment == alignof(int));
		}
	}

} // namespace robotick::test
