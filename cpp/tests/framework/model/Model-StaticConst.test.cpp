// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/model/Model.h"

#include <catch2/catch_all.hpp>

using namespace robotick;

namespace
{
	struct DummyStaticModelConfig
	{
		float entry_float = 0.0f;
		bool entry_bool = false;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(DummyStaticModelConfig)
	ROBOTICK_STRUCT_FIELD(DummyStaticModelConfig, float, entry_float)
	ROBOTICK_STRUCT_FIELD(DummyStaticModelConfig, bool, entry_bool)
	ROBOTICK_REGISTER_STRUCT_END(DummyStaticModelConfig)

	struct DummyStaticModelInputs
	{
		int entry_int = 0;
		FixedString32 entry_string = "Hello";
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(DummyStaticModelInputs)
	ROBOTICK_STRUCT_FIELD(DummyStaticModelInputs, int, entry_int)
	ROBOTICK_STRUCT_FIELD(DummyStaticModelInputs, FixedString32, entry_string)
	ROBOTICK_REGISTER_STRUCT_END(DummyStaticModelInputs)

	struct DummyStaticModelWorkload
	{
		DummyStaticModelConfig config;
		DummyStaticModelInputs inputs;
	};
	ROBOTICK_REGISTER_WORKLOAD(DummyStaticModelWorkload, DummyStaticModelConfig, DummyStaticModelInputs)

	static const float s_tick_100hz(100.f);
	static const float s_tick_200hz(200.f);

	void populate_model_static_const(Model& model)
	{
		static const ConfigEntry config_entries[] = {{"entry_float", "2.0f"}, {"entry_bool", "false"}};

		static const ConfigEntry input_entries[] = {{"entry_int", "10"}, {"entry_string", "there"}};

		// Declare workloads A and B
		static const WorkloadSeed workload_a{
			TypeId("DummyStaticModelWorkload"),
			StringView("A"),
			s_tick_100hz,
			{},				// children
			config_entries, // config
			{}				// inputs
		};

		static const WorkloadSeed workload_b{
			TypeId("DummyStaticModelWorkload"),
			StringView("B"),
			s_tick_100hz,
			{},			  // children
			{},			  // config
			input_entries // inputs
		};

		// Children list (must be declared after A/B)
		static const WorkloadSeed* group_children[] = {&workload_a, &workload_b};

		// Group workload (with children)
		static const WorkloadSeed workload_group{TypeId("DummyStaticModelWorkload"), StringView("Group"), s_tick_200hz, group_children};

		// All workload pointers
		static const WorkloadSeed* all[] = {&workload_a, &workload_b, &workload_group};

		// All Data Connections
		static const DataConnectionSeed data_connection_1("A.output", "B.input");

		static const DataConnectionSeed* data_connections[] = {&data_connection_1};

		// end of static-const items ^^

		// minimal runtime items:
		model.use_workload_seeds(all);
		model.use_data_connection_seeds(data_connections);
		model.set_root_workload(workload_group);
	}

} // namespace

TEST_CASE("Unit/Framework/Model-StaticConst")
{
	SECTION("Seeds are preserved for engine use")
	{
		Model model;
		populate_model_static_const(model);

		const auto& seeds = model.get_workload_seeds();
		REQUIRE(seeds.size() == 3);

		// Find each workload by name
		const WorkloadSeed* found_a = nullptr;
		const WorkloadSeed* found_b = nullptr;
		const WorkloadSeed* found_group = nullptr;

		for (auto* seed : seeds)
		{
			if (seed->name == "A")
				found_a = seed;
			if (seed->name == "B")
				found_b = seed;
			if (seed->name == "Group")
				found_group = seed;
		}

		REQUIRE(found_a);
		REQUIRE(found_b);
		REQUIRE(found_group);

		CHECK(found_group->children.size() == 2);
		CHECK((found_group->children[0] == found_a || found_group->children[1] == found_a));
		CHECK((found_group->children[0] == found_b || found_group->children[1] == found_b));

		// Validate connection
		const auto& data_connection_seeds = model.get_data_connection_seeds();
		REQUIRE(data_connection_seeds.size() == 1);
		const auto* conn = data_connection_seeds[0];
		CHECK(conn->source_field_path == "A.output");
		CHECK(conn->dest_field_path == "B.input");

		// Validate config entries on A
		REQUIRE(found_a->config.size() == 2);
		CHECK(found_a->config[0].first == "entry_float");
		CHECK(found_a->config[0].second == "2.0f");
		CHECK(found_a->config[1].first == "entry_bool");
		CHECK(found_a->config[1].second == "false");

		// Validate input entries on B
		REQUIRE(found_b->inputs.size() == 2);
		CHECK(found_b->inputs[0].first == "entry_int");
		CHECK(found_b->inputs[0].second == "10");
		CHECK(found_b->inputs[1].first == "entry_string");
		CHECK(found_b->inputs[1].second == "there");
	}
}
