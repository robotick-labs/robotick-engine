// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/api.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{

	namespace
	{
		struct DummyModelDataConnWorkload
		{
		};

		struct DummyRegister
		{
			DummyRegister()
			{
				const WorkloadRegistryEntry entry = {"DummyModelDataConnWorkload", sizeof(DummyModelDataConnWorkload),
					alignof(DummyModelDataConnWorkload),
					[](void* p)
					{
						new (p) DummyModelDataConnWorkload();
					},
					[](void* p)
					{
						static_cast<DummyModelDataConnWorkload*>(p)->~DummyModelDataConnWorkload();
					},
					nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

				WorkloadRegistry::get().register_entry(entry);
			}
		};
		static DummyRegister s_register;
	} // namespace

	TEST_CASE("Unit|Framework|DataConnections|Allows connecting input between valid workloads")
	{
		Model model;

		auto a = model.add("DummyModelDataConnWorkload", "A", 10.0);
		auto b = model.add("DummyModelDataConnWorkload", "B", 10.0);

		model.connect("A.output", "B.input");

		auto group = model.add("SequencedGroupWorkload", "Group", std::vector{a, b}, 10.0);
		model.set_root(group);

		REQUIRE_NOTHROW(model.finalize());
	}

	TEST_CASE("Unit|Framework|DataConnections|Duplicate inputs throw with clear error")
	{
		Model model;

		model.add("DummyModelDataConnWorkload", "A", 10.0);
		model.add("DummyModelDataConnWorkload", "B", 10.0);
		model.add("DummyModelDataConnWorkload", "C", 10.0);

		model.connect("A.output", "C.input");
		ROBOTICK_REQUIRE_ERROR(model.connect("B.output", "C.input"), ("already has an incoming connection"));
	}

	TEST_CASE("Unit|Framework|DataConnections|Seeds are preserved for engine use")
	{
		Model model;

		auto a = model.add("DummyModelDataConnWorkload", "A", 10.0);
		auto b = model.add("DummyModelDataConnWorkload", "B", 10.0);
		model.connect("A.output", "B.input");

		auto group = model.add("SequencedGroupWorkload", "Group", std::vector{a, b}, 10.0);
		model.set_root(group);

		const auto& seeds = model.get_data_connection_seeds();
		REQUIRE(seeds.size() == 1);
		CHECK(seeds[0].source_field_path == "A.output");
		CHECK(seeds[0].dest_field_path == "B.input");

		REQUIRE_NOTHROW(model.finalize());
	}

} // namespace robotick::test
