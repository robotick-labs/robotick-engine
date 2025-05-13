#include <catch2/catch_test_macros.hpp>
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/SyncedPairWorkload.h"
#include "robotick/framework/WorkloadBase.h"

using namespace robotick;

class FastSecondaryWorkload : public WorkloadBase {
public:
    int tick_count = 0;

    FastSecondaryWorkload(std::string name)
        : WorkloadBase(std::move(name), 0) {}

    void tick(const InputBlock&, OutputBlock&, double) override {
        ++tick_count;
    }
};

class PrimaryTriggerWorkload : public WorkloadBase {
public:
    int trigger_count = 0;

    PrimaryTriggerWorkload(std::string name)
        : WorkloadBase(std::move(name), 50.0) {}

    void tick(const InputBlock&, OutputBlock&, double) override {
        ++trigger_count;
    }
};

TEST_CASE("SyncedPairWorkload runs both sides and joins cleanly") {
    auto primary = std::make_shared<PrimaryTriggerWorkload>("primary");
    auto secondary = std::make_shared<FastSecondaryWorkload>("secondary");

    Engine engine;

    Model model;
    model.add(std::make_shared<SyncedPairWorkload>("testpair", primary, secondary));

    engine.load(model);
    engine.setup();
    engine.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine.stop();

    REQUIRE(primary->trigger_count >= 1);
    REQUIRE(secondary->tick_count >= 1);
    REQUIRE(primary->trigger_count == secondary->tick_count);
}
