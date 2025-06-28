// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model_v1.h"
#include "robotick/platform/EntryPoint.h"
#include "robotick/platform/Signals.h"
#include "robotick/platform/Threading.h"

robotick::AtomicFlag g_stop_flag;

void signal_handler()
{
	g_stop_flag.set();
}

void populate_model_hello_world(robotick::Model_v1& model)
{
	auto console = model.add("ConsoleTelemetryWorkload", "console", 5.0);
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "test_state_1");
	auto test_state_2 = model.add(
		"PythonWorkload", "test_state_2", 1.0, {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});

	std::vector<robotick::WorkloadHandle> children = {console, test_state_1, test_state_2};
	auto root = model.add("SyncedGroupWorkload", "root_group", children, 1000.0);
	model.set_root(root);
}

void populate_model_hello_mqtt(robotick::Model_v1& model)
{
	auto remote_control = model.add("RemoteControlWorkload", "remote_control", 30.0);
	auto mqtt_client = model.add("MqttClientWorkload", "mqtt_client", 30.0, {{"broker_url", "mqtt://192.168.5.14"}});
	auto console_telem = model.add("ConsoleTelemetryWorkload", "console", 5.0);

	std::vector<robotick::WorkloadHandle> children = {console_telem, remote_control, mqtt_client};
	auto root = model.add("SyncedGroupWorkload", "root_group", children, 30.0);
	model.set_root(root);
}

ROBOTICK_ENTRYPOINT
{
	robotick::setup_exit_handler(signal_handler);

	robotick::Model_v1 model;
	populate_model_hello_mqtt(model);

	robotick::Engine engine;
	engine.load(model);
	engine.run(g_stop_flag);

#if !defined(ROBOTICK_PLATFORM_ESP32)
	return 0;
#endif
}