
// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/model/Model.h"

#include <catch2/catch_all.hpp>

namespace robotick::test
{
	namespace
	{
		struct CountingDataConnInputs
		{
			int input_value = 0;
		};

		ROBOTICK_REGISTER_STRUCT_BEGIN(CountingDataConnInputs)
		ROBOTICK_STRUCT_FIELD(CountingDataConnInputs, int, input_value)
		ROBOTICK_REGISTER_STRUCT_END(CountingDataConnInputs)

		struct CountingDataConnOutputs
		{
			int output_value = 0;
		};

		ROBOTICK_REGISTER_STRUCT_BEGIN(CountingDataConnOutputs)
		ROBOTICK_STRUCT_FIELD(CountingDataConnOutputs, int, output_value)
		ROBOTICK_REGISTER_STRUCT_END(CountingDataConnOutputs)

		struct CountingDataConnWorkload
		{
			CountingDataConnInputs inputs;
			CountingDataConnOutputs outputs;
			size_t tick_count = 0;

			void tick(const TickInfo&)
			{
				outputs.output_value = inputs.input_value + 1;
				tick_count++;
			}
		};

		ROBOTICK_REGISTER_WORKLOAD(CountingDataConnWorkload, void, CountingDataConnInputs, CountingDataConnOutputs)
	} // namespace

	TEST_CASE("Unit/Framework/Data/Connection-Resolving")
	{
		SECTION("ExpectedHandler set for synced group children")
		{
			Model model;
			const WorkloadSeed& a = model.add("CountingDataConnWorkload", "A").set_tick_rate_hz(10.0f);
			const WorkloadSeed& b = model.add("CountingDataConnWorkload", "B").set_tick_rate_hz(10.0f);
			model.connect("A.outputs.output_value", "B.inputs.input_value");

			const WorkloadSeed& group = model.add("SyncedGroupWorkload", "Group").set_children({&a, &b}).set_tick_rate_hz(10.0f);
			model.set_root_workload(group);

			Engine engine;
			engine.load(model);

			const auto& connections = engine.get_all_data_connections();
			bool found = false;
			for (const auto& conn : connections)
			{
				if (conn.seed.source_field_path == "A.outputs.output_value" && conn.seed.dest_field_path == "B.inputs.input_value")
				{
					CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::DelegateToParent);
					found = true;
				}
			}
			REQUIRE(found);
		}

		SECTION("ExpectedHandler set for external connections")
		{
			Model model;
			const WorkloadSeed& child1 = model.add("CountingDataConnWorkload", "Child1").set_tick_rate_hz(10.0f);
			const WorkloadSeed& child2 = model.add("CountingDataConnWorkload", "Child2").set_tick_rate_hz(10.0f);

			model.connect("Child1.outputs.output_value", "Child2.inputs.input_value");
			const WorkloadSeed& group = model.add("SyncedGroupWorkload", "Group").set_children({&child1, &child2}).set_tick_rate_hz(10.0f);
			model.set_root_workload(group);

			Engine engine;
			engine.load(model);

			const auto& connections = engine.get_all_data_connections();
			bool found = false;
			for (const auto& conn : connections)
			{
				if (conn.seed.source_field_path == "Child1.outputs.output_value" && conn.seed.dest_field_path == "Child2.inputs.input_value")
				{
					CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::DelegateToParent);
					found = true;
				}
			}
			REQUIRE(found);
		}

		SECTION("ExpectedHandler set to SequencedGroupWorkload for internal connections")
		{
			Model model;
			const WorkloadSeed& child1 = model.add("CountingDataConnWorkload", "Child1").set_tick_rate_hz(10.0f);
			const WorkloadSeed& child2 = model.add("CountingDataConnWorkload", "Child2").set_tick_rate_hz(10.0f);

			model.connect("Child1.outputs.output_value", "Child2.inputs.input_value");
			const WorkloadSeed& group = model.add("SequencedGroupWorkload", "Group").set_children({&child1, &child2}).set_tick_rate_hz(10.0f);
			model.set_root_workload(group);

			Engine engine;
			engine.load(model);

			const auto& connections = engine.get_all_data_connections();
			bool found = false;
			for (const auto& conn : connections)
			{
				if (conn.seed.source_field_path == "Child1.outputs.output_value" && conn.seed.dest_field_path == "Child2.inputs.input_value")
				{
					CHECK(conn.expected_handler == DataConnectionInfo::ExpectedHandler::SequencedGroupWorkload);
					found = true;
				}
			}
			REQUIRE(found);
		}
	}

} // namespace robotick::test