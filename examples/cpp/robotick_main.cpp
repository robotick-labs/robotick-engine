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

std::atomic<bool> g_stop_flag = false;

void signal_handler(int)
{
	std::cout << "\nShutting down...\n";
	g_stop_flag = true;
}

void populate_model_groups(Model& model)
{
	auto sequence_tick_1 = model.add("TimingDiagnosticsWorkload", "sequence_tick_1");
	auto sequence_tick_2 = model.add("TimingDiagnosticsWorkload", "sequence_tick_2");
	auto sequence_tick_3 = model.add("TimingDiagnosticsWorkload", "sequence_tick_3");

	std::vector<WorkloadHandle> sequence_children = {sequence_tick_1, sequence_tick_2, sequence_tick_3};
	auto sequenced_group = model.add("SequencedGroupWorkload", "synced_group", sequence_children, 1000.0);
	// note - a SequencedGroupWorkload should just get through its child-ticks as quickly as they allow, in sequence -
	// it's done when its done (though see note below)

	auto synced_child_single = model.add("TimingDiagnosticsWorkload", "synced_child_single");

	std::vector<WorkloadHandle> synced_children = {synced_child_single, sequenced_group};
	auto synced_group = model.add("SyncedGroupWorkload", "synced_group", synced_children, 100.0);

	auto slow_ticker = model.add("TimingDiagnosticsWorkload", "slow_ticker_10Hz", 10.0);
	auto slowest_ticker = model.add("TimingDiagnosticsWorkload", "slow_ticker_1Hz", 1.0);

	std::vector<WorkloadHandle> resynced_children = {synced_group, slow_ticker, slowest_ticker};
	/*auto resynced_group =*/model.add("SyncedGroupWorkload", "resynced_group", resynced_children, 100.0);
}

void populate_model_brickpi(Model&)
{
#if 0
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
	std::vector<WorkloadHandle> sequence_children = {remote, deadzone, mixer};
	auto sequence = model.add("SequenceWorkload", "control_sequence", 0.0, sequence_children, {});

	model.add("SyncedPairWorkload", "control_pair", 100.0, {{"primary", brickpi}, {"secondary", sequence}});
#endif // #if 0
}

void populate_model(Model& model)
{
	populate_model_groups(model);
}

int main()
{
	std::signal(SIGINT, signal_handler);
	std::signal(SIGTERM, signal_handler);

	Model model; // the model is our "seed data"
	populate_model(model);

	Engine engine;
	engine.load(model); // instances our model and allows multithreaded-load/config for each

	engine.run(g_stop_flag); // engine runs on this thread, until requested to stop

	return 0;
}
