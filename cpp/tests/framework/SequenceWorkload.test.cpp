#if 0
#include "robotick/framework/Engine.h"
#include "robotick/framework/FieldMacros.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/WorkloadRegistry.h"
#include <catch2/catch_test_macros.hpp>

using namespace robotick;

class TickOrder
{
public:
    TickOrder() {}
    int &ref;
    int expect;
    double get_tick_rate_hz() const { return 0; }
    void tick(double)
    {
        REQUIRE(ref == expect - 1);
        ref = expect;
    }
};

ROBOTICK_REGISTER_WORKLOAD(TickOrder, robotick::EmptyConfig, robotick::EmptyInputs, robotick::EmptyOutputs);

TEST_CASE("SequenceWorkload ticks children in order")
{
    Model model;
    int state = 0;

    auto a = model.add_by_type("TickOrder", "a", {{"ref", &state}, {"expect", 1}});
    auto b = model.add_by_type("TickOrder", "b", {{"ref", &state}, {"expect", 2}});
    auto c = model.add_by_type("TickOrder", "c", {{"ref", &state}, {"expect", 3}});

    auto s = model.add_by_type("SequenceWorkload", "seq", {{"children", std::vector<WorkloadHandle>{a, b, c}}});
    model.finalise();

    auto *ptr = model.factory().get_raw_ptr(s);
    auto *entry = get_workload_registry_entry(model.factory().get_type_name(s));
    REQUIRE(entry);
    entry->setup(ptr);
    entry->tick(ptr, 0.01);

    REQUIRE(state == 3);
}
#endif // #if 0