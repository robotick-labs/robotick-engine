// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/HeapVector.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	TEST_CASE("Unit/Framework/Common/HeapVector")
	{
		SECTION("Basic operations")
		{
			HeapVector<int> vec;
			vec.initialize(3);
			vec[0] = 10;
			vec[1] = 20;
			vec[2] = 30;

			REQUIRE(vec.size() == 3);
			CHECK(vec[0] == 10);
			CHECK(vec[1] == 20);
			CHECK(vec[2] == 30);
		}

		SECTION("Copy constructor copies values")
		{
			HeapVector<int> a;
			a.initialize(2);
			a[0] = 7;
			a[1] = 42;

			HeapVector<int> b = a;
			REQUIRE(b.size() == 2);
			CHECK(b[0] == 7);
			CHECK(b[1] == 42);
		}

		SECTION("Move constructor transfers ownership")
		{
			HeapVector<int> a;
			a.initialize(2);
			a[0] = 100;
			a[1] = 200;

			HeapVector<int> b = std::move(a);
			REQUIRE(b.size() == 2);
			CHECK(b[0] == 100);
			CHECK(b[1] == 200);
			CHECK(a.size() == 0);
		}

		SECTION("Assignment is blocked after init")
		{
			HeapVector<int> a;
			a.initialize(1);
			a[0] = 123;

			HeapVector<int> b;
			b.initialize(1);

			ROBOTICK_REQUIRE_ERROR(b = a);
		}

		SECTION("Out of bounds access triggers error")
		{
			HeapVector<int> vec;
			vec.initialize(2);
			ROBOTICK_REQUIRE_ERROR(vec[2]);
			ROBOTICK_REQUIRE_ERROR(vec[999]);
		}
	}

} // namespace robotick::test
