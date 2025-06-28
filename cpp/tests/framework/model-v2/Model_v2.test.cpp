// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#if 0

#include "robotick/framework/model-v2/Model_v1.h"

#include "robotick/framework/registry-v2/TypeMacros.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

namespace
{
	struct DummyModelWorkload
	{
	};

	ROBOTICK_REGISTER_WORKLOAD(DummyModelWorkload)
} // namespace

TEST_CASE("Unit/Framework/Model_v1/Child inherits parent's tick rate")
{
	Model_v1 model;

	auto child = model.add("DummyModelWorkload", "Child", Model_v1::TICK_RATE_FROM_PARENT);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 100.0);

	model.set_root(parent);
	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds[child.index].tick_rate_hz == 100.0);
}

TEST_CASE("Unit/Framework/Model_v1/Throws if child tick rate faster than parent")
{
	Model_v1 model;

	auto child = model.add("DummyModelWorkload", "Child", 200.0);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 100.0);

	model.set_root(parent, false);
	ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "Child workload cannot have faster tick rate");
}

TEST_CASE("Unit/Framework/Model_v1/Throws if root has no explicit tick rate")
{
	Model_v1 model;

	auto root = model.add("DummyModelWorkload", "Root", Model_v1::TICK_RATE_FROM_PARENT);
	model.set_root(root, false);

	ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "Root workload must have an explicit tick rate");
}

TEST_CASE("Unit/Framework/Model_v1/Add workloads and retrieve them")
{
	Model_v1 model;

	auto h1 = model.add("DummyModelWorkload", "One", 1.0);
	auto h2 = model.add("DummyModelWorkload", "Two", 2.0);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds.size() == 2);
	REQUIRE(seeds[h1.index].name == "One");
	REQUIRE(seeds[h2.index].name == "Two");
}

TEST_CASE("Unit/Framework/Model_v1/Add workloads with and without children")
{
	Model_v1 model;

	auto child = model.add("DummyModelWorkload", "Child", 10.0);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 20.0);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds[parent.index].children.size() == 1);
	REQUIRE(seeds[parent.index].children[0].index == child.index);
}

TEST_CASE("Unit/Framework/Model_v1/Duplicate data connection throws")
{
	Model_v1 model;
	model.add("DummyModelWorkload", "A", 10.0);
	model.add("DummyModelWorkload", "B", 10.0);

	model.connect("A.outputs.x", "B.inputs.x");
	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.y", "B.inputs.x"), "already has an incoming connection");
}

TEST_CASE("Unit/Framework/Model_v1/Cannot connect after root is set")
{
	Model_v1 model;
	auto a = model.add("DummyModelWorkload", "A", 10.0);
	auto b = model.add("DummyModelWorkload", "B", 10.0);

	auto root = model.add("DummyModelWorkload", "Root", std::vector{a, b}, 20.0);
	model.set_root(root);

	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "B.inputs.x"), "Model_v1 root must be set last");
}

TEST_CASE("Unit/Framework/Model_v1/Remote source field path throws")
{
	Model_v1 model;
	model.add("DummyModelWorkload", "A", 10.0);
	model.add("DummyModelWorkload", "B", 10.0);

	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("|remote|field", "B.inputs.x"), "Source field paths cannot be remote");
}

TEST_CASE("Unit/Framework/Model_v1/Connect to remote model")
{
	Model_v1 spine;
	spine.add("DummyModelWorkload", "Steering", 10.0);

	Model_v1 brain;
	brain.add_remote_model(spine, "spine", "ip:127.0.0.1");
	brain.connect("Controller.outputs.turn", "|spine|Steering.inputs.turn_rate");

	const auto& remote_models = brain.get_remote_models();
	REQUIRE(remote_models.count("spine") == 1);
	const auto& remote_seed = remote_models.at("spine");

	REQUIRE(remote_seed.remote_data_connection_seeds.size() == 1);
	REQUIRE(remote_seed.remote_data_connection_seeds[0].dest_field_path == "Steering.inputs.turn_rate");
}

TEST_CASE("Unit/Framework/Model_v1/Invalid remote dest path format throws")
{
	Model_v1 model;
	model.add("DummyModelWorkload", "A", 10.0);

	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "|badformat"), "Invalid remote field format");
}

TEST_CASE("Unit/Framework/Model_v1/Duplicate remote connection throws")
{
	Model_v1 remote;
	remote.add("DummyModelWorkload", "R", 10.0);

	Model_v1 local;
	local.add_remote_model(remote, "spine", "ip:123.0.0.123");

	local.connect("X.outputs.x", "|spine|R.inputs.a");
	ROBOTICK_REQUIRE_ERROR_MSG(local.connect("Y.outputs.y", "|spine|R.inputs.a"), "already has an incoming remote-connection");
}

#endif // #if 0
