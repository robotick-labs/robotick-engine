#include "robotick/core/WorkloadGroup.hpp"
#include "robotick/core/IWorkload.hpp"
#include "robotick/core/core.hpp"

#include <memory>
#include <iostream>

// Dummy workload for testing
class DemoWorkload : public IWorkload {
public:
    void tick(const InputBlock& in, OutputBlock& out) override {
        out.result = in.x + in.y;
        out.state += 1.0;
    }
};

int main() {
    robotick::WorkloadGroup group;
    group.add(std::make_shared<DemoWorkload>());

    robotick::run_ticker(group, 10); // 10Hz for now
    return 0;
}
