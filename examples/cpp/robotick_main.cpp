#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"

#include <any>
#include <atomic>
#include <csignal>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

using namespace robotick;

std::atomic<bool> g_exit_requested = false;

void signal_handler(int)
{
	g_exit_requested = true;
}

void populate_model(Model& model)
{
	model.add("TimingDiagnosticsWorkload", "timing_diagnostics", 1000.0, {{"report_every", 1000}});

	if (true)
	{
		return;
	}

	// === Add top-level standalone workloads ===
	model.add("PythonWorkload", "comms", 30.0,
			  {{"script_name", "robotick.workloads.core.telemetry.mqtt_update"}, {"class_name", "MqttUpdate"}});

	model.add("PythonWorkload", "console", 2.0, {{"script_name", "console_update"}, {"class_name", "ConsoleUpdate"}});

	// === Subcomponents for synced pair ===
	auto brickpi = model.add("PythonWorkload", "brickpi3_interface", 100.0,
							 {{"script_name", "brickpi"}, {"class_name", "BrickPi3Interface"}});

	auto remote = model.add("PythonWorkload", "remote_control", 30.0,
							{{"script_name", "remote"}, {"class_name", "RemoteControlInterface"}});

	auto deadzone = model.add("PythonWorkload", "deadzone_transformer", 30.0,
							  {{"script_name", "transform"}, {"class_name", "DeadZoneScaleAndSplitTransformer"}});

	auto mixer = model.add("PythonWorkload", "steering_mixer", 30.0,
						   {{"script_name", "steering"}, {"class_name", "SteeringMixerTransformer"}});

	// === Sequence and Pair ===
	auto sequence = model.add("SequenceWorkload", "control_sequence", 0.0,
							  {{"children", std::vector<WorkloadHandle>{remote, deadzone, mixer}}});

	model.add("SyncedPairWorkload", "control_pair", 100.0, {{"primary", brickpi}, {"secondary", sequence}});
}

int main()
{
	std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);

	Model model; // the model is our "seed data"
	populate_model(model);

	Engine engine;
	engine.load(model); // instances our model and allows multithreaded-load/config for each
	engine.start();

	while (!g_exit_requested)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::cout << "\nShutting down...\n";
	engine.stop();
	return 0;
}
