// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/model/WorkloadSeed.h"
#include "robotick/platform/EntryPoint.h"
#include "robotick/platform/Signals.h"
#include "robotick/platform/Threading.h"

robotick::AtomicFlag g_stop_flag;

void signal_handler()
{
	g_stop_flag.set();
}

void populate_model_hello_world_load(robotick::Model& model)
{
	model.load("hello_world.yaml");
}

void populate_model_hello_world_dynamic(robotick::Model& model)
{
	auto& console = model.add("ConsoleTelemetryWorkload", "console").set_tick_rate_hz(5.0f);
	auto& test_state_1 = model.add("TimingDiagnosticsWorkload", "test_state_1");
	auto& test_state_2 = model.add("PythonWorkload", "test_state_2")
							 .set_tick_rate_hz(1.0f)
							 .set_config({{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});

	auto& root = model.add("SyncedGroupWorkload", "root_group").set_tick_rate_hz(1000.0f).set_children({&console, &test_state_1, &test_state_2});

	model.set_root_workload(root);
}

void populate_model_hello_world_constexpr(Model& model)
{
	static constexpr robotick::FieldConfigEntry test_state_2_config[] = {
		{"script_name", "robotick.workloads.optional.test.hello_workload"},
		{"class_name", "HelloWorkload"},
	};

	static constexpr robotick::WorkloadSeed console{GET_TYPE_ID("ConsoleTelemetryWorkload"), "console", 5.0f};

	static constexpr robotick::WorkloadSeed test_state_1{
		GET_TYPE_ID("TimingDiagnosticsWorkload"), "test_state_1"
		// default tick_rate_hz = TICK_RATE_FROM_PARENT
	};

	static constexpr robotick::WorkloadSeed test_state_2{GET_TYPE_ID("PythonWorkload"), "test_state_2", 1.0f, nullptr, 0, test_state_2_config, 2};

	static constexpr const robotick::WorkloadSeed* children[] = {&console, &test_state_1, &test_state_2};

	static constexpr robotick::WorkloadSeed root{GET_TYPE_ID("SyncedGroupWorkload"), "root_group", 1000.0f, children, 3};

	static constexpr const robotick::WorkloadSeed* all_workloads[] = {
		&console,
		&test_state_1,
		&test_state_2,
		&root,
	};

	model.set_workloads(all_workloads, robotick::arr_size(all_workloads));

	model.set_root_workload(root, /*auto_finalize=*/true);
}

void populate_model_hello_world_constexpr_macro(robotick::Model& model)
{
	ROBOTICK_MODEL_CONFIG_ENTRIES(test_state_2_config[]) = {
		{"script_name", "robotick.workloads.optional.test.hello_workload"},
		{"class_name", "HelloWorkload"},
	};

	ROBOTICK_MODEL_WORKLOAD(console, "ConsoleTelemetryWorkload", 5.0f);
	ROBOTICK_MODEL_WORKLOAD(test_state_1, "TimingDiagnosticsWorkload", robotick::Model::TICK_RATE_FROM_PARENT);
	ROBOTICK_MODEL_WORKLOAD(test_state_2, "PythonWorkload", 1.0f, test_state_2_config, 2);

	static constexpr const robotick::WorkloadSeed* children[] = {&console, &test_state_1, &test_state_2};

	ROBOTICK_MODEL_WORKLOAD_WITH_CHILDREN(root, "SyncedGroupWorkload", 1000.0f, children, 3);

	static constexpr const robotick::WorkloadSeed* all_workloads[] = {
		&console,
		&test_state_1,
		&test_state_2,
		&root,
	};

	model.set_workloads(all_workloads, arr_size(all_workloads));

	model.set_root_workload(root, /*auto_finalize=*/true);
}

ROBOTICK_ENTRYPOINT
{
	robotick::setup_exit_handler(signal_handler);

	robotick::Model model_load;
	populate_model_hello_world_load(model_load);

	robotick::Model model_dynamic;
	populate_model_hello_world_dynamic(model_dynamic);

	robotick::Model model_constexpr;
	populate_model_hello_world_constexpr(model_constexpr);

	robotick::Model model_constexpr_macro;
	populate_model_hello_world_constexpr_macro(model_constexpr_macro);

	const robotick::Model& model = model_dynamic;

	robotick::Engine engine;
	engine.load(model);
	engine.run(g_stop_flag);

#if !defined(ROBOTICK_PLATFORM_ESP32)
	return 0;
#endif
}