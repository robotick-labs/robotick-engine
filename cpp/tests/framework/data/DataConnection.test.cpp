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
			Vec3 out_vec3;
			Blackboard out_blackboard;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyAOutput)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, Blackboard, out_blackboard)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, int, x)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, double, y)
		ROBOTICK_STRUCT_FIELD(DummyAOutput, Vec3, out_vec3)
		ROBOTICK_REGISTER_STRUCT_END(DummyAOutput)

		struct DummyBInput
		{
			Blackboard in_blackboard;
			double y = 0.0;
			int x = 0;
			Vec3 in_vec3;
		};
		ROBOTICK_REGISTER_STRUCT_BEGIN(DummyBInput)
		ROBOTICK_STRUCT_FIELD(DummyBInput, Blackboard, in_blackboard)
		ROBOTICK_STRUCT_FIELD(DummyBInput, double, y)
		ROBOTICK_STRUCT_FIELD(DummyBInput, int, x)
		ROBOTICK_STRUCT_FIELD(DummyBInput, Vec3, in_vec3)
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
				//
				outputs.out_blackboard.initialize_fields(state->blackboard_fields);
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
				//
				inputs.in_blackboard.initialize_fields(state->blackboard_fields);
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
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			a->outputs.x = 42;
			a->outputs.y = 3.14;

			static const DataConnectionSeed data_connection_1("A.outputs.x", "B.inputs.x");
			static const DataConnectionSeed data_connection_2("A.outputs.y", "B.inputs.y");

			static const DataConnectionSeed* connection_array[] = {
				&data_connection_1,
				&data_connection_2,
			};

			ArrayView<const DataConnectionSeed*> seeds(connection_array);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

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
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			a->outputs.x = 42;
			a->outputs.y = 3.14;

			static const DataConnectionSeed data_connection_1("A.outputs.x", "B.inputs.in_blackboard.x");
			static const DataConnectionSeed data_connection_2("A.outputs.y", "B.inputs.in_blackboard.y");

			static const DataConnectionSeed* connection_array[] = {
				&data_connection_1,
				&data_connection_2,
			};

			ArrayView<const DataConnectionSeed*> seeds(connection_array);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

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
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>("A");
			a->outputs.out_blackboard.set("x", (int)42);
			a->outputs.out_blackboard.set("y", (double)3.14);

			static const DataConnectionSeed data_connection_1("A.outputs.out_blackboard.x", "B.inputs.x");
			static const DataConnectionSeed data_connection_2("A.outputs.out_blackboard.y", "B.inputs.y");

			static const DataConnectionSeed* connection_array[] = {
				&data_connection_1,
				&data_connection_2,
			};

			ArrayView<const DataConnectionSeed*> seeds(connection_array);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

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
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			// Modify live instance values
			auto* a = engine.find_instance<DummyA>("A");
			a->outputs.out_blackboard.set("x", (int)42);
			a->outputs.out_blackboard.set("y", (double)3.14);

			static const DataConnectionSeed data_connection_1("A.outputs.out_blackboard.x", "B.inputs.in_blackboard.x");
			static const DataConnectionSeed data_connection_2("A.outputs.out_blackboard.y", "B.inputs.in_blackboard.y");

			static const DataConnectionSeed* connection_array[] = {
				&data_connection_1,
				&data_connection_2,
			};

			ArrayView<const DataConnectionSeed*> seeds(connection_array);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

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
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);
			const Map<const char*, WorkloadInstanceInfo*>& infos_map = engine.get_all_instance_info_map();

			SECTION("Invalid workload name")
			{
				static const DataConnectionSeed conn_1("Z.outputs.x", "B.inputs.x");
				static const DataConnectionSeed* connections[] = {&conn_1};
				ArrayView<const DataConnectionSeed*> seeds(connections);

				HeapVector<DataConnectionInfo> resolved;
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, infos_map), ("Z"));
			}

			SECTION("Invalid section")
			{
				static const DataConnectionSeed conn_1("A.wrong.x", "B.inputs.x");
				static const DataConnectionSeed* connections[] = {&conn_1};
				ArrayView<const DataConnectionSeed*> seeds(connections);

				HeapVector<DataConnectionInfo> resolved;
				ROBOTICK_REQUIRE_ERROR_MSG(
					DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, infos_map), ("Invalid section"));
			}

			SECTION("Missing field")
			{
				static const DataConnectionSeed conn_1("A.outputs.missing", "B.inputs.x");
				static const DataConnectionSeed* connections[] = {&conn_1};
				ArrayView<const DataConnectionSeed*> seeds(connections);

				HeapVector<DataConnectionInfo> resolved;
				ROBOTICK_REQUIRE_ERROR_MSG(
					DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, infos_map), ("Field 'missing' not found"));
			}

			SECTION("Mismatched types")
			{
				static const DataConnectionSeed conn_1("A.outputs.x", "B.inputs.y"); // int -> double
				static const DataConnectionSeed* connections[] = {&conn_1};
				ArrayView<const DataConnectionSeed*> seeds(connections);

				HeapVector<DataConnectionInfo> resolved;
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, infos_map), ("Type mismatch"));
			}

			SECTION("Duplicate destination")
			{
				static const DataConnectionSeed conn_1("A.outputs.x", "B.inputs.x");
				static const DataConnectionSeed conn_2("A.outputs.x", "B.inputs.x");
				static const DataConnectionSeed* connections[] = {&conn_1, &conn_2};
				ArrayView<const DataConnectionSeed*> seeds(connections);

				HeapVector<DataConnectionInfo> resolved;
				ROBOTICK_REQUIRE_ERROR_MSG(DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, infos_map), ("Duplicate"));
			}
		}

		SECTION("Blackboard support pending")
		{
			SUCCEED("Will be added once Blackboard field path support is implemented");
		}

		SECTION("Unidirectional copy (primitive-int)")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			auto* b = engine.find_instance<DummyB>(seed_b.unique_name);

			a->outputs.x = 123;
			b->inputs.x = 999; // Should get overwritten

			static const DataConnectionSeed conn_1("A.outputs.x", "B.inputs.x");
			static const DataConnectionSeed* connections[] = {&conn_1};
			ArrayView<const DataConnectionSeed*> seeds(connections);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

			REQUIRE(resolved.size() == 1);
			resolved[0].do_data_copy();

			REQUIRE(b->inputs.x == 123);
			REQUIRE(a->outputs.x == 123); // Confirm unmodified
		}

		SECTION("Unidirectional copy (whole vec3)")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			auto* b = engine.find_instance<DummyB>(seed_b.unique_name);

			a->outputs.out_vec3 = Vec3(1, 2, 3);
			b->inputs.in_vec3 = Vec3(9, 9, 9); // Should get overwritten

			static const DataConnectionSeed conn_1("A.outputs.out_vec3", "B.inputs.in_vec3");
			static const DataConnectionSeed* connections[] = {&conn_1};
			ArrayView<const DataConnectionSeed*> seeds(connections);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

			REQUIRE(resolved.size() == 1);
			resolved[0].do_data_copy();

			REQUIRE(b->inputs.in_vec3 == Vec3(1, 2, 3));
			REQUIRE(a->outputs.out_vec3 == Vec3(1, 2, 3)); // Confirm unmodified
		}

		SECTION("Unidirectional copy (per-element vec3)")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			auto* a = engine.find_instance<DummyA>(seed_a.unique_name);
			auto* b = engine.find_instance<DummyB>(seed_b.unique_name);

			a->outputs.out_vec3 = Vec3(1, 2, 3);
			b->inputs.in_vec3 = Vec3(9, 9, 9); // Should get overwritten

			static const DataConnectionSeed conn_X("A.outputs.out_vec3.x", "B.inputs.in_vec3.x");
			static const DataConnectionSeed conn_Y("A.outputs.out_vec3.y", "B.inputs.in_vec3.y");
			static const DataConnectionSeed conn_Z("A.outputs.out_vec3.z", "B.inputs.in_vec3.z");
			static const DataConnectionSeed* connections[] = {&conn_X, &conn_Y, &conn_Z};
			ArrayView<const DataConnectionSeed*> seeds(connections);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

			REQUIRE(resolved.size() == 3);

			resolved[0].do_data_copy();
			REQUIRE(b->inputs.in_vec3 == Vec3(1, 9, 9));
			REQUIRE(a->outputs.out_vec3 == Vec3(1, 2, 3)); // Confirm unmodified

			resolved[1].do_data_copy();
			REQUIRE(b->inputs.in_vec3 == Vec3(1, 2, 9));
			REQUIRE(a->outputs.out_vec3 == Vec3(1, 2, 3)); // Confirm unmodified

			resolved[2].do_data_copy();
			REQUIRE(b->inputs.in_vec3 == Vec3(1, 2, 3));
			REQUIRE(a->outputs.out_vec3 == Vec3(1, 2, 3)); // Confirm unmodified
		}

		SECTION("Throws for blackboard subfield not found")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			static const DataConnectionSeed conn_1("A.outputs.out_blackboard.missing", "B.inputs.in_blackboard.x");
			static const DataConnectionSeed* connections[] = {&conn_1};
			ArrayView<const DataConnectionSeed*> seeds(connections);

			HeapVector<DataConnectionInfo> resolved;
			ROBOTICK_REQUIRE_ERROR_MSG(
				DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map()), ("sub-field"));
		}

		SECTION("Different subfields allowed")
		{
			Model model;
			const WorkloadSeed& seed_a = model.add("DummyA", "A").set_tick_rate_hz(1.0f);
			const WorkloadSeed& seed_b = model.add("DummyB", "B").set_tick_rate_hz(1.0f);
			const WorkloadSeed& root = model.add("TestSequencedGroupWorkload", "group").set_tick_rate_hz(1.0f).set_children({&seed_a, &seed_b});
			model.set_root_workload(root);

			Engine engine;
			engine.load(model);

			static const DataConnectionSeed conn_1("A.outputs.out_blackboard.x", "B.inputs.in_blackboard.x");
			static const DataConnectionSeed conn_2("A.outputs.out_blackboard.y", "B.inputs.in_blackboard.y");

			static const DataConnectionSeed* connections[] = {
				&conn_1,
				&conn_2,
			};

			ArrayView<const DataConnectionSeed*> seeds(connections);

			HeapVector<DataConnectionInfo> resolved;
			DataConnectionUtils::create(resolved, engine.get_workloads_buffer(), seeds, engine.get_all_instance_info_map());

			REQUIRE(resolved.size() == 2);
		}
	}

} // namespace robotick::test
