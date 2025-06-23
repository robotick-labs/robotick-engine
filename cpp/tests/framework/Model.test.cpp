// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Model.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include <catch2/catch_all.hpp>
#include <stdexcept>
#include <string>

using namespace robotick;

namespace
{
	struct DummyModelWorkload
	{
	};

	struct DummyModelWorkloadRegister
	{
		DummyModelWorkloadRegister()
		{
			const WorkloadRegistryEntry entry = {"DummyModelWorkload", GET_TYPE_ID(DummyModelWorkload), sizeof(DummyModelWorkload),
				alignof(DummyModelWorkload),
				[](void* p)
				{
					new (p) DummyModelWorkload();
				},
				[](void* p)
				{
					static_cast<DummyModelWorkload*>(p)->~DummyModelWorkload();
				},
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static DummyModelWorkloadRegister s_register_dummy;
} // namespace

TEST_CASE("Unit/Framework/Model/Child inherits parent's tick rate")
{
	Model model;

	auto child = model.add("DummyModelWorkload", "Child", Model::TICK_RATE_FROM_PARENT);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 100.0);

	model.set_root(parent);
	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds[child.index].tick_rate_hz == 100.0);
}

TEST_CASE("Unit/Framework/Model/Throws if child tick rate faster than parent")
{
	Model model;

	auto child = model.add("DummyModelWorkload", "Child", 200.0);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 100.0);

	model.set_root(parent, false);
	ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "Child workload cannot have faster tick rate");
}

TEST_CASE("Unit/Framework/Model/Throws if root has no explicit tick rate")
{
	Model model;

	auto root = model.add("DummyModelWorkload", "Root", Model::TICK_RATE_FROM_PARENT);
	model.set_root(root, false);

	ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "Root workload must have an explicit tick rate");
}

TEST_CASE("Unit/Framework/Model/Add workloads and retrieve them")
{
	Model model;

	auto h1 = model.add("DummyModelWorkload", "One", 1.0);
	auto h2 = model.add("DummyModelWorkload", "Two", 2.0);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds.size() == 2);
	REQUIRE(seeds[h1.index].name == "One");
	REQUIRE(seeds[h2.index].name == "Two");
}

TEST_CASE("Unit/Framework/Model/Add workloads with and without children")
{
	Model model;

	auto child = model.add("DummyModelWorkload", "Child", 10.0);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 20.0);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds[parent.index].children.size() == 1);
	REQUIRE(seeds[parent.index].children[0].index == child.index);
}

TEST_CASE("Unit/Framework/Model/Duplicate data connection throws")
{
	Model model;
	model.add("DummyModelWorkload", "A", 10.0);
	model.add("DummyModelWorkload", "B", 10.0);

	model.connect("A.outputs.x", "B.inputs.x");
	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.y", "B.inputs.x"), "already has an incoming connection");
}

TEST_CASE("Unit/Framework/Model/Cannot connect after root is set")
{
	Model model;
	auto a = model.add("DummyModelWorkload", "A", 10.0);
	auto b = model.add("DummyModelWorkload", "B", 10.0);

	auto root = model.add("DummyModelWorkload", "Root", std::vector{a, b}, 20.0);
	model.set_root(root);

	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "B.inputs.x"), "Model root must be set last");
}

TEST_CASE("Unit/Framework/Model/Remote source field path throws")
{
	Model model;
	model.add("DummyModelWorkload", "A", 10.0);
	model.add("DummyModelWorkload", "B", 10.0);

	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("|remote|field", "B.inputs.x"), "Source field paths cannot be remote");
}

TEST_CASE("Unit/Framework/Model/Connect to remote model")
{
	Model spine;
	spine.add("DummyModelWorkload", "Steering", 10.0);

	Model brain;
	brain.add_remote_model(spine, "spine", "ip:127.0.0.1");
	brain.connect("Controller.outputs.turn", "|spine|Steering.inputs.turn_rate");

	const auto& remote_models = brain.get_remote_models();
	REQUIRE(remote_models.count("spine") == 1);
	const auto& remote_seed = remote_models.at("spine");

	REQUIRE(remote_seed.remote_data_connection_seeds.size() == 1);
	REQUIRE(remote_seed.remote_data_connection_seeds[0].dest_field_path == "Steering.inputs.turn_rate");
}

TEST_CASE("Unit/Framework/Model/Invalid remote dest path format throws")
{
	Model model;
	model.add("DummyModelWorkload", "A", 10.0);

	ROBOTICK_REQUIRE_ERROR_MSG(model.connect("A.outputs.x", "|badformat"), "Invalid remote field format");
}

TEST_CASE("Unit/Framework/Model/Duplicate remote connection throws")
{
	Model remote;
	remote.add("DummyModelWorkload", "R", 10.0);

	Model local;
	local.add_remote_model(remote, "spine", "ip:123.0.0.123");

	local.connect("X.outputs.x", "|spine|R.inputs.a");
	ROBOTICK_REQUIRE_ERROR_MSG(local.connect("Y.outputs.y", "|spine|R.inputs.a"), "already has an incoming remote-connection");
}
