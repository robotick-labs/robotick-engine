// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"

#include <catch2/catch_all.hpp>
#include <cstring>
#include <typeindex>

namespace robotick
{
	struct BlackboardTestUtils
	{
		static std::pair<WorkloadsBuffer, Blackboard*> make_buffer_and_embedded_blackboard(const std::vector<BlackboardFieldInfo>& schema)
		{
			// create a temp-blackboard on the stack to find out how much data-block space the scheme needs
			Blackboard temp(schema);
			size_t total_size = sizeof(Blackboard) + temp.get_info()->total_datablock_size;

			// create a sufficiently large WorkloadsBuffer for actual Blackboard and its data
			WorkloadsBuffer buffer(total_size);

			// create the blackboard:
			auto* blackboard_ptr = buffer.as<Blackboard>(0);
			new (blackboard_ptr) Blackboard(schema);
			blackboard_ptr->bind(sizeof(Blackboard));
			return {std::move(buffer), blackboard_ptr};
		}

		static const BlackboardInfo& get_info(Blackboard& blackboard) { return *blackboard.get_info(); }
	};

	TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard basic construction and memory layout", "[blackboard]")
	{
		std::vector<BlackboardFieldInfo> schema = {
			{"age", std::type_index(typeid(int))}, {"score", std::type_index(typeid(double))}, {"name", std::type_index(typeid(FixedString64))}};

		auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(schema);

		REQUIRE(BlackboardTestUtils::get_info(*blackboard).schema.size() == 3);
		REQUIRE(BlackboardTestUtils::get_info(*blackboard).total_datablock_size >= sizeof(int) + sizeof(double) + sizeof(FixedString64));

		REQUIRE(BlackboardTestUtils::get_info(*blackboard).has_field("age"));
		REQUIRE(BlackboardTestUtils::get_info(*blackboard).has_field("score"));
		REQUIRE(BlackboardTestUtils::get_info(*blackboard).has_field("name"));
		REQUIRE_FALSE(BlackboardTestUtils::get_info(*blackboard).has_field("missing"));
	}

	TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard binds to WorkloadsBuffer and performs typed access", "[blackboard][buffer]")
	{
		std::vector<BlackboardFieldInfo> schema = {
			{"age", std::type_index(typeid(int))}, {"score", std::type_index(typeid(double))}, {"name", std::type_index(typeid(FixedString64))}};

		auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(schema);

		SECTION("Set and get int")
		{
			blackboard->set<int>("age", 42);
			REQUIRE(blackboard->get<int>("age") == 42);
		}

		SECTION("Set and get double")
		{
			blackboard->set<double>("score", 98.5);
			REQUIRE(blackboard->get<double>("score") == Catch::Approx(98.5));
		}

		SECTION("Set and get FixedString64")
		{
			FixedString64 name = "Magg.e";
			blackboard->set<FixedString64>("name", name);
			REQUIRE(std::string(blackboard->get<FixedString64>("name").c_str()) == "Magg.e");
		}
	}

	TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard throws on missing keys or unbound source", "[blackboard][errors]")
	{
		std::vector<BlackboardFieldInfo> schema = {{"alpha", std::type_index(typeid(int))}};

		SECTION("Throws on unbound field offset")
		{
			Blackboard temp(schema);
			WorkloadsBuffer buffer(sizeof(Blackboard) + BlackboardTestUtils::get_info(temp).total_datablock_size);

			auto* blackboard_ptr = buffer.as<Blackboard>(0);
			new (blackboard_ptr) Blackboard(schema); // not bound
			REQUIRE_THROWS_WITH(blackboard_ptr->get<int>("alpha"), Catch::Matchers::ContainsSubstring("Blackboard"));
		}

		SECTION("Throws on missing field")
		{
			auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(schema);

			REQUIRE_THROWS_WITH(blackboard->get<int>("nonexistent"), Catch::Matchers::ContainsSubstring("Blackboard"));
		}
	}

	TEST_CASE("Unit|Framework|Data|Blackboard|Blackboard alignment and offset consistency", "[blackboard][layout]")
	{
		std::vector<BlackboardFieldInfo> schema = {
			{"a", std::type_index(typeid(int))}, {"b", std::type_index(typeid(double))}, {"c", std::type_index(typeid(int))}};

		auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(schema);

		blackboard->set<int>("a", 1);
		blackboard->set<double>("b", 3.14);
		blackboard->set<int>("c", 7);

		REQUIRE(blackboard->get<int>("a") == 1);
		REQUIRE(blackboard->get<double>("b") == Catch::Approx(3.14));
		REQUIRE(blackboard->get<int>("c") == 7);
	}

} // namespace robotick
