// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Buffer.h"

#include <catch2/catch_all.hpp>
#include <cstring>

using namespace robotick;

TEST_CASE("RawBuffer supports basic access and bounds checking", "[buffer]")
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

TEST_CASE("RawBuffer clones data correctly", "[buffer][clone]")
{
	RawBuffer original(64);
	std::memset(original.raw_ptr(), 0xAB, 64);

	RawBuffer clone = original;

	REQUIRE(clone.get_size() == 64);
	REQUIRE(std::memcmp(clone.raw_ptr(), original.raw_ptr(), 64) == 0);

	// Modify original, ensure clone is unchanged
	static_cast<char*>(original.raw_ptr())[0] = 0xCD;
	REQUIRE(static_cast<const char*>(clone.raw_ptr())[0] == static_cast<char>(0xAB));
}

TEST_CASE("RawBuffer mirror_from validates size and performs copy", "[buffer][sync]")
{
	RawBuffer a(16);
	RawBuffer b(16);
	std::memset(b.raw_ptr(), 0x66, 16);

	a.mirror_from(b);
	REQUIRE(std::memcmp(a.raw_ptr(), b.raw_ptr(), 16) == 0);

	RawBuffer c(32);
	REQUIRE_THROWS_WITH(a.mirror_from(c), Catch::Matchers::ContainsSubstring("size mismatch"));
}

TEST_CASE("BlackboardsBuffer syncs from source", "[blackboard][buffer]")
{
	static BlackboardsBuffer source(24);
	std::memset(source.raw_ptr(), 0x11, 24);

	BlackboardsBuffer::set_source(&source);

	BlackboardsBuffer& mirror = BlackboardsBuffer::get();
	mirror = BlackboardsBuffer(24); // reallocate thread-local buffer
	mirror.mirror_from_source();

	REQUIRE(mirror.get_size() == 24);
	REQUIRE(std::memcmp(mirror.raw_ptr(), source.raw_ptr(), 24) == 0);
}

TEST_CASE("WorkloadsBuffer syncs from source", "[workload][buffer]")
{
	static WorkloadsBuffer source(48);
	std::memset(source.raw_ptr(), 0x99, 48);

	WorkloadsBuffer::set_source(&source);

	WorkloadsBuffer& mirror = WorkloadsBuffer::get();
	mirror = WorkloadsBuffer(48); // reallocate thread-local buffer
	mirror.mirror_from_source();

	REQUIRE(mirror.get_size() == 48);
	REQUIRE(std::memcmp(mirror.raw_ptr(), source.raw_ptr(), 48) == 0);
}
