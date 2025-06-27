// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/Map.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	TEST_CASE("Unit/Framework/Common/Map")
	{
		Map<const char*, int, 8> map;

		SECTION("Insert and find basic keys")
		{
			map.insert("a", 1);
			map.insert("b", 2);
			map.insert("c", 3);

			CHECK(map.find("a"));
			CHECK(map.find("b"));
			CHECK(map.find("c"));
			CHECK(*map.find("a") == 1);
			CHECK(*map.find("b") == 2);
			CHECK(*map.find("c") == 3);
		}

		SECTION("Update existing key")
		{
			map.insert("x", 10);
			map.insert("x", 42); // overwrite
			REQUIRE(map.find("x"));
			CHECK(*map.find("x") == 42);
		}

		SECTION("Missing keys return nullptr")
		{
			map.insert("key", 123);
			CHECK(map.find("notfound") == nullptr);
		}
	}
} // namespace robotick::test
