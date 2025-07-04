// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/platform/EntryPoint.h"
#include "robotick/platform/NetworkManager.h"
#include "robotick/platform/Threading.h"

#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Constants for engine task configuration
static constexpr const char* ENGINE_TASK_NAME = "robotick_main";
static constexpr uint32_t ENGINE_STACK_SIZE = 8192; // in bytes
static constexpr UBaseType_t ENGINE_TASK_PRIORITY = 5;
static constexpr BaseType_t ENGINE_CORE_ID = 1;

namespace robotick
{
	void ensure_workloads()
	{
		ROBOTICK_KEEP_WORKLOAD(BaseXWorkload)
		ROBOTICK_KEEP_WORKLOAD(ConsoleTelemetryWorkload)
		ROBOTICK_KEEP_WORKLOAD(FaceDisplayWorkload)
		ROBOTICK_KEEP_WORKLOAD(SequencedGroupWorkload)
		ROBOTICK_KEEP_WORKLOAD(SteeringMixerWorkload)
		ROBOTICK_KEEP_WORKLOAD(TimingDiagnosticsWorkload)
	}

} // namespace robotick

void populate_model(robotick::Model& model)
{
	std::vector<robotick::WorkloadHandle_v1> children = {model.add("BaseXWorkload", "basex"), model.add("ConsoleTelemetryWorkload", "console"),
		model.add("FaceDisplayWorkload", "face_display"), model.add("SteeringMixerWorkload", "steering"),
		model.add("TimingDiagnosticsWorkload", "timing")};

	auto root = model.add("SequencedGroupWorkload", "root", children, 100.0);
	model.set_root_workload(root);
}

void run_engine_on_core1(void* param)
{
	ROBOTICK_INFO("esp32-testbed - Running on CPU%d", xPortGetCoreID());

	auto* engine = static_cast<robotick::Engine*>(param);

	robotick::Model model;
	populate_model(model);

	ROBOTICK_INFO("esp32-testbed - Loading Robotick model...");
	engine->load(model); // Ensures memory locality on Core 1

	ROBOTICK_INFO("esp32-testbed - Starting tick loop...");
	robotick::AtomicFlag dummy_flag{false};
	engine->run(dummy_flag);
}

static inline void get_network_hotspot_config(robotick::NetworkHotspotConfig& hotspot_config)
{
	hotspot_config.ssid = "BARR.e";
	hotspot_config.password = "tortoise123";
	hotspot_config.iface = "wlp88s0f0";
}

ROBOTICK_ENTRYPOINT
{
	ROBOTICK_INFO("esp32-testbed - Started on CPU%d", xPortGetCoreID());

	robotick::ensure_workloads();

	// connect to our local wifi-hotspot (hard-coded creds for now) - needs security pass later:
	robotick::NetworkHotspotConfig hotspot_config;
	get_network_hotspot_config(hotspot_config);

	robotick::NetworkClientConfig client_config;
	client_config.type = hotspot_config.type;
	client_config.iface = "wlan0"; // TBC - may not even be needed on ESP?
	client_config.ssid = hotspot_config.ssid;
	client_config.password = hotspot_config.password;

	ROBOTICK_INFO("==============================================================\n");
	ROBOTICK_INFO("BARR.e Brain - connecting to wifi hotspot...");
	const bool hotspot_success = robotick::NetworkClient::connect(client_config);
	if (!hotspot_success)
	{
		ROBOTICK_FATAL_EXIT("BARR.e Brain - Failed to connect to wifi-hotspot!");
	}
	ROBOTICK_INFO("\n");
	ROBOTICK_INFO("==============================================================\n");

	static robotick::Engine engine;

	ROBOTICK_INFO("esp32-testbed - Launching Robotick engine task on core 1...");

	xTaskCreatePinnedToCore(run_engine_on_core1, ENGINE_TASK_NAME, ENGINE_STACK_SIZE, &engine, ENGINE_TASK_PRIORITY, nullptr, ENGINE_CORE_ID);
}
