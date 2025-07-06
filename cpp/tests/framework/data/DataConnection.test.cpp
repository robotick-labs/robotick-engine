// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"
#include "robotick/api_base.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/utils/TypeId.h"

#include <catch2/catch_all.hpp>
#include <cstring>

namespace robotick::test
{
	namespace
	{
		struct DummyAOutput
		{
			int x = 0;
			double y = 0.0;
			Blackboard out_blackboard;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyAOutput)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, Blackboard, out_blackboard)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, int, x)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, double, y)
		ROBOTICK_REGISTER_STRUCT_END(DummyAOutput)

		struct DummyBInput
		{
			Blackboard in_blackboard;
			double y = 0.0;
			int x = 0;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyBInput)
		ROBOTICK_STRUCT_FIELD(DummyBInput, Blackboard, in_blackboard)
		ROBOTICK_STRUCT_FIELD(DummyBInput, double, y)
		ROBOTICK_STRUCT_FIELD(DummyBInput, int, x)
		ROBOTICK_REGISTER_STRUCT_END(DummyBInput)

		struct DummyState
		{
			HeapVector<FieldDescriptor> blackboard_fields;
		};

		struct DummyA
		{
			DummyAOutput outputs;
			State<DummyState> state;

			void pre_load()
			{
				state->blackboard_fields.initialize(2);
				//
				FieldDescriptor& field_desc_0 = state->blackboard_fields[0];
				field_desc_0.name = "x";
				field_desc_0.type_id = GET_TYPE_ID(int);
				//
				FieldDescriptor& field_desc_1 = state->blackboard_fields[1];
				field_desc_1.name = "y";
				field_desc_1.type_id = GET_TYPE_ID(double);
			}
		};
		ROBOTICK_REGISTER_WORKLOAD(DummyA, void, void, DummyAOutput)

		struct DummyB
		{
			DummyBInput inputs;
			State<DummyState> state;

			void pre_load()
			{
				state->blackboard_fields.initialize(2);
				//
				FieldDescriptor& field_desc_0 = state->blackboard_fields[0];
				field_desc_0.name = "x";
				field_desc_0.type_id = GET_TYPE_ID(int);
				//
				FieldDescriptor& field_desc_1 = state->blackboard_fields[1];
				field_desc_1.name = "y";
				field_desc_1.type_id = GET_TYPE_ID(double);
			}
		};
		ROBOTICK_REGISTER_WORKLOAD(DummyB, void, DummyBInput)
	} // namespace

	TEST_CASE("Unit/Framework/Data/Connection")
	{
		SECTION("Resolves non-blackboard to non-blackboard")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			a->outputs.x = 42;
			a->outputs.y = 3.14;

			std::vector<DataConnectionSeed_v1> seeds = {
				{"A.outputs.x", "B.inputs.x"},
				{"A.outputs.y", "B.inputs.y"},
			};

			std::vector<DataConnectionInfo> resolved =
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info());

			REQUIRE(resolved.size() == 2);

			// Execute copy
			for (const auto& conn : resolved)
			{
				conn.do_data_copy();
			}

			const DummyB* b = engine.find_instance<DummyB>(seed_b.unique_name);
			REQUIRE(b->inputs.x == 42);
			REQUIRE(b->inputs.y == Catch::Approx(3.14));
		}

		SECTION("Resolves non-blackboard to blackboard")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			a->outputs.x = 42;
			a->outputs.y = 3.14;

			std::vector<DataConnectionSeed_v1> seeds = {
				{"A.outputs.x", "B.inputs.in_blackboard.x"},
				{"A.outputs.y", "B.inputs.in_blackboard.y"},
			};

			std::vector<DataConnectionInfo> resolved =
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info());
			REQUIRE(resolved.size() == 2);

			const DummyB* b = engine.find_instance<DummyB>(seed_b.unique_name);

			// Execute copy
			for (const auto& conn : resolved)
			{
				conn.do_data_copy();
			}

			REQUIRE(b->inputs.in_blackboard.get<int>("x") == 42);
			REQUIRE(b->inputs.in_blackboard.get<double>("y") == Catch::Approx(3.14));
		}

		SECTION("Resolves blackboard to non-blackboard")
		{
			Model model;
			model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>("A");
			a->outputs.out_blackboard.set("x", (int)42);
			a->outputs.out_blackboard.set("y", (double)3.14);

			std::vector<DataConnectionSeed_v1> seeds = {
				{"A.outputs.out_blackboard.x", "B.inputs.x"},
				{"A.outputs.out_blackboard.y", "B.inputs.y"},
			};

			std::vector<DataConnectionInfo> resolved =
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info());

			REQUIRE(resolved.size() == 2);

			// Execute copy
			for (const auto& conn : resolved)
			{
				conn.do_data_copy();
			}

			const DummyB* b = engine.find_instance<DummyB>("B");
			REQUIRE(b->inputs.x == 42);
			REQUIRE(b->inputs.y == Catch::Approx(3.14));
		}

		SECTION("Resolves blackboard to blackboard")
		{
			Model model;
			model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>("A");
			a->outputs.out_blackboard.set("x", (int)42);
			a->outputs.out_blackboard.set("y", (double)3.14);

			std::vector<DataConnectionSeed_v1> seeds = {
				{"A.outputs.out_blackboard.x", "B.inputs.in_blackboard.x"},
				{"A.outputs.out_blackboard.y", "B.inputs.in_blackboard.y"},
			};

			std::vector<DataConnectionInfo> resolved =
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info());

			REQUIRE(resolved.size() == 2);

			// Execute copy
			for (const auto& conn : resolved)
			{
				conn.do_data_copy();
			}

			const DummyB* b = engine.find_instance<DummyB>("B");

			REQUIRE(b->inputs.in_blackboard.get<int>("x") == 42);
			REQUIRE(b->inputs.in_blackboard.get<double>("y") == Catch::Approx(3.14));
		}

		SECTION("Errors on invalid connections")
		{
			Model model;
			model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);
			std::vector<WorkloadInstanceInfo> infos = engine.get_all_instance_info();

			SECTION("Invalid workload name")
			{
				std::vector<DataConnectionSeed_v1> seeds = {{"Z.outputs.x", "B.inputs.x"}};
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, infos), ("Z"));
			}

			SECTION("Invalid section")
			{
				std::vector<DataConnectionSeed_v1> seeds = {{"A.wrong.x", "B.inputs.x"}};
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, infos), ("Invalid section"));
			}

			SECTION("Missing field")
			{
				std::vector<DataConnectionSeed_v1> seeds = {{"A.outputs.missing", "B.inputs.x"}};
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, infos), ("field"));
			}

			SECTION("Mismatched types")
			{
				std::vector<DataConnectionSeed_v1> seeds = {{"A.outputs.x", "B.inputs.y"}}; // int -> double
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, infos), ("Type mismatch"));
			}

			SECTION("Duplicate destination")
			{
				std::vector<DataConnectionSeed_v1> seeds = {{"A.outputs.x", "B.inputs.x"}, {"A.outputs.x", "B.inputs.x"}};
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, infos), ("Duplicate"));
			}
		}

		SECTION("Blackboard support pending")
		{
			SUCCEED("Will be added once Blackboard field path support is implemented");
		}

		SECTION("Unidirectional copy")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			auto* b = engine.find_instance<DummyB>(seed_b.unique_name);

			a->outputs.x = 123;
			b->inputs.x = 999; // Should get overwritten

			std::vector<DataConnectionSeed_v1> seeds = {{"A.outputs.x", "B.inputs.x"}};
			std::vector<DataConnectionInfo> resolved =
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info());

			REQUIRE(resolved.size() == 1);
			resolved[0].do_data_copy();

			REQUIRE(b->inputs.x == 123);
			REQUIRE(a->outputs.x == 123); // Confirm unmodified
		}

		SECTION("Throws for blackboard subfield not found")
		{
			Model model;
			model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			std::vector<DataConnectionSeed_v1> seeds = {{"A.outputs.out_blackboard.missing", "B.inputs.in_blackboard.x"}};

			ROBOTICK_REQUIRE_ERROR_MSG(
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info()), ("subfield"));
		}

		SECTION("Different subfields allowed")
		{
			Model model;
			model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			model_helpers::wrap_all_in_sequenced_group(model);

			Engine engine;
			engine.load(model);

			std::vector<DataConnectionSeed_v1> seeds = {
				{"A.outputs.out_blackboard.x", "B.inputs.in_blackboard.x"},
				{"A.outputs.out_blackboard.y", "B.inputs.in_blackboard.y"},
			};

			std::vector<DataConnectionInfo> resolved =
				DataConnectionUtils::create(engine.get_workloads_buffer(), seeds, engine.get_all_instance_info());

			REQUIRE(resolved.size() == 2);
		}
	}

} // namespace robotick::test
