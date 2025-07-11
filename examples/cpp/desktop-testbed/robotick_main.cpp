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

void populate_model_hello_world(robotick::Model& model)
{
	using namespace robotick;

	const WorkloadSeed& console = model.add("ConsoleTelemetryWorkload", "console").set_tick_rate_hz(5.0f);

	const WorkloadSeed& test_state_1 = model.add("TimingDiagnosticsWorkload", "test_state_1");

	const WorkloadSeed& test_state_2 =
		model.add("PythonWorkload", "test_state_2")
			.set_tick_rate_hz(1.0f)
			.set_config({{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});

	const WorkloadSeed& root =
		model.add("SyncedGroupWorkload", "root_group").set_children({&console, &test_state_1, &test_state_2}).set_tick_rate_hz(1000.0f);

	model.set_root_workload(root);
}

void populate_model_hello_mqtt(robotick::Model& model)
{
	using namespace robotick;

	const WorkloadSeed& remote_control = model.add("RemoteControlWorkload", "remote_control").set_tick_rate_hz(30.0f);

	const WorkloadSeed& mqtt_client =
		model.add("MqttClientWorkload", "mqtt_client").set_tick_rate_hz(30.0f).set_config({{"broker_url", "mqtt://192.168.5.14"}});

	const WorkloadSeed& console_telem = model.add("ConsoleTelemetryWorkload", "console").set_tick_rate_hz(5.0f);

	const WorkloadSeed& root =
		model.add("SyncedGroupWorkload", "root_group").set_children({&console_telem, &remote_control, &mqtt_client}).set_tick_rate_hz(30.0f);

	model.set_root_workload(root);
}

ROBOTICK_ENTRYPOINT
{
	robotick::setup_exit_handler(signal_handler);

	robotick::Model model;
	populate_model_hello_world(model);

	robotick::Engine engine;
	engine.load(model);
	engine.run(g_stop_flag);

#if !defined(ROBOTICK_PLATFORM_ESP32)
	return 0;
#endif
}