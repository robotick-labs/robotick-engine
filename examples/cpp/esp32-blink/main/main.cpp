// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/platform/Threading.h"

#include "esp_log.h"
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
		ROBOTICK_KEEP_WORKLOAD(ConsoleTelemetryWorkload)
		ROBOTICK_KEEP_WORKLOAD(HeartbeatDisplayWorkload)
		ROBOTICK_KEEP_WORKLOAD(TimingDiagnosticsWorkload)
		ROBOTICK_KEEP_WORKLOAD(SyncedGroupWorkload)
	}

} // namespace robotick

void create_threaded_model(robotick::Model& model)
{
	auto console = model.add("ConsoleTelemetryWorkload", "console", 2.0);
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "timing_diag");

	std::vector<robotick::WorkloadHandle_v1> children = {console, test_state_1};

	auto root = model.add("SyncedGroupWorkload", "root", children, 100.0);
	model.set_root_workload(root);
}

void create_non_threaded_model(robotick::Model& model)
{
	auto console = model.add("ConsoleTelemetryWorkload", "console", 2.0);
	auto test_state_1 = model.add("TimingDiagnosticsWorkload", "timing_diag");

	std::vector<robotick::WorkloadHandle_v1> children = {console, test_state_1};

	auto root = model.add("SequencedGroupWorkload", "root", children, 100.0);
	model.set_root_workload(root);
}

void create_simple_model(robotick::Model& model)
{
	auto root = model.add("HeartbeatDisplayWorkload", "heartbeat", 30.0);
	model.set_root_workload(root);
}

void run_engine_on_core1(void* param)
{
	ESP_LOGI(ENGINE_TASK_NAME, "Running on CPU%d", xPortGetCoreID());

	esp_task_wdt_add(nullptr); // register current task with watchdog

	auto* engine = static_cast<robotick::Engine*>(param);

	robotick::Model model;
	// create_threaded_model(model);
	create_simple_model(model);

	ESP_LOGI("Robotick", "Loading Robotick model...");
	engine->load(model); // Ensures memory locality on Core 1

	ESP_LOGI("Robotick", "Starting tick loop...");
	robotick::AtomicFlag dummy_flag{false};
	engine->run(dummy_flag);

	vTaskDelete(nullptr);
}

extern "C" void app_main(void)
{
	ESP_LOGI("main_task", "Started on CPU%d", xPortGetCoreID());

	robotick::ensure_workloads();

	static robotick::Engine engine;

	ESP_LOGI("Robotick", "Launching Robotick engine task on core 1...");

	xTaskCreatePinnedToCore(run_engine_on_core1, ENGINE_TASK_NAME, ENGINE_STACK_SIZE, &engine, ENGINE_TASK_PRIORITY, nullptr, ENGINE_CORE_ID);
}
