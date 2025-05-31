// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/WorkloadsBuffer.h"

#include <catch2/catch_all.hpp>
#include <cstring>

using namespace robotick;

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer supports basic access and bounds checking", "[buffer]")
{
	RawBuffer buffer(32);
	REQUIRE(buffer.get_size() == 32);

	SECTION("Read and write via typed pointer")
	{
		auto* iptr = buffer.as<int>(0);
		*iptr = 42;
		REQUIRE(*buffer.as<int>(0) == 42);
	}

	SECTION("Out-of-bounds access throws")
	{
		REQUIRE_THROWS_AS(buffer.as<int>(32), std::out_of_range);
	}
}

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer clones data correctly", "[buffer][clone]")
{
	RawBuffer original(64);
	std::memset(original.raw_ptr(), 0xAB, 64);

	RawBuffer clone;
	clone.create_mirror_from(original);

	REQUIRE(clone.get_size() == 64);
	REQUIRE(std::memcmp(clone.raw_ptr(), original.raw_ptr(), 64) == 0);

	original.raw_ptr()[0] = 0xCD;
	REQUIRE(clone.raw_ptr()[0] == static_cast<uint8_t>(0xAB));
}

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer update_mirror_from validates size and performs copy", "[buffer][mirror]")
{
	RawBuffer a(16);
	RawBuffer b(16);
	std::memset(b.raw_ptr(), 0x66, 16);

	a.update_mirror_from(b);
	REQUIRE(std::memcmp(a.raw_ptr(), b.raw_ptr(), 16) == 0);

	RawBuffer c(32);
	REQUIRE_THROWS_WITH(a.update_mirror_from(c), Catch::Matchers::ContainsSubstring("size mismatch"));
}

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer prevents duplicate create_mirror_from", "[buffer][mirror][lifecycle]")
{
	RawBuffer source(8);
	RawBuffer mirror;
	mirror.create_mirror_from(source);

	REQUIRE_THROWS_WITH(mirror.create_mirror_from(source), Catch::Matchers::ContainsSubstring("already allocated"));
}

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer throws if update_mirror_from used before allocation", "[buffer][mirror][invalid]")
{
	RawBuffer source(8);
	RawBuffer mirror; // not allocated

	REQUIRE_THROWS_WITH(mirror.update_mirror_from(source), Catch::Matchers::ContainsSubstring("not initialized"));
}

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer contains_object checks bounds", "[buffer][bounds]")
{
	RawBuffer buffer(32);
	uint8_t* base = buffer.raw_ptr();

	REQUIRE(buffer.contains_object(base, 0));			 // zero-length is valid
	REQUIRE(buffer.contains_object(base, 32));			 // entire buffer
	REQUIRE_FALSE(buffer.contains_object(base + 1, 32)); // extends past end
	REQUIRE_FALSE(buffer.contains_object(base + 32, 1)); // out of range
}

TEST_CASE("Unit|Framework|Data|Buffer|RawBuffer handles alignment correctly in as<T>()", "[buffer][align]")
{
	struct Aligned
	{
		int a;
		double b;
	};

	constexpr size_t offset = 0;
	constexpr size_t alignment = alignof(Aligned);
	constexpr size_t raw_size = sizeof(Aligned);
	constexpr size_t size = (raw_size + alignment - 1) & ~(alignment - 1); // align up

	RawBuffer buffer(size);
	REQUIRE_NOTHROW(buffer.as<Aligned>(offset));

	SECTION("Misaligned offset throws invalid_argument")
	{
		if (alignment > 1)
		{
			RawBuffer bad_buffer(size + 1);

			try
			{
				bad_buffer.as<Aligned>(1);
				FAIL("Expected std::invalid_argument to be thrown");
			}
			catch (const std::invalid_argument& ex)
			{
				REQUIRE_THAT(std::string{ex.what()}, Catch::Matchers::ContainsSubstring("Offset is not properly aligned"));
			}
		}
	}
}
