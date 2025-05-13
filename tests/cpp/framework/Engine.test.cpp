#include <catch2/catch_test_macros.hpp>
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadBase.h"

using namespace robotick;

class CountingWorkload : public WorkloadBase {
public:
    int tick_count = 0;

    CountingWorkload(std::string name, double hz)
        : WorkloadBase(std::move(name), hz) {}

    void tick(const InputBlock&, OutputBlock&, double) override {
        ++tick_count;
    }
};

TEST_CASE("Engine runs workloads at expected rate (approx)") {
    Model model;
    auto w = std::make_shared<CountingWorkload>("counter", 10.0);
    model.add(w);

    Engine engine;
    engine.load(model);
    engine.setup();
    engine.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    engine.stop();

    REQUIRE(w->tick_count >= 1);  // Should tick at least once
}
