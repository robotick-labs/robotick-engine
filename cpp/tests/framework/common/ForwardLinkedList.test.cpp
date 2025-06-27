// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/ForwardLinkedList.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	struct TestItem
	{
		int value = 0;
		TestItem* next_entry = nullptr;

		explicit TestItem(int v) : value(v) {}
	};

	TEST_CASE("Unit/Framework/Common/ForwardLinkedList")
	{

		SECTION("Basic insertion and iteration")
		{
			ForwardLinkedList<TestItem> list;

			TestItem a(1), b(2), c(3);
			list.add(a);
			list.add(b);
			list.add(c);

			int sum = 0;
			for (TestItem& item : list)
			{
				sum += item.value;
			}

			// Note: Insertion order is LIFO, but sum is unaffected
			CHECK(sum == 6);
		}

		SECTION("Empty list reports empty() and iterates zero times")
		{
			ForwardLinkedList<TestItem> list;
			CHECK(list.empty());

			int count = 0;
			for (TestItem& item : list)
			{
				(void)item;
				++count;
			}
			CHECK(count == 0);
		}

		SECTION("front returns first added item")
		{
			ForwardLinkedList<TestItem> list;

			TestItem a(100);
			list.add(a);

			REQUIRE(list.front() == &a);
			CHECK(list.front()->value == 100);
		}

		SECTION("Multiple adds produce correct head")
		{
			ForwardLinkedList<TestItem> list;

			TestItem a(1), b(2), c(3);
			list.add(a); // head -> a
			list.add(b); // head -> b -> a
			list.add(c); // head -> c -> b -> a

			REQUIRE(list.front() == &c);
			CHECK(list.front()->next_entry == &b);
			CHECK(list.front()->next_entry->next_entry == &a);
		}
	}
} // namespace robotick::test
