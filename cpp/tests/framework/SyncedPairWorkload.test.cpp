#if 0

#include "robotick/framework/Engine.h"
#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

class TickCounter
{
public:
    int count = 0;
    double get_tiXck_rate_hz() const { return 50.0; }
    void tick(double) { ++count; }
};

static robotick::WorkloadAutoRegister<TickCounter> s_auto_register;

TEST_CASE("SyncedPairWorkload ticks both children")
{
    Model model;

    auto p = model.add("TickCounter", "p", {});
    auto s = model.add("TickCounter", "s", {});
    auto pair = model.add("SyncedPairWorkload", "pair", {{"primary", p}, {"secondary", s}});

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

#endif  // #if 0