// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/model/Model.h"
#include "robotick/framework/Engine.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

namespace
{
	struct DummyModelWorkload
	{
	};

	ROBOTICK_REGISTER_WORKLOAD(DummyModelWorkload)
} // namespace

TEST_CASE("Unit/Framework/Model-Dynamic")
{
	static const float s_tick_100hz(100.f);
	static const float s_tick_200hz(100.f);

	SECTION("Throws if child tick rate faster than parent")
	{
		Model model;

		const auto& child = model.add("DummyModelWorkload", "Child").set_tick_rate_hz(s_tick_200hz);
		const auto& parent = model.add("DummyModelWorkload", "Parent").set_tick_rate_hz(s_tick_100hz).set_children({&child});

		model.set_root_workload(parent, false);
		ROBOTICK_REQUIRE_ERROR_MSG(model.validate(), "Child workload cannot have faster tick rate than parent");
	}

	SECTION("Add workloads and retrieve them")
	{
		Model model;

		const auto& one = model.add("DummyModelWorkload", "One").set_tick_rate_hz(s_tick_100hz);
		const auto& two = model.add("DummyModelWorkload", "Two").set_tick_rate_hz(s_tick_200hz);

		const auto& seeds = model.get_workload_seeds();
		REQUIRE(seeds.size() == 2);
		REQUIRE(((seeds[0] == &one && seeds[1] == &two) || (seeds[1] == &one && seeds[0] == &two)));

		REQUIRE(one.type->id == GET_TYPE_ID(DummyModelWorkload));
		REQUIRE(one.name == "One");
		REQUIRE(one.tick_rate_hz == s_tick_100hz);

		REQUIRE(two.type->id == GET_TYPE_ID(DummyModelWorkload));
		REQUIRE(two.name == "Two");
		REQUIRE(two.tick_rate_hz == s_tick_200hz);
	}

	SECTION("Add workloads with and without children")
	{
		Model model;

		const auto& child = model.add("DummyModelWorkload", "Child").set_tick_rate_hz(s_tick_100hz);
		const auto& parent = model.add("DummyModelWorkload", "Parent").set_children({&child}).set_tick_rate_hz(s_tick_200hz);

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
		const auto& a = model.add("DummyModelWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		const auto& b = model.add("DummyModelWorkload", "B").set_tick_rate_hz(s_tick_100hz);

		auto root = model.add("DummyModelWorkload", "Root").set_children({&a, &b}).set_tick_rate_hz(s_tick_200hz);
		model.set_root_workload(root);

		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "B.inputs.x"), "Model root must be set last");
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
		Model spine;
		spine.add("DummyModelWorkload", "Steering").set_tick_rate_hz(s_tick_100hz);

		Model brain;
		brain.add_remote_model(spine, "spine", "ip:127.0.0.1");
		brain.connect("Controller.outputs.turn", "|spine|Steering.inputs.turn_rate");

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
		remote.add("DummyModelWorkload", "R").set_tick_rate_hz(s_tick_100hz);

		Model local;
		local.add_remote_model(remote, "spine", "ip:123.0.0.123");

		local.connect("X.outputs.x", "|spine|R.inputs.a");
		ROBOTICK_REQUIRE_ERROR_MSG(local.connect("Y.outputs.y", "|spine|R.inputs.a"), "already has an incoming remote-connection");
	}

	SECTION("Allows connecting input between valid workloads")
	{
		Model model;

		auto a = model.add("DummyModelDataConnWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		auto b = model.add("DummyModelDataConnWorkload", "B").set_tick_rate_hz(s_tick_100hz);

		model.connect("A.output", "B.input");

		auto group = model.add("SequencedGroupWorkload", "Group").set_children({&a, &b}).set_tick_rate_hz(s_tick_100hz);
		model.set_root_workload(group);

		REQUIRE_NOTHROW(model.validate());
	}

	SECTION("Duplicate inputs throw with clear error")
	{
		Model model;

		model.add("DummyModelDataConnWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		model.add("DummyModelDataConnWorkload", "B").set_tick_rate_hz(s_tick_100hz);
		model.add("DummyModelDataConnWorkload", "C").set_tick_rate_hz(s_tick_100hz);

		model.connect("A.output", "C.input");
		ROBOTICK_REQUIRE_ERROR_MSG(model.connect("B.output", "C.input"), ("already has an incoming connection"));
	}

	SECTION("Seeds are preserved for engine use")
	{
		Model model;

		auto a = model.add("DummyModelDataConnWorkload", "A").set_tick_rate_hz(s_tick_100hz);
		auto b = model.add("DummyModelDataConnWorkload", "B").set_tick_rate_hz(s_tick_100hz);
		model.connect("A.output", "B.input");

		auto group = model.add("SequencedGroupWorkload", "Group").set_children({&a, &b}).set_tick_rate_hz(s_tick_100hz);
		model.set_root_workload(group);

		const auto& seeds = model.get_data_connection_seeds();
		REQUIRE(seeds.size() == 1);
		CHECK(seeds[0]->source_field_path == "A.output");
		CHECK(seeds[0]->dest_field_path == "B.input");

		REQUIRE_NOTHROW(model.validate());
	}
}
