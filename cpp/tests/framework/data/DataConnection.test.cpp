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
				: out_blackboard({BlackboardFieldInfo("x", std::type_index(typeid(int))), BlackboardFieldInfo("y", std::type_index(typeid(double)))})
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

			DummyBInput()
				: in_blackboard({BlackboardFieldInfo("x", std::type_index(typeid(int))), BlackboardFieldInfo("y", std::type_index(typeid(double)))})
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

	TEST_CASE("Unit|Framework|Data|Connection|Resolves non-blackboard to non-blackboard")
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
			{"A.outputs.x", "B.inputs.x"},
			{"A.outputs.y", "B.inputs.y"},
		};

		std::vector<DataConnectionInfo> resolved =
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.do_data_copy();
		}

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, handle_b.index);
		REQUIRE(b->inputs.x == 42);
		REQUIRE(b->inputs.y == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|Data|Connection|Resolves non-blackboard to blackboard")
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

		std::vector<DataConnectionInfo> resolved =
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine));
		REQUIRE(resolved.size() == 2);

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, handle_b.index);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.do_data_copy();
		}

		REQUIRE(b->inputs.in_blackboard.get<int>("x") == 42);
		REQUIRE(b->inputs.in_blackboard.get<double>("y") == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|Data|Connection|Resolves blackboard to non-blackboard")
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

		std::vector<DataConnectionInfo> resolved =
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.do_data_copy();
		}

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, 1);
		REQUIRE(b->inputs.x == 42);
		REQUIRE(b->inputs.y == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|Data|Connection|Resolves blackboard to blackboard")
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

		std::vector<DataConnectionInfo> resolved =
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);

		// Execute copy
		for (const auto& conn : resolved)
		{
			conn.do_data_copy();
		}

		const DummyB* b = EngineInspector::get_instance<DummyB>(engine, 1);

		REQUIRE(b->inputs.in_blackboard.get<int>("x") == 42);
		REQUIRE(b->inputs.in_blackboard.get<double>("y") == Catch::Approx(3.14));
	}

	TEST_CASE("Unit|Framework|Data|Connection|Errors on invalid connections")
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
			REQUIRE_THROWS_WITH(
				DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, infos), Catch::Matchers::ContainsSubstring("Z"));
		}

		SECTION("Invalid section")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.wrong.x", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, infos),
				Catch::Matchers::ContainsSubstring("Invalid section"));
		}

		SECTION("Missing field")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.outputs.missing", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, infos),
				Catch::Matchers::ContainsSubstring("field"));
		}

		SECTION("Mismatched types")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.outputs.x", "B.inputs.y"}}; // int -> double
			REQUIRE_THROWS_WITH(DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, infos),
				Catch::Matchers::ContainsSubstring("Type mismatch"));
		}

		SECTION("Duplicate destination")
		{
			std::vector<DataConnectionSeed> seeds = {{"A.outputs.x", "B.inputs.x"}, {"A.outputs.x", "B.inputs.x"}};
			REQUIRE_THROWS_WITH(DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, infos),
				Catch::Matchers::ContainsSubstring("Duplicate"));
		}
	}

	TEST_CASE("Unit|Framework|Data|Connection|Blackboard support pending")
	{
		SUCCEED("Will be added once Blackboard field path support is implemented");
	}

	TEST_CASE("Unit|Framework|Data|Connection|Unidirectional copy")
	{
		Model model;
		const WorkloadHandle handle_a = model.add("DummyA", "A", 1.0);
		const WorkloadHandle handle_b = model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		auto* a = EngineInspector::get_instance<DummyA>(engine, handle_a.index);
		auto* b = EngineInspector::get_instance<DummyB>(engine, handle_b.index);

		a->outputs.x = 123;
		b->inputs.x = 999; // Should get overwritten

		std::vector<DataConnectionSeed> seeds = {{"A.outputs.x", "B.inputs.x"}};
		std::vector<DataConnectionInfo> resolved =
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 1);
		resolved[0].do_data_copy();

		REQUIRE(b->inputs.x == 123);
		REQUIRE(a->outputs.x == 123); // Confirm unmodified
	}

	TEST_CASE("Unit|Framework|Data|Connection|Throws for blackboard subfield not found")
	{
		Model model;
		model.add("DummyA", "A", 1.0);
		model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		std::vector<DataConnectionSeed> seeds = {{"A.outputs.out_blackboard.missing", "B.inputs.in_blackboard.x"}};

		REQUIRE_THROWS_WITH(
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine)),
			Catch::Matchers::ContainsSubstring("subfield"));
	}

	TEST_CASE("Unit|Framework|Data|Connection|Different subfields allowed")
	{
		Model model;
		model.add("DummyA", "A", 1.0);
		model.add("DummyB", "B", 1.0);
		model_helpers::wrap_all_in_sequenced_group(model);

		Engine engine;
		engine.load(model);

		std::vector<DataConnectionSeed> seeds = {
			{"A.outputs.out_blackboard.x", "B.inputs.in_blackboard.x"},
			{"A.outputs.out_blackboard.y", "B.inputs.in_blackboard.y"},
		};

		std::vector<DataConnectionInfo> resolved =
			DataConnectionsFactory::create(EngineInspector::get_workloads_buffer(engine), seeds, EngineInspector::get_all_instance_info(engine));

		REQUIRE(resolved.size() == 2);
	}

} // namespace robotick::test
