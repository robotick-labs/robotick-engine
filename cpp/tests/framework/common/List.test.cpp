// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/List.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	TEST_CASE("Unit/Framework/Common/List")
	{
		SECTION("Basic insertion and iteration")
		{
			List<int> list;
			list.push_back(1);
			list.push_back(2);
			list.push_back(3);

			int sum = 0;
			for (int& v : list)
			{
				sum += v;
			}
			CHECK(sum == 6);
		}

		SECTION("Empty list reports empty() and iterates zero times")
		{
			List<int> list;
			CHECK(list.empty());

			int count = 0;
			for (int& v : list)
			{
				(void)v;
				count++;
			}
			CHECK(count == 0);
		}

		SECTION("clear deletes all elements")
		{
			List<int> list;
			list.push_back(10);
			list.push_back(20);
			CHECK_FALSE(list.empty());

			list.clear();
			CHECK(list.empty());
		}

		SECTION("Works with move-only types")
		{
			struct MoveOnly
			{
				int value;
				MoveOnly(int v) : value(v) {}
				MoveOnly(MoveOnly&&) = default;
				MoveOnly& operator=(MoveOnly&&) = default;
				MoveOnly(const MoveOnly&) = delete;
				MoveOnly& operator=(const MoveOnly&) = delete;
			};

			List<MoveOnly> list;
			list.push_back(MoveOnly(5));
			list.push_back(MoveOnly(10));

			int total = 0;
			for (auto& m : list)
			{
				total += m.value;
			}
			CHECK(total == 15);
		}
	}
} // namespace robotick::test