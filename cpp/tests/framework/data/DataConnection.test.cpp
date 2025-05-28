// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/data/DataConnection.h"
#include "../utils/EngineInspector.h"
#include "../utils/ModelHelper.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
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

			DummyAOutput()
				: out_blackboard({BlackboardField("x", std::type_index(typeid(int))), BlackboardField("y", std::type_index(typeid(double)))})
			{
			}
		};
		ROBOTICK_BEGIN_FIELDS(DummyAOutput)
		ROBOTICK_FIELD(DummyAOutput, out_blackboard)
		ROBOTICK_FIELD(DummyAOutput, x)
		ROBOTICK_FIELD(DummyAOutput, y)
		ROBOTICK_END_FIELDS()

		struct DummyBInput
		{
			Blackboard in_blackboard;
			double y = 0.0;
			int x = 0;

			DummyBInput() : in_blackboard({BlackboardField("x", std::type_index(typeid(int))), BlackboardField("y", std::type_index(typeid(double)))})
			{
			}
		};
		ROBOTICK_BEGIN_FIELDS(DummyBInput)
		ROBOTICK_FIELD(DummyBInput, in_blackboard)
		ROBOTICK_FIELD(DummyBInput, y)
		ROBOTICK_FIELD(DummyBInput, x)
		ROBOTICK_END_FIELDS()

		struct DummyA
		{
			DummyAOutput outputs;
		};
		ROBOTICK_DEFINE_WORKLOAD(DummyA)

		struct DummyB
		{
			DummyBInput inputs;
		};
		ROBOTICK_DEFINE_WORKLOAD(DummyB)
	} // namespace

	TEST_CASE("Unit|Framework|DataConnection|Resolves non-blackboard to non-blackboard")
	{
		Model model;
		model.add("DummyA", "A", 1.0);
		model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		// Modify live instance values
		auto* a = EngineInspector::get_instance<DummyA>(engine, 0);
		a->outputs.x = 42;
		a->outputs.y = 3.14;

		std::vector<DataConnectionSeed> seeds = {
			{"A.outputs.x", "B.inputs.x"},
			{"A.outputs.y", "B.inputs.y"},
		};

		std::vector<DataConnectionInfo> resolved = DataConnectionResolver::resolve(seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.copy_data();
		}

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, 1);
		REQUIRE(b->inputs.x == 42);
		REQUIRE(b->inputs.y == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|DataConnection|Resolves non-blackboard to blackboard")
	{
		Model model;
		const WorkloadHandle handle_a = model.add("DummyA", "A", 1.0);
		const WorkloadHandle handle_b = model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		// Modify live instance values
		auto* a = EngineInspector::get_instance<DummyA>(engine, handle_a.index);
		a->outputs.x = 42;
		a->outputs.y = 3.14;

		std::vector<DataConnectionSeed> seeds = {
			{"A.outputs.x", "B.inputs.in_blackboard.x"},
			{"A.outputs.y", "B.inputs.in_blackboard.y"},
		};

		std::vector<DataConnectionInfo> resolved = DataConnectionResolver::resolve(seeds, EngineInspector::get_all_instance_info(engine));
		REQUIRE(resolved.size() == 2);

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, handle_b.index);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.copy_data();
		}

		REQUIRE(b->inputs.in_blackboard.get<int>("x") == 42);
		REQUIRE(b->inputs.in_blackboard.get<double>("y") == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|DataConnection|Resolves blackboard to non-blackboard")
	{
		Model model;
		model.add("DummyA", "A", 1.0);
		model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		// Modify live instance values
		auto* a = EngineInspector::get_instance<DummyA>(engine, 0);
		a->outputs.out_blackboard.set("x", (int)42);
		a->outputs.out_blackboard.set("y", (double)3.14);

		std::vector<DataConnectionSeed> seeds = {
			{"A.outputs.out_blackboard.x", "B.inputs.x"},
			{"A.outputs.out_blackboard.y", "B.inputs.y"},
		};

		std::vector<DataConnectionInfo> resolved = DataConnectionResolver::resolve(seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.copy_data();
		}

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, 1);
		REQUIRE(b->inputs.x == 42);
		REQUIRE(b->inputs.y == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|DataConnection|Resolves blackboard to blackboard")
	{
		Model model;
		model.add("DummyA", "A", 1.0);
		model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		// Modify live instance values
		auto* a = EngineInspector::get_instance<DummyA>(engine, 0);
		a->outputs.out_blackboard.set("x", (int)42);
		a->outputs.out_blackboard.set("y", (double)3.14);

		std::vector<DataConnectionSeed> seeds = {
			{"A.outputs.out_blackboard.x", "B.inputs.in_blackboard.x"},
			{"A.outputs.out_blackboard.y", "B.inputs.in_blackboard.y"},
		};

		std::vector<DataConnectionInfo> resolved = DataConnectionResolver::resolve(seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.copy_data();
		}

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, 1);

		REQUIRE(b->inputs.in_blackboard.get<int>("x") == 42);
		REQUIRE(b->inputs.in_blackboard.get<double>("y") == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|DataConnection|Errors on invalid connections")
	{
		Model model;
		model.add("DummyA", "A", 1.0);
		model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);
		std::vector<WorkloadInstanceInfo> infos = EngineInspector::get_all_instance_info(engine);

		SECTION("Invalid workload name")
		{
			std::vector<DataConnectionSeed> seeds = {{"Z.outputs.x", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionResolver::resolve(seeds, infos), Catch::Matchers::ContainsSubstring("Z"));
		}

		SECTION("Invalid section")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.wrong.x", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionResolver::resolve(seeds, infos), Catch::Matchers::ContainsSubstring("Invalid section"));
		}

		SECTION("Missing field")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.outputs.missing", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionResolver::resolve(seeds, infos), Catch::Matchers::ContainsSubstring("field"));
		}

		SECTION("Mismatched types")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.outputs.x", "B.inputs.y"}}; // int -> double
			REQUIRE_THROWS_WITH(DataConnectionResolver::resolve(seeds, infos), Catch::Matchers::ContainsSubstring("Type mismatch"));
		}

		SECTION("Duplicate destination")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.outputs.x", "B.inputs.x"}, {"A.outputs.x", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionResolver::resolve(seeds, infos), Catch::Matchers::ContainsSubstring("Duplicate"));
		}
	}

	TEST_CASE("Unit|Framework|DataConnection|Blackboard support pending")
	{
		SUCCEED("Will be added once Blackboard field path support is implemented");
	}
} // namespace robotick::test
