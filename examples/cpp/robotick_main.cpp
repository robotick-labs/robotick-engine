#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/PythonWorkload.h"
#include "robotick/framework/SequenceWorkload.h"
#include "robotick/framework/SyncedPairWorkload.h"

#include <csignal>
#include <atomic>
#include <iostream>

using namespace robotick;

Model build_model() {
    Model model;

    // Standalone workloads
    model.add(std::make_shared<PythonWorkload>("comms", "mqtt_update", "MqttUpdate", 30.0));
    model.add(std::make_shared<PythonWorkload>("console", "console_update", "ConsoleUpdate", 2.0));

    // Synced pair
    auto brickpi = std::make_shared<PythonWorkload>("brickpi3_interface", "brickpi", "BrickPi3Interface", 100.0);

    auto remote = std::make_shared<PythonWorkload>("remote_control", "remote", "RemoteControlInterface", 30.0);
    auto deadzone = std::make_shared<PythonWorkload>("deadzone_transformer", "transform", "DeadZoneScaleAndSplitTransformer", 30.0);
    auto mixer = std::make_shared<PythonWorkload>("steering_mixer", "steering", "SteeringMixerTransformer", 30.0);

    auto sequence = std::make_shared<SequenceWorkload>("control_sequence");
    sequence->add(remote);
    sequence->add(deadzone);
    sequence->add(mixer);

    auto pair = std::make_shared<SyncedPairWorkload>("control_pair", brickpi, sequence);

    model.add(pair);

    return model;
}

std::atomic<bool> g_exit_requested = false;

void signal_handler(int) {
    g_exit_requested = true;
}

int main() {
    std::signal(SIGINT, signal_handler);  // handle Ctrl+C
    std::signal(SIGTERM, signal_handler); // handle kill signal

    Model model = build_model();

    Engine engine;
    engine.load(model); // multithreaded loading
    engine.setup();     // setup (single-threaded)
    engine.start();     // tick loop

    while (!g_exit_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\nShutting down...\n";
    engine.stop();

    return 0;
}
