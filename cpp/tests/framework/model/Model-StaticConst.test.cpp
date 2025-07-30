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

	struct DummyStaticModelOutputs
	{
		int entry_int_1 = 0;
		int entry_int_2 = 0;
	};
	ROBOTICK_REGISTER_STRUCT_BEGIN(DummyStaticModelOutputs)
	ROBOTICK_STRUCT_FIELD(DummyStaticModelOutputs, int, entry_int_1)
	ROBOTICK_STRUCT_FIELD(DummyStaticModelOutputs, int, entry_int_2)
	ROBOTICK_REGISTER_STRUCT_END(DummyStaticModelOutputs)

	struct DummyStaticModelWorkload
	{
		DummyStaticModelConfig config;
		DummyStaticModelInputs inputs;
		DummyStaticModelOutputs outputs;
	};
	ROBOTICK_REGISTER_WORKLOAD(DummyStaticModelWorkload, DummyStaticModelConfig, DummyStaticModelInputs, DummyStaticModelOutputs)

	static const float s_tick_100hz(100.f);
	static const float s_tick_200hz(200.f);

	void populate_model_static_const(Model& model)
	{
		static const FieldConfigEntry config_entries[] = {{"entry_float", "2.0f"}, {"entry_bool", "false"}};

		static const FieldConfigEntry input_entries[] = {{"entry_int", "10"}, {"entry_string", "there"}};

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
		static const DataConnectionSeed data_connection_1("A.outputs.entry_int_1", "B.inputs.entry_int");

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
	SECTION("Rejects unknown workload type")
	{
		// Declare workload with unknown type
		static const WorkloadSeed workload_unknown{
			TypeId("UnknownType"), // deliberately unknown type
			StringView("fail"),	   // workload name
			s_tick_100hz,		   // tick rate
			{},					   // no children
			{},					   // no config
			{}					   // no inputs
		};

		// Updated all workload pointers to include it
		static const WorkloadSeed* all[] = {&workload_unknown};

		Model model;
		ROBOTICK_REQUIRE_ERROR_MSG(model.use_workload_seeds(all), "Unable to find workload type");
	}

	SECTION("Throws if child tick rate faster than parent")
	{
		static const WorkloadSeed child{TypeId("DummyStaticModelWorkload"), "A", s_tick_200hz, {}, {}, {}};

		static const WorkloadSeed* children[] = {&child};

		static const WorkloadSeed parent{TypeId("DummyStaticModelWorkload"), "Parent", s_tick_100hz, children, {}, {}};

		static const WorkloadSeed* all[] = {&child, &parent};

		Model model;
		model.use_workload_seeds(all);

		const bool auto_finalize = false;
		model.set_root_workload(parent, auto_finalize);

		ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "faster tick rate");
	}

	SECTION("Duplicate data connection throws")
	{
		static const WorkloadSeed a{TypeId("DummyStaticModelWorkload"), "A", s_tick_100hz, {}, {}, {}};
		static const WorkloadSeed b{TypeId("DummyStaticModelWorkload"), "B", s_tick_100hz, {}, {}, {}};

		static const WorkloadSeed* all[] = {&a, &b};

		static const DataConnectionSeed dc1{"A.outputs.entry_int_1", "B.inputs.entry_int"};
		static const DataConnectionSeed dc2{"A.outputs.entry_int_2", "B.inputs.entry_int"};
		static const DataConnectionSeed* connections[] = {&dc1, &dc2};

		Model model;
		model.use_workload_seeds(all);
		model.use_data_connection_seeds(connections);

		const bool auto_finalize = false;
		model.set_root_workload(a, auto_finalize);

		ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "already has an incoming connection");
	}

	SECTION("Allows connecting input between valid workloads")
	{
		static const WorkloadSeed a{TypeId("DummyStaticModelWorkload"), "A", s_tick_100hz, {}, {}, {}};
		static const WorkloadSeed b{TypeId("DummyStaticModelWorkload"), "B", s_tick_100hz, {}, {}, {}};
		static const WorkloadSeed* children[] = {&a, &b};
		static const WorkloadSeed group{TypeId("DummyStaticModelWorkload"), "Group", s_tick_100hz, children, {}, {}};

		static const WorkloadSeed* all[] = {&a, &b, &group};

		static const DataConnectionSeed dc{"A.outputs.entry_int_1", "B.inputs.entry_int"};
		static const DataConnectionSeed* dcs[] = {&dc};

		Model model;
		model.use_workload_seeds(all);
		model.use_data_connection_seeds(dcs);

		const bool auto_finalize = false;
		model.set_root_workload(group, auto_finalize);

		REQUIRE_NOTHROW(model.finalize());
	}

	SECTION("Throws when static destination field is not to inputs")
	{
		static const WorkloadSeed a{TypeId("DummyStaticModelWorkload"), "A", s_tick_100hz, {}, {}, {}};
		static const WorkloadSeed b{TypeId("DummyStaticModelWorkload"), "B", s_tick_100hz, {}, {}, {}};

		static const WorkloadSeed* all[] = {&a, &b};

		static const DataConnectionSeed bad_dst_1{"A.outputs.entry_int", "B.config.entry_float"};
		static const DataConnectionSeed bad_dst_2{"A.outputs.entry_int", "B.outputs.entry_int"};

		static const DataConnectionSeed* dcs[] = {&bad_dst_1, &bad_dst_2};

		Model model;
		model.use_workload_seeds(all);
		model.use_data_connection_seeds(dcs);

		const bool auto_finalize = false;
		model.set_root_workload(b, auto_finalize);

		ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "must use the 'inputs' structure");
	}

	SECTION("Throws when static source field is not from outputs")
	{
		static const WorkloadSeed a{TypeId("DummyStaticModelWorkload"), "A", s_tick_100hz, {}, {}, {}};
		static const WorkloadSeed b{TypeId("DummyStaticModelWorkload"), "B", s_tick_100hz, {}, {}, {}};

		static const WorkloadSeed* all[] = {&a, &b};

		static const DataConnectionSeed bad_src_1{"A.config.entry_float", "B.inputs.entry_int"};
		static const DataConnectionSeed bad_src_2{"A.inputs.entry_int", "B.inputs.entry_int"};

		static const DataConnectionSeed* dcs[] = {&bad_src_1, &bad_src_2};

		Model model;
		model.use_workload_seeds(all);
		model.use_data_connection_seeds(dcs);

		const bool auto_finalize = false;
		model.set_root_workload(a, auto_finalize);

		ROBOTICK_REQUIRE_ERROR_MSG(model.finalize(), "must use the 'outputs' structure");
	}

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
			if (seed->unique_name == "A")
				found_a = seed;
			if (seed->unique_name == "B")
				found_b = seed;
			if (seed->unique_name == "Group")
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
		CHECK(conn->source_field_path == "A.outputs.entry_int_1");
		CHECK(conn->dest_field_path == "B.inputs.entry_int");

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
