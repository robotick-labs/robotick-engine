// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/platform/Threading.h"

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

void create_threaded_model(robotick::Model& model)
{
	auto console = model.add("ConsoleTelemetryWorkload", "console", 2.0); // lower rate to reduce UART spam
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "timing_diag");

	std::vector<robotick::WorkloadHandle> children = {console, test_state_1};

	auto root = model.add("SyncedGroupWorkload", "root", children, 100.0);
	model.set_root(root);
}

void create_non_threaded_model(robotick::Model& model)
{
	auto console = model.add("ConsoleTelemetryWorkload", "console", 2.0); // lower rate to reduce UART spam
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "timing_diag");

	std::vector<robotick::WorkloadHandle> children = {console, test_state_1};

	auto root = model.add("SequencedGroupWorkload", "root", children, 100.0);
	model.set_root(root);
}

void create_simple_model(robotick::Model& model)
{
	auto root = model.add("TimingDiagnosticsWorkload", "timing_diag", 100.0);
	model.set_root(root);
}

extern "C" void app_main(void)
{
	robotick::ensure_workloads();

	ESP_LOGI("Robotick", "Starting Robotick engine on ESP32...");

	robotick::Model model;
	// create_threaded_model(model);
	create_simple_model(model);

	ESP_LOGI("Robotick", "Loading Robotick model...");

	robotick::Engine engine;
	engine.load(model);

	ESP_LOGI("Robotick", "Running Robotick engine...");

	robotick::AtomicFlag stop_after_next_tick_flag{false};
	engine.run(stop_after_next_tick_flag);
	// ^- on MCU g_stop_flag is deliberately never cleared — engine runs forever unless rebooted or halted
}
