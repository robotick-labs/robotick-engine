
// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "utils/EngineInspector.h"

#include <catch2/catch_all.hpp>

using namespace robotick;
using namespace robotick::test;

TEST_CASE("Unit|Framework|DataConnections|ExpectedHandler set for child destination")
{
	Model model;
	auto a = model.add("CountingWorkload", "A", 10.0);
	auto b = model.add("CountingWorkload", "B", 10.0);
	model.connect("A.value", "B.input");

	auto group = model.add("SyncedGroupWorkload", "Group", {a, b}, 10.0);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& connections = EngineInspector::get_all_data_connections(engine);
	bool found = false;
	for (const auto& conn : connections)
	{
		if (conn.seed.source_field_path == "A.value" && conn.seed.dest_field_path == "B.input")
		{
			CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine);
			found = true;
		}
	}
	REQUIRE(found);
}

TEST_CASE("Unit|Framework|DataConnections|ExpectedHandler not set for external connections")
{
	Model model;
	auto child1 = model.add("CountingWorkload", "Child1", 10.0);
	auto child2 = model.add("CountingWorkload", "Child2", 10.0);

	model.connect("Child1.out", "Child2.in");
	auto group = model.add("SyncedGroupWorkload", "Group", {child1, child2}, 10.0);
	model.set_root(group);

	Engine engine;
	engine.load(model);

	const auto& connections = EngineInspector::get_all_data_connections(engine);
	bool found = false;
	for (const auto& conn : connections)
	{
		if (conn.seed.source_field_path == "Child1.out" && conn.seed.dest_field_path == "Child2.in")
		{
			CHECK(conn.expected_handler != DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine);
			found = true;
		}
	}
	REQUIRE(found);
}
