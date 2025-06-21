// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
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
	auto console = model.add("ConsoleTelemetryWorkload", "console", 5.0);
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "test_state_1");
	auto test_state_2 = model.add(
		"PythonWorkload", "test_state_2", 1.0, {{"script_name", "robotick.workloads.optional.test.hello_workload"}, {"class_name", "HelloWorkload"}});

	std::vector<robotick::WorkloadHandle> children = {console, test_state_1, test_state_2};
	auto root = model.add("SyncedGroupWorkload", "root_group", children, 1000.0);
	model.set_root(root);
}

void populate_model_hello_mqtt(robotick::Model& model)
{
	auto remote_control = model.add("RemoteControlWorkload", "remote_control", 30.0);
	auto mqtt_client = model.add("MqttClientWorkload", "mqtt_client", 30.0, {{"broker_url", "mqtt://192.168.5.14"}});
	auto console_telem = model.add("ConsoleTelemetryWorkload", "console", 5.0);

	std::vector<robotick::WorkloadHandle> children = {console_telem, remote_control, mqtt_client};
	auto root = model.add("SyncedGroupWorkload", "root_group", children, 30.0);
	model.set_root(root);
}

#ifdef ENABLE_MODEL_1

void populate_model_hello_mqtt_alt0(robotick::Model& model)
{
	model.load("hello_mqtt.yaml");
}

void populate_model_hello_mqtt_alt1(robotick::Model& model)
{
	auto remote_control = model.create("RemoteControlWorkload", "remote_control")
	                          .set_tick_rate_hz(30.0);

	auto mqtt_client = model.create("MqttClientWorkload", "mqtt_client")
	                        .set_tick_rate_hz(30.0)
	                        .create_config({{"broker_url", "mqtt://192.168.5.14"}});

	auto console_telem = model.create("ConsoleTelemetryWorkload", "console")
	                          .set_tick_rate_hz(5.0);

	HeapVector<robotick::WorkloadHandle> children = {console_telem, remote_control, mqtt_client};

	auto root = model.create("SyncedGroupWorkload", "root_group")
	                 .set_tick_rate_hz(30.0)
	                 .create_children(children);

	model.set_root_workload(root);
}

void populate_model_hello_mqtt_alt2(robotick::Model& model)
{
	static WorkloadInfo remote_control("RemoteControlWorkload", "remote_control");
	static WorkloadInfo mqtt_client("MqttClientWorkload", "mqtt_client");
	static WorkloadInfo console("ConsoleTelemetryWorkload", "console");
	static WorkloadInfo root_group("SyncedGroupWorkload", "root_group");

	static const char* mqtt_config[][2] = {
		{"broker_url", "mqtt://192.168.5.14"}
	};

	remote_control.set_tick_rate_hz(30.0);
	mqtt_client.set_tick_rate_hz(30.0)
	           .use_config(mqtt_config, arr_size(mqtt_config));
	console.set_tick_rate_hz(5.0);

	static WorkloadInfo* children[] = {&remote_control, &mqtt_client, &console};
	static WorkloadInfo* all_workloads[] = {&remote_control, &mqtt_client, &console, &root_group};

	root_group.use_children(children, arr_size(children));

	model.use_workloads(all_workloads, arr_size(all_workloads));
	model.set_root_workload(root_group);
}

#endif // #ifdef ENABLE_MODEL_1

ROBOTICK_ENTRYPOINT
{
	robotick::setup_exit_handler(signal_handler);

	robotick::Model model;
	populate_model_hello_mqtt(model);

	robotick::Engine engine;
	engine.load(model);
	engine.run(g_stop_flag);

#if !defined(ROBOTICK_PLATFORM_ESP32)
	return 0;
#endif
}