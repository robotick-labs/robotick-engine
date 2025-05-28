// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"

#include <catch2/catch_all.hpp>
#include <cstring>

using namespace robotick;

TEST_CASE("Blackboard basic construction and memory layout", "[blackboard]")
{
	std::vector<BlackboardField> schema = {
		{"age", BlackboardFieldType::Int}, {"score", BlackboardFieldType::Double}, {"name", BlackboardFieldType::FixedString64}};

	Blackboard blackboard(schema);

	REQUIRE(blackboard.get_schema().size() == 3);
	REQUIRE(blackboard.required_size() >= sizeof(int) + sizeof(double) + sizeof(FixedString64));

	// Ensure `has` reports correctly
	REQUIRE(blackboard.has("age"));
	REQUIRE(blackboard.has("score"));
	REQUIRE(blackboard.has("name"));
	REQUIRE_FALSE(blackboard.has("missing"));
}

TEST_CASE("Blackboard binds external memory and performs typed access", "[blackboard]")
{
	std::vector<BlackboardField> schema = {
		{"age", BlackboardFieldType::Int}, {"score", BlackboardFieldType::Double}, {"name", BlackboardFieldType::FixedString64}};

	Blackboard blackboard(schema);
	size_t buffer_size = blackboard.required_size();

	std::vector<uint8_t> memory(buffer_size, 0);
	blackboard.bind(memory.data());

	SECTION("Set and get int")
	{
		blackboard.set<int>("age", 42);
		REQUIRE(blackboard.get<int>("age") == 42);
	}

	SECTION("Set and get double")
	{
		blackboard.set<double>("score", 98.5);
		REQUIRE(blackboard.get<double>("score") == Catch::Approx(98.5));
	}

	SECTION("Set and get FixedString64")
	{
		FixedString64 name = "Magg.e";
		blackboard.set<FixedString64>("name", name);
		REQUIRE(std::string(blackboard.get<FixedString64>("name").c_str()) == "Magg.e");
	}
}

TEST_CASE("Blackboard throws on missing keys or unbound memory", "[blackboard][errors]")
{
	std::vector<BlackboardField> schema = {{"alpha", BlackboardFieldType::Int}};
	Blackboard blackboard(schema);
	std::vector<uint8_t> memory(blackboard.required_size(), 0);

	SECTION("Access before bind throws")
	{
		try
		{
			blackboard.set<int>("alpha", 1);
			FAIL("Expected exception not thrown");
		}
		catch (const std::runtime_error& e)
		{
			REQUIRE(std::string(e.what()).find("get_ptr") != std::string::npos);
		}

		try
		{
			blackboard.get<int>("alpha");
			FAIL("Expected exception not thrown");
		}
		catch (const std::runtime_error& e)
		{
			REQUIRE(std::string(e.what()).find("get_ptr") != std::string::npos);
		}
	}

	blackboard.bind(memory.data());

	SECTION("Access after bind succeeds")
	{
		REQUIRE_NOTHROW(blackboard.set<int>("alpha", 123));
		REQUIRE(blackboard.get<int>("alpha") == 123);
	}

	SECTION("Access unknown field still throws")
	{
		try
		{
			blackboard.get<int>("nonexistent");
			FAIL("Expected exception not thrown");
		}
		catch (const std::runtime_error& e)
		{
			REQUIRE(std::string(e.what()).find("get_ptr") != std::string::npos);
		}
	}
}

TEST_CASE("Blackboard handles alignment and offset consistency", "[blackboard][layout]")
{
	std::vector<BlackboardField> schema = {{"a", BlackboardFieldType::Int}, {"b", BlackboardFieldType::Double}, {"c", BlackboardFieldType::Int}};

	Blackboard blackboard(schema);
	auto schema_ref = blackboard.get_schema();

	REQUIRE(blackboard.has("a"));
	REQUIRE(blackboard.has("b"));
	REQUIRE(blackboard.has("c"));

	std::vector<uint8_t> memory(blackboard.required_size(), 0);
	blackboard.bind(memory.data());

	// Set distinct values and validate memory independence
	blackboard.set<int>("a", 1);
	blackboard.set<double>("b", 3.14);
	blackboard.set<int>("c", 7);

	REQUIRE(blackboard.get<int>("a") == 1);
	REQUIRE(blackboard.get<double>("b") == Catch::Approx(3.14));
	REQUIRE(blackboard.get<int>("c") == 7);
}
