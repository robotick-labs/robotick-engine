// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/platform/EntryPoint.h"
#include "robotick/platform/Signals.h"
#include "robotick/platform/Threading.h"

using namespace robotick;

AtomicFlag g_stop_flag;

void signal_handler()
{
	g_stop_flag.set();
}

void populate_model(Model& model)
{
	auto console = model.add("ConsoleTelemetryWorkload", "console", 10.0);
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "test_state_1");
	auto test_state_2 = model.add(
		"PythonWorkload", "test_state_2", 1.0, {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});

	std::vector<WorkloadHandle> children = {console, test_state_1, test_state_2};
	auto root = model.add("SyncedGroupWorkload", "root_group", children, 1000.0);
	model.set_root(root);
}

ROBOTICK_ENTRYPOINT
{
	setup_exit_handler(signal_handler);

	Model model;
	populate_model(model);

	Engine engine;
	engine.load(model);
	engine.run(g_stop_flag);

#if !defined(ROBOTICK_PLATFORM_ESP32)
	return 0;
#endif
}