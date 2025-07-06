// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/model/Model.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

namespace
{
	struct DummyModelConfig
	{
		float entry_float = 0.0f;
		bool entry_bool = false;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(DummyModelConfig)
	ROBOTICK_STRUCT_FIELD(DummyModelConfig, float, entry_float)
	ROBOTICK_STRUCT_FIELD(DummyModelConfig, bool, entry_bool)
	ROBOTICK_REGISTER_STRUCT_END(DummyModelConfig)

	struct DummyModelInputs
	{
		int entry_int = 0;
		FixedString32 entry_string = "Hello";
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(DummyModelInputs)
	ROBOTICK_STRUCT_FIELD(DummyModelInputs, int, entry_int)
	ROBOTICK_STRUCT_FIELD(DummyModelInputs, FixedString32, entry_string)
	ROBOTICK_REGISTER_STRUCT_END(DummyModelInputs)

	struct DummyModelWorkload
	{
		DummyModelConfig config;
		DummyModelInputs inputs;
	};
	ROBOTICK_REGISTER_WORKLOAD(DummyModelWorkload, DummyModelConfig, DummyModelInputs)
} // namespace

TEST_CASE("Unit/Framework/Model-Dynamic")
{
	static const float s_tick_100hz(100.f);
	static const float s_tick_200hz(200.f);

	SECTION("Throws if child tick rate faster than parent")
	{
		Model model;

		const WorkloadSeed& child = model.add("DummyModelWorkload", "Child").set_tick_rate_hz(s_tick_200hz);
		const WorkloadSeed& parent = model.add("DummyModelWorkload", "Parent").set_tick_rate_hz(s_tick_100hz).set_children({&child});

		model.set_root_workload(parent, false);
		ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "faster tick rate");
	}

	SECTION("Add workloads and retrieve them")
	{
		Model model;

		const WorkloadSeed& one = model.add("DummyModelWorkload", "One").set_tick_rate_hz(s_tick_100hz);
		const WorkloadSeed& two = model.add("DummyModelWorkload", "Two").set_tick_rate_hz(s_tick_200hz);

		const bool auto_finalise_and_validate = true;
		model.set_root_workload(one, auto_finalise_and_validate);

		const auto& seeds = model.get_workload_seeds();
		REQUIRE(seeds.size() == 2);
		REQUIRE(((seeds[0] == &one && seeds[1] == &two) || (seeds[1] == &one && seeds[0] == &two)));

		REQUIRE(one.type_id == TypeId("DummyModelWorkload"));
		REQUIRE(one.unique_name == "One");
		REQUIRE(one.tick_rate_hz == s_tick_100hz);

		REQUIRE(two.type_id == TypeId("DummyModelWorkload"));
		REQUIRE(two.unique_name == "Two");
		REQUIRE(two.tick_rate_hz == s_tick_200hz);
	}

	SECTION("Add workloads with and without children")
	{
		Model model;

		const WorkloadSeed& child = model.add("DummyModelWorkload", "Child").set_tick_rate_hz(s_tick_100hz);
		const WorkloadSeed& parent = model.add("DummyModelWorkload", "Parent").set_children({&child}).set_tick_rate_hz(s_tick_200hz);

		const bool auto_finalise_and_validate = true;
		model.set_root_workload(parent, auto_finalise_and_validate);

		REQUIRE(parent.children.size() == 1);
		REQUIRE(parent.children[0] == &child);
	}

	SECTION("Duplicate data connection throws")
	{
		Model model;
		model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz);

		model.connect("A.outputs.x", "B.inputs.x");
		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.y", "B.inputs.x"), "already has an incoming connection");
	}

	SECTION("Cannot connect after root is set")
	{
		Model model;
		const WorkloadSeed& a = model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		const WorkloadSeed& b = model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz);

		const WorkloadSeed& root = model.add("DummyModelWorkload", "Root").set_children({&a, &b}).set_tick_rate_hz(s_tick_200hz);
		model.set_root_workload(root);

		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "B.inputs.x"), "Root must be set last");
	}

	SECTION("Remote source field path throws")
	{
		Model model;
		model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz);

		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("|remote|field", "B.inputs.x"), "Source field paths cannot be remote");
	}

	SECTION("Connect to remote model")
	{
		// create spine remote-model:
		Model spine;
		const WorkloadSeed& steering = spine.add("DummyModelWorkload", "Steering").set_tick_rate_hz(s_tick_100hz);

		const bool auto_finalise_and_validate = true;
		spine.set_root_workload(steering, auto_finalise_and_validate);

		// create brain local-model:
		Model brain;
		const WorkloadSeed& controller = brain.add("DummyModelWorkload", "Controller").set_tick_rate_hz(s_tick_100hz);
		brain.add_remote_model(spine, "spine", "ip:127.0.0.1");
		brain.connect("Controller.outputs.turn", "|spine|Steering.inputs.turn_rate");

		brain.set_root_workload(controller, auto_finalise_and_validate);

		const auto& remote_models = brain.get_remote_models();
		REQUIRE((remote_models.size() == 1 && remote_models[0]->model_name == "spine"));
		const auto* remote_model = remote_models[0];

		REQUIRE(remote_model->remote_data_connection_seeds.size() == 1);
		REQUIRE(remote_model->remote_data_connection_seeds[0]->dest_field_path == "Steering.inputs.turn_rate");
	}

	SECTION("Invalid remote dest path format throws")
	{
		Model model;
		model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);

		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "|badformat"), "Invalid remote field format");
	}

	SECTION("Duplicate remote connection throws")
	{
		Model remote;
		const WorkloadSeed& remote_workload = remote.add("DummyModelWorkload", "R").set_tick_rate_hz(s_tick_100hz);
		remote.set_root_workload(remote_workload);

		Model local;
		local.add("DummyModelWorkload", "X").set_tick_rate_hz(s_tick_100hz);
		local.add("DummyModelWorkload", "Y").set_tick_rate_hz(s_tick_100hz);
		local.add_remote_model(remote, "spine", "ip:123.0.0.123");

		local.connect("X.outputs.x", "|spine|R.inputs.a");
		ROBOTICK_REQUIRE_ERROR_MSG(local.connect("Y.outputs.y", "|spine|R.inputs.a"), "already has an incoming remote-connection");
	}

	SECTION("Allows connecting input between valid workloads")
	{
		Model model;

		const WorkloadSeed& a = model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		const WorkloadSeed& b = model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz);

		model.connect("A.output", "B.input");

		const WorkloadSeed& group = model.add("DummyModelWorkload", "Group").set_children({&a, &b}).set_tick_rate_hz(s_tick_100hz);
		model.set_root_workload(group, false);

		REQUIRE_NOTHROW(model.finalize());
	}

	SECTION("Duplicate inputs throw with clear error")
	{
		Model model;

		model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz);
		model.add("DummyModelWorkload", "C").set_tick_rate_hz(s_tick_100hz);

		model.connect("A.output", "C.input");
		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("B.output", "C.input"), ("already has an incoming connection"));
	}

	SECTION("Seeds are preserved for engine use")
	{
		Model model;

		const WorkloadSeed& a =
			model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz).set_config({{"entry_float", "2.0f"}, {"entry_bool", "false"}});
		const WorkloadSeed& b =
			model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz).set_inputs({{"entry_int", "10"}, {"entry_string", "there"}});
		model.connect("A.output", "B.input");

		const WorkloadSeed& group = model.add("DummyModelWorkload", "Group").set_children({&a, &b}).set_tick_rate_hz(s_tick_100hz);
		model.set_root_workload(group);

		const auto& workload_seeds = model.get_workload_seeds();
		REQUIRE(workload_seeds.size() == 3);

		// Check names, types, tick rates, and child linkage
		const WorkloadSeed* found_a = nullptr;
		const WorkloadSeed* found_b = nullptr;
		const WorkloadSeed* found_group = nullptr;

		for (const WorkloadSeed* seed : workload_seeds)
		{
			CHECK(seed != nullptr);
			if (strcmp(seed->unique_name.c_str(), "A") == 0)
			{
				found_a = seed;
				CHECK(seed->type_id == TypeId("DummyModelWorkload"));
				CHECK(seed->tick_rate_hz == s_tick_100hz);
				CHECK(seed->children.size() == 0);
			}
			else if (strcmp(seed->unique_name.c_str(), "B") == 0)
			{
				found_b = seed;
				CHECK(seed->type_id == TypeId("DummyModelWorkload"));
				CHECK(seed->tick_rate_hz == s_tick_100hz);
				CHECK(seed->children.size() == 0);
			}
			else if (strcmp(seed->unique_name.c_str(), "Group") == 0)
			{
				found_group = seed;
				CHECK(seed->type_id == TypeId("DummyModelWorkload"));
				CHECK(seed->tick_rate_hz == s_tick_100hz);
			}
			else
			{
				FAIL("Unexpected workload name");
			}
		}

		REQUIRE(found_a);
		REQUIRE(found_b);
		REQUIRE(found_group);

		CHECK(found_group->children.size() == 2);
		CHECK(((found_group->children[0] == found_a && found_group->children[1] == found_b) ||
			   (found_group->children[0] == found_b && found_group->children[1] == found_a)));

		// Validate connection
		const auto& data_connection_seeds = model.get_data_connection_seeds();
		REQUIRE(data_connection_seeds.size() == 1);
		const auto* conn = data_connection_seeds[0];
		CHECK(conn->source_field_path == "A.output");
		CHECK(conn->dest_field_path == "B.input");

		// Validate config entries on A
		REQUIRE(found_a->config.size() == 2);
		CHECK(found_a->config[0].first == "entry_float");
		CHECK(found_a->config[0].second == "2.0f");
		CHECK(found_a->config[1].first == "entry_bool");
		CHECK(found_a->config[1].second == "false");

		// Validate input entries on B
		REQUIRE(found_b->inputs.size() == 2);
		CHECK(found_b->inputs[0].first == "entry_int");
		CHECK(found_b->inputs[0].second == "10");
		CHECK(found_b->inputs[1].first == "entry_string");
		CHECK(found_b->inputs[1].second == "there");
	}
}
