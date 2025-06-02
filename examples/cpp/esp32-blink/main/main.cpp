// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robotick
{
	void ensure_workloads()
	{
		ROBOTICK_KEEP_WORKLOAD(ConsoleTelemetryWorkload)
		ROBOTICK_KEEP_WORKLOAD(TimingDiagnosticsWorkload)
		ROBOTICK_KEEP_WORKLOAD(SyncedGroupWorkload)
	}
} // namespace robotick

extern "C" void app_main(void)
{
	robotick::ensure_workloads();

	ESP_LOGI("Robotick", "Starting Robotick ESP32 engine...");

	ESP_LOGI("Robotick", "std::atomic<bool> is lock-free? %s", std::atomic<bool>().is_lock_free() ? "yes" : "no");

	robotick::Model model;

	auto console = model.add("ConsoleTelemetryWorkload", "console", 2.0); // lower rate to reduce UART spam
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "timing_diag");

	std::vector<robotick::WorkloadHandle> children = {console, test_state_1};

	auto root = model.add("SyncedGroupWorkload", "root", children, 100.0);
	model.set_root(root);

	robotick::Engine engine;
	engine.load(model);

	AtomicFlag stop_after_next_tick_flag{false};
	engine.run(stop_after_next_tick_flag);
	// ^- on MCU g_stop_flag is deliberately never cleared — engine runs forever unless rebooted or halted
}
