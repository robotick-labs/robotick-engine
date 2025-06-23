// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/common/FixedString.h"
#include <catch2/catch_all.hpp>

namespace robotick::test
{
	TEST_CASE("Unit/Framework/Common/FixedString/Construct and compare")
	{
		FixedString32 a("hello");
		FixedString32 b("hello");
		FixedString32 c("world");

		CHECK(a == b);
		CHECK(a != c);
		CHECK(!(a < b));
		CHECK(a < c);
		CHECK(strcmp(a.c_str(), "hello") == 0);
	}

	TEST_CASE("Unit/Framework/Common/FixedString/Assignment and truncation")
	{
		FixedString8 s;
		s = "toolongname"; // will truncate
		CHECK(s.length() == 7);
		CHECK(s == "toolong");
	}

	TEST_CASE("Unit/Framework/Common/FixedString/Empty and length")
	{
		FixedString64 s;
		CHECK(s.empty());

		s = "abc";
		CHECK(!s.empty());
		CHECK(s.length() == 3);
	}

	TEST_CASE("Unit/Framework/Common/FixedString/Hash consistent for equal strings")
	{
		FixedString32 a("matchme");
		FixedString32 b("matchme");

		CHECK(hash(a) == hash(b));
	}
} // namespace robotick::test
