
// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "utils/EngineInspector.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		struct CountingDataConnInputs
		{
			int input_value = 0;
		};

		ROBOTICK_BEGIN_FIELDS(CountingDataConnInputs)
		ROBOTICK_FIELD(CountingDataConnInputs, input_value)
		ROBOTICK_END_FIELDS()

		struct CountingDataConnOutputs
		{
			int output_value = 0;
		};

		ROBOTICK_BEGIN_FIELDS(CountingDataConnOutputs)
		ROBOTICK_FIELD(CountingDataConnOutputs, output_value)
		ROBOTICK_END_FIELDS()

		struct CountingDataConnWorkload
		{
			CountingDataConnInputs inputs;
			CountingDataConnOutputs outputs;
			size_t tick_count = 0;

			void tick(double)
			{
				outputs.output_value = inputs.input_value + 1;
				tick_count++;
			}
		};

		ROBOTICK_DEFINE_WORKLOAD(CountingDataConnWorkload)
	} // namespace

	TEST_CASE("Unit|Framework|DataConnections|ExpectedHandler set for synced group children")
	{
		Model model;
		auto a = model.add("CountingDataConnWorkload", "A", 10.0);
		auto b = model.add("CountingDataConnWorkload", "B", 10.0);
		model.connect("A.outputs.output_value", "B.inputs.input_value");

		auto group = model.add("SyncedGroupWorkload", "Group", {a, b}, 10.0);
		model.set_root(group);

		Engine engine;
		engine.load(model);

		const auto& connections = EngineInspector::get_all_data_connections(engine);
		bool found = false;
		for (const auto& conn : connections)
		{
			if (conn.seed.source_field_path == "A.outputs.output_value" && conn.seed.dest_field_path == "B.inputs.input_value")
			{
				CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine);
				found = true;
			}
		}
		REQUIRE(found);
	}

	TEST_CASE("Unit|Framework|DataConnections|ExpectedHandler set for external connections")
	{
		Model model;
		auto child1 = model.add("CountingDataConnWorkload", "Child1", 10.0);
		auto child2 = model.add("CountingDataConnWorkload", "Child2", 10.0);

		model.connect("Child1.outputs.output_value", "Child2.inputs.input_value");
		auto group = model.add("SyncedGroupWorkload", "Group", {child1, child2}, 10.0);
		model.set_root(group);

		Engine engine;
		engine.load(model);

		const auto& connections = EngineInspector::get_all_data_connections(engine);
		bool found = false;
		for (const auto& conn : connections)
		{
			if (conn.seed.source_field_path == "Child1.outputs.output_value" && conn.seed.dest_field_path == "Child2.inputs.input_value")
			{
				CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine);
				found = true;
			}
		}
		REQUIRE(found);
	}

	TEST_CASE("Unit|Framework|DataConnections|ExpectedHandler not set for internal connections")
	{
		Model model;
		auto child1 = model.add("CountingDataConnWorkload", "Child1", 10.0);
		auto child2 = model.add("CountingDataConnWorkload", "Child2", 10.0);

		model.connect("Child1.outputs.output_value", "Child2.inputs.input_value");
		auto group = model.add("SequencedGroupWorkload", "Group", {child1, child2}, 10.0);
		model.set_root(group);

		Engine engine;
		engine.load(model);

		const auto& connections = EngineInspector::get_all_data_connections(engine);
		bool found = false;
		for (const auto& conn : connections)
		{
			if (conn.seed.source_field_path == "Child1.outputs.output_value" && conn.seed.dest_field_path == "Child2.inputs.input_value")
			{
				CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::NotSet);
				found = true;
			}
		}
		REQUIRE(found);
	}

} // namespace robotick