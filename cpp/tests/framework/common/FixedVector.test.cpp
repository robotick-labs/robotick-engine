// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedVector.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	TEST_CASE("Unit/Framework/Common/FixedVector")
	{
		FixedVector<int, 4> vec;

		SECTION("Initial state")
		{
			CHECK(vec.empty());
			CHECK(vec.size() == 0);
			CHECK_FALSE(vec.full());
		}

		SECTION("Add and index elements")
		{
			vec.add(5);
			vec.add(10);
			vec.add(15);

			CHECK(vec.size() == 3);
			CHECK(vec[0] == 5);
			CHECK(vec[1] == 10);
			CHECK(vec[2] == 15);
		}

		SECTION("Full detection - multiple adds")
		{
			CHECK(!vec.full());
			vec.add(1);
			CHECK(!vec.full());
			vec.add(2);
			CHECK(!vec.full());
			vec.add(3);
			CHECK(!vec.full());
			vec.add(4);
			CHECK(vec.full());
		}

		SECTION("Full detection - single fill() call")
		{
			CHECK(!vec.full());
			vec.fill();
			CHECK(vec.full());
		}

		SECTION("Clear vector")
		{
			vec.add(99);
			vec.clear();
			CHECK(vec.size() == 0);
			CHECK(vec.empty());
		}

		SECTION("Iteration over elements")
		{
			vec.add(1);
			vec.add(2);
			vec.add(3);

			int sum = 0;
			for (int v : vec)
			{
				sum += v;
			}
			CHECK(sum == 6);
		}
	}
} // namespace robotick::test
