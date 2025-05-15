#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"

#include <csignal>
#include <atomic>
#include <iostream>
#include <thread>
#include <map>
#include <any>
#include <vector>

using namespace robotick;

std::atomic<bool> g_exit_requested = false;

void signal_handler(int)
{
    g_exit_requested = true;
}

#if 1
void populate_model(Model &model)
{
    model.add_by_type("PythonWorkload", "py",
                      {{"script_name", "robotick.workloads.optional.test.hello_workload"},
                       {"class_name", "HelloWorkload"},
                       {"tick_rate_hz", 5.0}});
}
#else
void populate_model(Model &model)
{
    // === Add top-level standalone workloads ===
    model.add_by_type("PythonWorkload", "comms", {{"script_name", "mqtt_update"}, {"class_name", "MqttUpdate"}, {"tick_rate_hz", 30.0}});

    model.add_by_type("PythonWorkload", "console", {{"script_name", "console_update"}, {"class_name", "ConsoleUpdate"}, {"tick_rate_hz", 2.0}});

    // === Subcomponents for synced pair ===
    auto brickpi = model.add_by_type("PythonWorkload", "brickpi3_interface", {{"script_name", "brickpi"}, {"class_name", "BrickPi3Interface"}, {"tick_rate_hz", 100.0}});

    auto remote = model.add_by_type("PythonWorkload", "remote_control", {{"script_name", "remote"}, {"class_name", "RemoteControlInterface"}, {"tick_rate_hz", 30.0}});

    auto deadzone = model.add_by_type("PythonWorkload", "deadzone_transformer", {{"script_name", "transform"}, {"class_name", "DeadZoneScaleAndSplitTransformer"}, {"tick_rate_hz", 30.0}});

    auto mixer = model.add_by_type("PythonWorkload", "steering_mixer", {{"script_name", "steering"}, {"class_name", "SteeringMixerTransformer"}, {"tick_rate_hz", 30.0}});

    // === Sequence and Pair ===
    auto sequence = model.add_by_type("SequenceWorkload", "control_sequence", {{"children", std::vector<WorkloadHandle>{remote, deadzone, mixer}}});

    auto pair = model.add_by_type("SyncedPairWorkload", "control_pair", {{"primary", brickpi}, {"secondary", sequence}});
}
#endif // #if 1

int main()
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    Model model;
    populate_model(model);
    model.finalise(); // finalise memory - creating actual workloads in a single large buffer

    Engine engine;
    engine.load(model);
    engine.setup();
    engine.start();

    while (!g_exit_requested)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\nShutting down...\n";
    engine.stop();
    return 0;
}
