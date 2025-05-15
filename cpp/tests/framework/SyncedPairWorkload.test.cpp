#if 0

#include <catch2/catch_test_macros.hpp>
#include "robotick/framework/Model.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/WorkloadMacros.h"

using namespace robotick;

class TickCounter
{
public:
    int count = 0;
    double get_tick_rate_hz() const { return 50.0; }
    void tick(double) { ++count; }
};

ROBOTICK_REGISTER_WORKLOAD(TickCounter, robotick::EmptyConfig, robotick::EmptyInputs, robotick::EmptyOutputs);

TEST_CASE("SyncedPairWorkload ticks both children")
{
    Model model;

    auto p = model.add_by_type("TickCounter", "p", {});
    auto s = model.add_by_type("TickCounter", "s", {});
    auto pair = model.add_by_type("SyncedPairWorkload", "pair", {{"primary", p}, {"secondary", s}});

    model.finalise();

    auto *pInst = model.get<TickCounter>(p);
    auto *sInst = model.get<TickCounter>(s);

    Engine engine;
    engine.load(model);
    engine.setup();
    engine.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine.stop();

    REQUIRE(pInst->count >= 1);
    REQUIRE(sInst->count >= 1);
}

#endif // #if 0