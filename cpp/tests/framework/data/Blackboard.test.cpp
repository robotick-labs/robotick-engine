// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/Buffer.h"

#include <catch2/catch_all.hpp>
#include <cstring>
#include <typeindex>

using namespace robotick;

TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard basic construction and memory layout", "[blackboard]")
{
	std::vector<BlackboardField> schema = {
		{"age", std::type_index(typeid(int))}, {"score", std::type_index(typeid(double))}, {"name", std::type_index(typeid(FixedString64))}};

	Blackboard blackboard(schema);

	REQUIRE(blackboard.get_schema().size() == 3);
	REQUIRE(blackboard.required_size() >= sizeof(int) + sizeof(double) + sizeof(FixedString64));

	REQUIRE(blackboard.has("age"));
	REQUIRE(blackboard.has("score"));
	REQUIRE(blackboard.has("name"));
	REQUIRE_FALSE(blackboard.has("missing"));
}

TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard binds to BlackboardsBuffer and performs typed access", "[blackboard][buffer]")
{
	std::vector<BlackboardField> schema = {
		{"age", std::type_index(typeid(int))}, {"score", std::type_index(typeid(double))}, {"name", std::type_index(typeid(FixedString64))}};

	Blackboard blackboard(schema);
	size_t size = blackboard.required_size();

	BlackboardsBuffer temp_buffer(size);
	BlackboardsBuffer::set_source(&temp_buffer);

	blackboard.bind(0);

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

TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard throws on missing keys or unbound source", "[blackboard][errors]")
{
	std::vector<BlackboardField> schema = {{"alpha", std::type_index(typeid(int))}};
	Blackboard blackboard(schema);

	SECTION("Throws if no source buffer set")
	{
		BlackboardsBuffer::set_source(nullptr);
		REQUIRE_THROWS_WITH(blackboard.get<int>("alpha"), Catch::Matchers::ContainsSubstring("Blackboard"));
	}

	SECTION("Throws on unbound field offset")
	{
		BlackboardsBuffer temp_buffer(blackboard.required_size());
		BlackboardsBuffer::set_source(&temp_buffer);
		REQUIRE_THROWS_WITH(blackboard.get<int>("alpha"), Catch::Matchers::ContainsSubstring("Blackboard"));
	}

	SECTION("Throws on missing field")
	{
		BlackboardsBuffer temp(blackboard.required_size());
		BlackboardsBuffer::set_source(&temp);
		blackboard.bind(0);
		REQUIRE_THROWS_WITH(blackboard.get<int>("nonexistent"), Catch::Matchers::ContainsSubstring("Blackboard"));
	}
}

TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard alignment and offset consistency", "[blackboard][layout]")
{
	std::vector<BlackboardField> schema = {
		{"a", std::type_index(typeid(int))}, {"b", std::type_index(typeid(double))}, {"c", std::type_index(typeid(int))}};

	Blackboard blackboard(schema);
	size_t size = blackboard.required_size();

	BlackboardsBuffer temp_buffer(size);
	BlackboardsBuffer::set_source(&temp_buffer);

	blackboard.bind(0);

	blackboard.set<int>("a", 1);
	blackboard.set<double>("b", 3.14);
	blackboard.set<int>("c", 7);

	REQUIRE(blackboard.get<int>("a") == 1);
	REQUIRE(blackboard.get<double>("b") == Catch::Approx(3.14));
	REQUIRE(blackboard.get<int>("c") == 7);
}
