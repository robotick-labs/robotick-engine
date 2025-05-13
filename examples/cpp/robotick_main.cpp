#include "robotick/framework/WorkloadGroup.hpp"
#include "robotick/framework/IWorkload.hpp"
#include "robotick/framework/Engine.hpp"

#include <memory>
#include <iostream>

// Dummy workload for testing
class DemoWorkload : public robotick::IWorkload {
public:
    void tick(const robotick::InputBlock& in, robotick::OutputBlock& out) override {
        out.result = in.x + in.y;
        out.state += 1.0;
    }
};

int main() {
    robotick::WorkloadGroup group;
    group.add(std::make_shared<DemoWorkload>());

    robotick::Engine engine;
    engine.start(group, 10); // 10Hz for now

    return 0;
}
