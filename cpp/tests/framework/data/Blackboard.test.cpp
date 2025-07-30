// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/Blackboard.h"
#include "../utils/BlackboardTestUtils.h"
#include "robotick/api_base.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/utils/TypeId.h"

#include <catch2/catch_all.hpp>
#include <cstring>
#include <typeindex>

namespace robotick
{
	TEST_CASE("Unit/Framework/Data/Blackboard")
	{
		SECTION("Blackboard basic construction and memory layout", "[blackboard]")
		{
			HeapVector<FieldDescriptor> blackboard_fields;
			blackboard_fields.initialize(3);
			blackboard_fields[0] = FieldDescriptor{"age", GET_TYPE_ID(int)};
			blackboard_fields[1] = FieldDescriptor{"score", GET_TYPE_ID(double)};
			blackboard_fields[2] = FieldDescriptor{"name", GET_TYPE_ID(FixedString64)};

			auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(blackboard_fields);

			const auto& info = blackboard->get_info();

			REQUIRE(info.struct_descriptor.fields.size() == 3);
			REQUIRE(info.total_datablock_size >= sizeof(int) + sizeof(double) + sizeof(FixedString64));

			REQUIRE(info.has_field("age"));
			REQUIRE(info.has_field("score"));
			REQUIRE(info.has_field("name"));
			REQUIRE_FALSE(info.has_field("missing"));
		}

		SECTION("Blackboard binds to WorkloadsBuffer and performs typed access", "[blackboard][buffer]")
		{
			HeapVector<FieldDescriptor> blackboard_fields;
			blackboard_fields.initialize(3);
			blackboard_fields[0] = FieldDescriptor{"age", GET_TYPE_ID(int)};
			blackboard_fields[1] = FieldDescriptor{"score", GET_TYPE_ID(double)};
			blackboard_fields[2] = FieldDescriptor{"name", GET_TYPE_ID(FixedString64)};

			auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(blackboard_fields);

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

		SECTION("Blackboard error conditions for missing keys and unbound access", "[blackboard][errors]")
		{
			HeapVector<FieldDescriptor> blackboard_fields;
			blackboard_fields.initialize(1);
			blackboard_fields[0] = FieldDescriptor{"alpha", GET_TYPE_ID(int)};

			SECTION("Throws on unbound field offset")
			{
				Blackboard temp;
				temp.initialize_fields(blackboard_fields);
				WorkloadsBuffer buffer(sizeof(Blackboard) + temp.get_info().total_datablock_size);
				Blackboard* blackboard_ptr = buffer.as<Blackboard>(0);
				new (blackboard_ptr) Blackboard(); // Constructed but not bound
				blackboard_ptr->initialize_fields(blackboard_fields);
				// intentionally don't call blackboard_ptr->bind();

				ROBOTICK_REQUIRE_ERROR_MSG(blackboard_ptr->get<int>("alpha"), ("has not yet been bound"));
			}

			SECTION("Throws on missing field")
			{
				auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(blackboard_fields);
				ROBOTICK_REQUIRE_ERROR_MSG(blackboard->get<int>("nonexistent"), ("Blackboard"));
			}
		}

		SECTION("Blackboard field offset and alignment correctness", "[blackboard][layout]")
		{
			HeapVector<FieldDescriptor> blackboard_fields;
			blackboard_fields.initialize(3);
			blackboard_fields[0] = FieldDescriptor{"a", GET_TYPE_ID(int)};
			blackboard_fields[1] = FieldDescriptor{"b", GET_TYPE_ID(double)};
			blackboard_fields[2] = FieldDescriptor{"c", GET_TYPE_ID(int)};

			auto [buffer, blackboard] = BlackboardTestUtils::make_buffer_and_embedded_blackboard(blackboard_fields);

			blackboard->set<int>("a", 1);
			blackboard->set<double>("b", 3.14);
			blackboard->set<int>("c", 7);

			REQUIRE(blackboard->get<int>("a") == 1);
			REQUIRE(blackboard->get<double>("b") == Catch::Approx(3.14));
			REQUIRE(blackboard->get<int>("c") == 7);
		}
	}

} // namespace robotick
