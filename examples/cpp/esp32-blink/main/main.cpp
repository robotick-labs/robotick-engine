// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace robotick;

extern "C" void app_main(void)
{
	ESP_LOGI("Robotick", "Starting Robotick ESP32 engine...");

	ESP_LOGI("Robotick", "std::atomic<bool> is lock-free? %s", std::atomic<bool>().is_lock_free() ? "yes" : "no");

	Model model;

	auto console = model.add("ConsoleTelemetryWorkload", "console", 2.0); // lower rate to reduce UART spam
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "timing_diag");

	std::vector<WorkloadHandle> children = {console, test_state_1};

	auto root = model.add("SyncedGroupWorkload", "root", children, 100.0);
	model.set_root(root);

	Engine engine;
	engine.load(model);

	static std::atomic<bool> g_stop_flag{false};
	engine.run(g_stop_flag);
	// ^- on MCU g_stop_flag is deliberately never cleared — engine runs forever unless rebooted or halted
}
