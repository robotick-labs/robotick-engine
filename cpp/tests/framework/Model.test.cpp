// Copyright Robotick Labs
//
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
	// === DummyModelWorkload ===

	struct DummyModelWorkload
	{
	};

	struct DummyModelWorkloadRegister
	{
		DummyModelWorkloadRegister()
		{
			const WorkloadRegistryEntry entry = {"DummyModelWorkload", sizeof(DummyModelWorkload), alignof(DummyModelWorkload),
				[](void* p)
				{
					new (p) DummyModelWorkload();
				},
				[](void* p)
				{
					static_cast<DummyModelWorkload*>(p)->~DummyModelWorkload();
				},
				nullptr, 0, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

			WorkloadRegistry::get().register_entry(entry);
		}
	};
	static DummyModelWorkloadRegister s_register_dummy;
} // namespace

TEST_CASE("Unit|Framework|Model|Child inherits parent's tick rate")
{
	Model model;

	auto child = model.add("DummyModelWorkload", "Child", Model::TICK_RATE_FROM_PARENT);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 100.0);

	model.set_root(parent);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds[child.index].tick_rate_hz == 100.0);
}

TEST_CASE("Unit|Framework|Model|Throws if child tick rate faster than parent")
{
	Model model;

	auto child = model.add("DummyModelWorkload", "Child", 200.0);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 100.0);

	model.set_root(parent, false);
	REQUIRE_THROWS_AS(model.finalize(), std::exception);
}

TEST_CASE("Unit|Framework|Model|Throws if root has no explicit tick rate")
{
	Model model;

	auto root = model.add("DummyModelWorkload", "Root", Model::TICK_RATE_FROM_PARENT);
	model.set_root(root, false);

	REQUIRE_THROWS_AS(model.finalize(), std::exception);
}

TEST_CASE("Unit|Framework|Model|Add workloads and retrieve them")
{
	Model model;

	auto h1 = model.add("DummyModelWorkload", "One", 1.0);
	auto h2 = model.add("DummyModelWorkload", "Two", 2.0);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds.size() == 2);
	REQUIRE(seeds[h1.index].name == "One");
	REQUIRE(seeds[h2.index].name == "Two");
}

TEST_CASE("Unit|Framework|Model|Add workloads with and without children")
{
	Model model;

	auto child = model.add("DummyModelWorkload", "Child", 10.0);
	auto parent = model.add("DummyModelWorkload", "Parent", std::vector{child}, 20.0);

	const auto& seeds = model.get_workload_seeds();
	REQUIRE(seeds[parent.index].children.size() == 1);
	REQUIRE(seeds[parent.index].children[0].index == child.index);
}
