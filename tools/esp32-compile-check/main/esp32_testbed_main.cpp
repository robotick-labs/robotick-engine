// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/system/EntryPoint.h"
#include "robotick/framework/services/NetworkManager.h"
#include "robotick/framework/concurrency/Atomic.h"

#include <M5Unified.h>

#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Constants for engine task configuration
static constexpr const char* ENGINE_TASK_NAME = "robotick_main";
static constexpr uint32_t ENGINE_STACK_SIZE = 8192; // in bytes
static constexpr UBaseType_t ENGINE_TASK_PRIORITY = 5;
static constexpr BaseType_t ENGINE_CORE_ID = 1;

// no need to "ensure" the workloads - they won't exist here anyway - we just want to test esp32 compiles with this build

void populate_model(robotick::Model& model)
{
	const float tick_rate_hz_main = 20.0f;

	// Functional workloads:
	static const robotick::WorkloadSeed heart_display{
		robotick::TypeId("HeartbeatDisplayWorkload"), robotick::StringView("heart_display"), tick_rate_hz_main};

	static const robotick::WorkloadSeed imu{robotick::TypeId("ImuWorkload"), robotick::StringView("imu"), tick_rate_hz_main};
	static const robotick::WorkloadSeed steering{robotick::TypeId("SteeringMixerWorkload"), robotick::StringView("steering"), tick_rate_hz_main};
	static const robotick::WorkloadSeed basex{robotick::TypeId("BaseXWorkload"), robotick::StringView("basex"), tick_rate_hz_main};

	// Root group
	static const robotick::WorkloadSeed* root_children[] = {&heart_display, &imu, &steering, &basex};
	static const robotick::WorkloadSeed root{
		robotick::TypeId("SequencedGroupWorkload"), robotick::StringView("root"), tick_rate_hz_main, root_children};

	// All workloads
	static const robotick::WorkloadSeed* all[] = {&heart_display, &imu, &steering, &basex, &root};

	// Final registration
	model.use_workload_seeds(all);
	model.set_root_workload(root);
}

void run_engine_on_core1(void* param)
{
	ROBOTICK_INFO("esp32-testbed - Running on CPU%d", xPortGetCoreID());

	robotick::Model model;
	populate_model(model);

	ROBOTICK_INFO("esp32-testbed - Loading Robotick model...");

	robotick::Engine engine;
	engine.load(model); // Ensures memory locality on Core 1

	ROBOTICK_INFO("esp32-testbed - Starting tick loop...");
	robotick::AtomicFlag dummy_flag{false};
	engine.run(dummy_flag);
}

static inline void get_network_hotspot_config(robotick::NetworkHotspotConfig& hotspot_config)
{
	hotspot_config.ssid = "Robotick";
	hotspot_config.password = "robotick123";
	hotspot_config.iface = "wlp88s0f0";

	// or run manually: sudo nmcli dev wifi hotspot ifname wlan0 ssid Robotick password robotick123
	// or look up wifi device name (E.g. wlp88s0f0) using nmcli device status
	// e.g. sudo nmcli dev wifi hotspot ifname wlp88s0f0 ssid Robotick password robotick123
}

static void connect_to_wifi_hotspot()
{
	// connect to our local wifi-hotspot while the engine's spinning up (hard-coded creds for now) - needs security pass later:
	robotick::NetworkHotspotConfig hotspot_config;
	get_network_hotspot_config(hotspot_config);

	robotick::NetworkClientConfig client_config;
	client_config.type = hotspot_config.type;
	client_config.iface = "wlan0"; // TBC - may not even be needed on ESP?
	client_config.ssid = hotspot_config.ssid;
	client_config.password = hotspot_config.password;

	const bool hotspot_success = robotick::NetworkClient::connect(client_config);
	if (hotspot_success)
	{
		ROBOTICK_INFO("esp32-testbed  - successfully connected to wifi hotspot!");
	}
	else
	{
		ROBOTICK_WARNING("esp32-testbed  - Failed to connect to wifi-hotspot!");
	}
	ROBOTICK_INFO("\n");
}

ROBOTICK_ENTRYPOINT
{
	ROBOTICK_INFO("esp32-testbed - Started on CPU%d", xPortGetCoreID());

	ROBOTICK_INFO("esp32-testbed  - connecting to wifi hotspot...");
	connect_to_wifi_hotspot();

	ROBOTICK_INFO("esp32-testbed - Launching Robotick engine task on core 1...");

	xTaskCreatePinnedToCore(run_engine_on_core1, ENGINE_TASK_NAME, ENGINE_STACK_SIZE, nullptr, ENGINE_TASK_PRIORITY, nullptr, ENGINE_CORE_ID);
}
